
# Resampling

Some encoders, such as Opus, rely on the sample rate being a specific value
(in this case, 48000 Hz). To make it easier to use these encoders, Exocaster
will carry out resampling when necessary.

Exocaster can use the following libraries to implement the resampler:
* `soxr` (soxr: SoX resampler)
* `libsamplerate` (libsamplerate)
* `libswresample` (libswresample)

None of these currently look for any configurations.

If none of these are available, resampling will not be supported,
which will cause an error if the encoder requires one due to how
it or the PCM buffer is configured.
