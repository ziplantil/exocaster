
# Test tone decoder

Type: `testcard`

Generates a sine wave, by default at 1000 Hz.

## Argument

The duration of the tone to generate in seconds, as a positive number.

## Configuration

Either `null` or a JSON object. If an object, the fields are:

* `amplitude` (optional): The amplitude as a positive number between 0 and 1.
  (default: `0.5`)
* `frequency` (optional): The frequency of the tone in Hz. (default: `1000`)

If `null`, default values are used.
