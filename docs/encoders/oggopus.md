
# Ogg Opus encoder

Type: `oggopus`

Encodes PCM data into Ogg Vorbis (Ogg container, Opus codec).

The Opus codec uses a constant input sample rate of 48000 Hz. Any other
input sample rates have to be resampled.

## Configuration

A JSON object with the following fields:

* `bitrate` (optional): The bitrate to use, in bits per second
  (default: automatic, determined by the encoder).
