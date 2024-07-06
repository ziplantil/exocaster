
# File broca

Type: `file`

Saves an encoded bitstream into a file.

## Configuration

Either a JSON string or object. If an object, the fields are:

* `file` (required): The path of the file to write to, as a string.
* `append` (required): A boolean value. If `true`, the bytes are appended to
  an existing file. If `false`, the file is emptied every time the broca
  is started (if applicable, does not apply to e.g. stdout).

If a string, it is taken as the `file` parameter.
