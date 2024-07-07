
# Exocaster design

Exocaster includes multiple kinds of components. The general flow of audio data
is: decoder -> (buffer) -> encoder -> (buffer) -> broca.

One decoder, specified by a command, can feed data to multiple encoders,
which in turn feed data to one or more brocas. Each broca is associated with
precisely one encoder.

## Read and write queues

These are used to read or write messages to external sources.
They are used to implement the _shell_, i.e. command queue, which Exocaster
uses to read commands for what to do next, and the _publishers_, which publish
events.

See `messages.md` for more information.

## Decoders

A decoder is responsible for producing data for the encoders.
They take the parameters given to them in the command and use it to emit
both metadata and PCM data.

Decoders can receive configurations from Exocaster's main configuration.

## Encoders

Encoders take PCM data and encode it into a bitstream of encoded
audio data to be broadcasted.

Encoders can receive configurations from Exocaster's main configuration.

## Brocas

Brocas, or broadcasters, take encoded data and broadcast it as configured.

Brocas can receive configurations from Exocaster's main configuration.

## Ring buffers

All intermediate buffers are ring buffers, which, when full, may block writes.
Decoder writes to buffers are non-blocking, but timed according to the number
of samples provided. If encoders take too long, blocks of PCM data are skipped.
Encoder writes to buffers are always blocking, since Exocaster does not know
anything about the format and thus cannot safely skip blocks.

## Metadata

Metadata is expressed internally as if it were a pair of keys and values
like a Vorbis comment. The PCM buffers from the decoder to the encoder encode
metadata changes and ensure that they are passed to the encoder
at right positions.
