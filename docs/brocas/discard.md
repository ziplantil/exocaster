
# Discard broca

Type: `discard`

Discards the bitstream passed to it.

## Configuration

Either `null` or a JSON object. If an object, the fields are:

* `log` (optional): A boolean value. If `true`, dumps information about each
  discarded packet into the standard error stream. (Default: `false`)
* `wait` (optional): A boolean value. If `true`, issues waits or sleeps 
  according to the number of frames received to simulate 'real-time'
  streaming. (Default: `false`)

If `null`, default values are used.
