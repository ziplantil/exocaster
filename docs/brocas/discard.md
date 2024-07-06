
# Discard broca

Type: `discard`

Discards the bitstream passed to it.

## Configuration

Either `null` or a JSON object. If an object, the fields are:

* `log` (optional): A boolean value. If `true`, dumps information about each
  discarded packet into the standard error stream. The default is `false`.

If `null`, default values are used.
