
# Ogg Opus encoder

Type: `oggopus`

Encodes PCM data into Ogg Vorbis (Ogg container, Opus codec).

The Opus codec uses a constant input sample rate of 48000 Hz. Any other
input sample rates have to be resampled.

## Configuration

A JSON object with the following fields:

* `bitrate` (optional): The bitrate to use, in bits per second. A negative
  value will cause the encoder to use as much bitrate as it wants
  (default: automatic, determined by the encoder).
* `complexity` (optional): The computation complexity, as an integer between
  0 and 10; higher values represent higher complexity (default: 10).
