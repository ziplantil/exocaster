/***
exocaster -- audio streaming helper
pcmbuffer.cc -- PCM and metadata buffer

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

#include <thread>
#include <utility>

#include "log.hh"
#include "pcmbuffer.hh"
#include "server.hh"

namespace exo {

static std::shared_ptr<exo::Metadata> empty_;

std::shared_ptr<exo::Metadata> PcmBuffer::readMetadata() {
    std::unique_lock lock(mutex_);
    // some of this song still left, or no more metadata changes queued
    if (songHead_ == songTail_ || pcmLeft_) return std::move(empty_);
    // read next metadata and return it
    auto index = songTail_;
    songTail_ = (songTail_ + 1) % songChanges_.size();
    pcmLeft_ = std::exchange(songChanges_[index].pcmBytes, 0);
    auto metadata = std::exchange(songChanges_[index].metadata, nullptr);
    publisher_->acknowledgeEncoderCommand(subindex_,
                                songChanges_[index].command);
    lock.unlock();
    hasPcm_.notify_all();
    return metadata;
}

std::size_t PcmBuffer::readPcm(std::span<exo::byte> destination) {
    std::size_t canRead;
    std::unique_lock lock(mutex_);
    if (!pcmLeft_) {
        hasPcm_.wait(lock, [this]() {
            return pcmLeft_ > 0 || songHead_ != songTail_ || closed_;
        });
        if (pcmLeft_ == 0 && songHead_ == songTail_ && closed_) return 0;
    }
    canRead = std::min(pcmLeft_, destination.size());
    canRead -= canRead % pcmFormat_.bytesPerFrame();
    if (!canRead) return canRead;
    pcmLeft_ -= canRead;
    lock.unlock();
    pcm_.readFull(destination.begin(), canRead);
    return canRead;
}

void PcmBuffer::writeMetadata(std::shared_ptr<exo::ConfigObject> command,
                              std::shared_ptr<exo::Metadata> metadata) {
    if (closed_) return;
    std::unique_lock lock(mutex_);
    if (metadataFull_()) {
        // metadata queue is full
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (!lock.try_lock_for(std::chrono::milliseconds(1000)))
            return;
        // metadata queue is still full, bail out
        if (metadataFull_())
            return;
    }
    // write song change
    auto index = songHead_;
    songHead_ = (songHead_ + 1) % songChanges_.size();
    songChanges_[index].command = command;
    songChanges_[index].metadata = metadata;
    songChanges_[index].pcmBytes = 0;
    lock.unlock();
    hasPcm_.notify_all();
}

void PcmBuffer::writePcm(std::span<const exo::byte> data) {
    if (closed_) return;
    // leave some time for the buffer to be emptied
    double timeout = std::max(waitAbs_,
                    waitRel_ * pcmFormat_.estimateDuration(data.size()));
    std::size_t written = pcm_.writeTimed(data.begin(), data.size(), timeout);
    if (written < data.size()) {
        if (exo::shouldRun())
            EXO_LOG("buffer overrun: %zu < %zu, waited %0.5f s",
                    written, data.size(), timeout);
    }
    if (written) {
        std::unique_lock lock(mutex_);
        if (songHead_ != songTail_) {
            // song change queued, add PCMs to most recent song change
            std::size_t index;
            if (songHead_ == 0)
                index = songChanges_.size() - 1;
            else
                index = songHead_ - 1;
            songChanges_[index].pcmBytes += written;
        } else {
            // no song change queued, add to the current counter
            pcmLeft_ += written;
        }
        lock.unlock();
        hasPcm_.notify_all();
    }
}

void PcmBuffer::close() noexcept {
    closed_ = true;
    pcm_.close();
    hasPcm_.notify_all();
}

bool PcmBuffer::closed() const noexcept {
    return closed_;
}

std::shared_ptr<exo::PcmBuffer> PcmSplitter::addBuffer(
                const exo::PcmBufferConfig& config) {
    auto ptr = std::make_shared<exo::PcmBuffer>(
                bufferIndex_++, pcmFormat_, bufferSize_, publisher_, config);
    buffers_.push_back(ptr);
    return ptr;
}

void PcmSplitter::metadata(std::shared_ptr<exo::ConfigObject> command,
                           const exo::Metadata& metadata) {
    auto metadataPtr = std::make_shared<exo::Metadata>(metadata);
    publisher_->acknowledgeDecoderCommand(command);
    for (const auto& buffer: buffers_)
        buffer->writeMetadata(command, metadataPtr);
}

void PcmSplitter::pcm(std::span<const exo::byte> data) {
    for (const auto& buffer: buffers_)
        buffer->writePcm(data);
}

} // namespace exo
