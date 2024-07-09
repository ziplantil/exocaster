
**NOTE**: This is an experimental project. Use entirely at your own risk.

# Exocaster

Exocaster is an audio streaming program. It takes commands from a source,
uses them to decode audio files into PCM audio data and stream metadata,
encodes them into a stream, and then broadcasts it using a variety of methods.

See the `docs` directory for full documentation.

## Building

Configure `Makefile.inc` and run `make`.

## Running

Provide a configuration file (see the configuration help under `docs`)
and run the executable.

## Notes

Exocaster bears no relation to ExoPlayer. The naming is a coincidence,
in reference to earlier private streaming applications named after
layers of the atmosphere.
