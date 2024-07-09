/***
exocaster -- audio streaming helper
broca/portaudio/playback.cc -- broca for playback through portaudio

MIT License

Copyright (c) 2024 ziplantil

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

***/

#include "broca/portaudio/playback.hh"
#include "log.hh"
#include "pcmtypes.hh"
#include "server.hh"
#include "util.hh"

extern "C" {
#include <portaudio.h>
}

namespace exo {

void PortAudioGlobal::init() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        EXO_LOG("PortAudio failed to initialize (%d): %s", err,
                Pa_GetErrorText(err));
        throw std::runtime_error("Pa_Initialize failed");
    }
}

void PortAudioGlobal::quit() {
    PaError err = Pa_Terminate();
    if (err != paNoError)
        EXO_LOG("Pa_Terminate failed: %s", Pa_GetErrorText(err));
}

extern "C" {
static int streamCallback(const void* input, void* output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags, void* userData) {
    // forward to PortAudioBroca through userData pointer
    return reinterpret_cast<PortAudioBroca*>(userData)->streamCallback(
        input, output, frameCount, timeInfo, statusFlags);
}
}

/** Returns the smallest power of two greater than or equal to the passed
    unsigned integral value. */
template <std::unsigned_integral T> static T roundUpToTwo_(T value) {
    --value;
    for (unsigned shiftCount = 1; shiftCount < sizeof(T) * CHAR_BIT;
         shiftCount <<= 1)
        value |= value >> shiftCount;
    return value + 1;
}

PortAudioStream::PortAudioStream(PaStream* p) : PointerSlot(p) {
    if (!has())
        throw std::bad_alloc();
}

PortAudioStream::~PortAudioStream() noexcept {
    if (has())
        Pa_CloseStream(release());
}

PortAudioBroca::PortAudioBroca(const exo::ConfigObject& config,
                               std::shared_ptr<exo::PacketRingBuffer> source,
                               const exo::StreamFormat& streamFormat,
                               unsigned long frameRate)
    : BaseBroca(source, frameRate), stream_(nullptr) {
    PaError err = paNoError;
    PaStreamParameters streamParameters;

    if (!std::holds_alternative<exo::PcmFormat>(streamFormat))
        throw std::runtime_error("PortAudio broca needs PCM format");
    const auto& pcmFormat = std::get<exo::PcmFormat>(streamFormat);

    bytesPerFrame_ = pcmFormat.bytesPerFrame();

    streamParameters.device = Pa_GetDefaultOutputDevice();
    if (streamParameters.device == paNoDevice) {
        EXO_LOG("PortAudio has no default output device");
        throw std::runtime_error("PortAudio has no default output device");
    }

    streamParameters.channelCount = exo::channelCount(pcmFormat.channels);
    switch (pcmFormat.sample) {
    case exo::PcmSampleFormat::S8:
        streamParameters.sampleFormat = paInt8;
        break;
    case exo::PcmSampleFormat::U8:
        streamParameters.sampleFormat = paUInt8;
        break;
    case exo::PcmSampleFormat::S16:
        streamParameters.sampleFormat = paInt16;
        break;
    case exo::PcmSampleFormat::F32:
        streamParameters.sampleFormat = paFloat32;
        break;
    default:
        EXO_UNREACHABLE;
        throw std::runtime_error("unsupported sample format for "
                                 "PortAudio broca");
    }

    streamParameters.suggestedLatency =
        Pa_GetDeviceInfo(streamParameters.device)->defaultHighOutputLatency;
    streamParameters.hostApiSpecificStreamInfo = NULL;

    PaStream* paStream;
    err = Pa_OpenStream(&paStream, NULL, &streamParameters, pcmFormat.rate,
                        exo::roundUpToTwo_(pcmFormat.rate / 10), paClipOff,
                        &exo::streamCallback, this);
    if (err != paNoError) {
        EXO_LOG("PortAudio failed to open stream (%d): %s", err,
                Pa_GetErrorText(err));
        throw std::runtime_error("Pa_OpenStream failed");
    }
    stream_.reset(paStream);
}

void PortAudioBroca::runImpl() {
    PaError err = paNoError;

    auto stream = stream_.get();
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        EXO_LOG("PortAudio failed to start stream (%d): %s", err,
                Pa_GetErrorText(err));
        return;
    }

    while (EXO_LIKELY((err = Pa_IsStreamActive(stream)) == 1))
        Pa_Sleep(500);

    Pa_Sleep(500);
    Pa_StopStream(stream);
}

int PortAudioBroca::streamCallback(const void* input, void* output,
                                   unsigned long frameCount,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags) {
    auto destination = reinterpret_cast<unsigned char*>(output);
    std::memset(output, 0, frameCount * bytesPerFrame_);
    std::size_t n = source_->readDirectFull(packet_, destination,
                                            frameCount * bytesPerFrame_);
    if (EXO_UNLIKELY(!n))
        return paComplete;
    if (EXO_UNLIKELY(!exo::shouldRun()))
        return paComplete;
    return paContinue;
}

} // namespace exo
