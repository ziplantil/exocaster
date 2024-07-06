
# Ogg Vorbis encoder

Type: `oggvorbis`

Encodes PCM data into Ogg Vorbis (Ogg container, Vorbis codec).

## Configuration

A JSON object with the following fields:

* `bitrate` (optional): The bitrate to use, in bits per second
  (default: 128000, equal to 128 kbps).
* `minbitrate` (optional): The minimum bitrate in bits per second.
  Passed straight to the Vorbis analyzer.
  If not specified, the minimum bitrate is not limited.
* `maxbitrate` (optional): The maximum bitrate in bits per second.
  Passed straight to the Vorbis analyzer.
  If not specified, the maximum bitrate is not limited.
