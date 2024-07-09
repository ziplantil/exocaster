
# Ogg FLAC encoder

Type: `oggflac`

Encodes PCM data into Ogg FLAC (Ogg container, FLAC codec).

## Configuration

A JSON object with the following fields:

* `level` (optional): The FLAC level, as an integer between (inclusive)
  0 and 8 (default: 5)
* `float24` (optional): A boolean value. If `true`, when the PCM buffer is
  set to `f32`, FLAC will be encoded using 24-bit rather than 16-bit
  sample data. (default: `false`)
