
# Exocaster configuration

## Build configuration

Exocaster can optionally use a variety of external libraries. See
`Makefile.inc.example` for the selection. To see which libraries have been
linked into a build of Exocaster, use the `-v` or `--version` flag to
display version information.

## Command line

Exocaster relies on a single JSON file for its configuration. By default,
Exocaster assumes it is called `config.json` and found in the
working directory. The `-c` command line flag can be used to override
the configuration path.

## Configuration JSON

An example file is provided as `config.json.EXAMPLE`.

### shell

Provides configuration for the shell, i.e. command read queue.
Used to configure where Exocaster tries to read commands from.

This field is required, and must be a JSON object.

**IMPORTANT NOTE**: Exocaster's security model assumes that all commands can
be trusted. Do not ever make Exocaster accept commands from sources that are
not 100% trustworthy!

Fields:

* `type` (required): The type of read queue to use. See the `read-queues`
  directory under `docs` for a list of possible queues. Note that your
  Exocaster build must be compiled with support for a queue in order to use it.
* `config` (optional): The configuration to provide for the queue. See the
  documentation of each queue for more information. Defaults to JSON `null`
  if omitted.

Example:
```json
    "shell": {
        "type": "file",
        "config": {
            "file": "/dev/stdin"
        }
    }
```
specifies that the shell should be a file read queue that reads
commands from the standard input, with one command per line.

### publish

Provides any number of publisher configurations. Each publisher is
given event information (see `messages.md`) to be published.

If specified, it must be an array, with one JSON object for each configuration.
If not specified at all, it is taken as an empty array.

**IMPORTANT NOTE**: Publishers may be given information, e.g. commands passed
to Exocaster. Do not use publishers if you cannot trust where they take
their data.

Fields for every object in `publish`:

* `type` (required): The type of write queue to use. See the `write-queues`
  directory under `docs` for a list of possible queues. Note that your
  Exocaster build must be compiled with support for a queue in order to use it.
* `config` (optional): The configuration to provide for the queue. See the
  documentation of each queue for more information. Defaults to JSON `null`
  if omitted.

Example:
```json
    "publish": [
        {
            "type": "file",
            "config": {
                "file": "/dev/stdout"
            }
        }
    ]
```
specifies that there is one publisher, which uses a file write queue that
writes events to the standard output, with one command per line.

### commands

Maps command names to decoders with specific configurations.

This field is required, and must be a JSON object.

The key is the command name, and the value is a decoder configuration.
Note that the command `quit` is hardcoded and any command mapping for it
will be ignored.

A decoder configuration has the following fields:

* `type` (required): The type of decoder to use. See the `decoders` directory
  under `docs` for a list of possible decoders. Note that your Exocaster
  build must be compiled with support for a decoder in order to use it.
* `config` (optional): The configuration to provide for the decoder. See the
  documentation of each decoder for more information. Defaults to JSON `null`
  if omitted.

Example:
```json
    "commands": {
        "play": {
            "type": "lavc",
            "config": {
                "applyReplayGain": true
            }
        }
    }
```
specifies a command called `play` that uses the `lavc` decoder and enables
ReplayGain in that decoder.

### resampler

An object describing the resampler to use and its configuration

Fields (all optional):
* `type`: The type of resampler to use. See `resampling.md` for options.
  The default is to try them in the order described in the document.
* `config`: The configuration to pass to the resampler.

If omitted, default values are used.

### pcmbuffer

Provides the configuration for the internal PCM buffers, used to pass data
from the decoder to the encoders. One of these buffers is created per encoder.

This field is required, and must be a JSON object.

Fields (all optional):
* `format`: The PCM sample format to use. Must be one of the following strings:
    * `s8`: signed 8-bit integer
    * `u8`: unsigned 8-bit integer
    * `s16`: signed 16-bit integer (default)
    * `f32`: 32-bit floating point
* `samplerate`: The sample rate in Hz, as a positive integer (default: 44100)
* `channels`: The channel layout to use, as one of the following strings:
    * `mono`: mono, one channel
    * `stereo`: stereo, two channels (default)
* `duration`: The duration of the sample buffer in seconds,
  as a positive number (default: 1.0)
* `skip`: A boolean specifying whether the decoder should try to skip certain
  encoders if they take too long to accept sample data. (Default: `true`)
* `skipmargin`: A positive number. The number of seconds of margin to allow
  when pushing PCM blocks before skipping. (Default: 0.1)
* `skipfactor`: A positive number. A multiplier to apply when waiting. If the
  buffer is full, the waiting time is determined by multiplying the duration
  of the PCM block with this factor and adding it to `margin`. (default: 2.0)

Example:
```json
    "pcmbuffer": {
        "format": "s16",
        "samplerate": 44100,
        "channels": "stereo",
        "duration": 5.0
    }
```
specifies that each PCM buffer should be in CD audio quality (signed 16-bit
integer samples, 44100 Hz, stereo) and be five seconds long.

### outputs

Provides any number of encoders.

This field is required, and must be a JSON array containing at least one
encoder configuration.

Fields for every encoder configuration:

* `type` (required): The type of encoder to use. See the `encoders` directory
  under `docs` for a list of possible encoders. Note that your Exocaster
  build must be compiled with support for an encoder in order to use it.
* `buffer` (optional): The size of the buffer in bytes. These búffers contain
  encoded audio data, and one of these buffers is created for each broca
  in this encoder. The default value is `65536`.
* `config` (optional): The configuration to provide for the encoder. See the
  documentation of each encoder for more information. Defaults to JSON `null`
  if omitted.
* `broca` (required): The brocas for this encoder, as a JSON array containing
  broca configurations. Encoders without any brocas are ignored, but still
  count towards the encoder index.
* `barrier` (optional): A string. If multiple encoders share the same
  (non-empty) string, they will all share a common 'barrier'. This means that
  the encoder track changes are forcefully synchronized, such that an encoder
  may not start encoding the next track until all encoders have done so.
  This is not recommended if the encoders or any of their brocas may fail,
  as it will cause all encoders to get stuck.

Broca configurations are JSON objects with the following fields:

* `type` (required): The type of broca to use. See the `brocas` directory
  under `docs` for a list of possible brocas. Note that your Exocaster
  build must be compiled with support for a broca in order to use it.
* `config` (optional): The configuration to provide for the broca. See the
  documentation of each broca for more information. Defaults to JSON `null`
  if omitted.

Example:
```json
    "outputs": [
        {
            "type": "oggvorbis",
            "buffer": 65536,
            "config": {
                "bitrate": 192000,
                "minbitrate": 16000
            },
            "broca": [
                {
                    "type": "file",
                    "config": {
                        "file": "/tmp/out.ogg"
                    }
                }
            ]
        }
    ]
```
specifies a single `oggvorbis` encoder with a buffer size of 65536 bytes,
with a nominal bitrate of 192 kbps, a minimum bitrate of 16 kbps, and a single
broca, which stores the output into a file (not recommended for
indefinite streaming!).
