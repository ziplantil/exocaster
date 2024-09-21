
# Exocaster messages

## Commands

Exocaster is configured with a single _shell_, or command queue. It reads
commands whenever there is a free worker available. All commands are expressed
as JSON objects, with at least the following two fields:

* `cmd`: Command name. This must be a string, and is mapped through the
  `commands` field in Exocaster configuration to determine the decoder to use.
* `param`: The arguments. These are passed to the decoder.

A command which lacks either of these two fields is ignored.

When working with line-oriented queues, the JSON must be formatted on a
single line of text, with one JSON command per line.

The command name `quit` is special. It will ignore the arguments and cause
Exocaster to quit once all preceding commands have been completed and
encoder and broca buffers have been flushed.

**IMPORTANT NOTE**: Exocaster's security model assumes that all commands can
be trusted. Do not ever make Exocaster accept commands from sources that are
not 100% trustworthy!

## Events

Exocaster may publish events to publishers, if configured to do so. Each event
is likewise a JSON object, containing at least the field `type`.

Currently there is only one kind of event: command acknowledgment events.
These have `"type": "acknowledge"`.

When working with line-oriented queues, each event JSON is formatted on a
single line of text, with one JSON event per line.

### Acknowledgment events

Besides `type`, these events have the following fields:

* `source`: One of the following:
  * `decoder`, if the command was acknowledged by the decoder
  * `encoder`, if the command was acknowledged by an encoder,
  * `broca`, if the command was acknowledged by a broca.
* `index`: The index of the encoder or broca. `0` represents the first encoder
  or broca in Exocaster's configuration, `1` represents the second, and so on.
  This value is not present if `source` is `decoder`.
* `command`: The command that was acknowledged. This is a copy of the
  JSON object that was passed to Exocaster as a command.
