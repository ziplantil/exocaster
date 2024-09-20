
# libshout broca

Type: `shout`

Broadcasts encoded audio through an Icecast server.

## Configuration

A JSON object with the following fields:

* `protocol` (required): A string, one of the following:
    * `http`: HTTP
    * `icy`: ICY
    * `roaraudio`: RoarAudio
* `host` (required): The host to connect to, as a string.
* `port` (required): The port to connect to, as an integer.
* `user` (required): The user to connect as, as a string.
* `password` (required): The password for authentication, as a string.
* `mount` (required): The mountpoint to connect with, as a string.
* `name` (optional): The stream name, as a string. Stream metadata.
* `genre` (optional): The stream genre, as a string. Stream metadata.
* `description` (optional): The stream description, as a string.
  Stream metadata.
* `url` (optional): The stream URL, as a string. Stream metadata.
* `selfsync` (optional): A boolean value. If `true`, Exocaster uses its own
  sync, rather than relying on that provided by libshout. (Default: `false`)
* `selfsyncthreshold` (optional): A numeric value. The number of seconds in
  flight after which to sleep before sending more. If the output stream is
  skipping, try decreasing this value; if it is cutting out, try increasing
  this value. (Default: `0.1`)

## Notes

The supported codecs are MP3 and Ogg Vorbis. Ogg FLAC support is experimental
and depends on a modified version of libshout that can accommodate
OGG FLAC streams. It must be enabled when compiling Exocaster with the
`EXO_SHOUT_ALLOW_OGGFLAC` define. It is possible that no changes to libshout
are needed if `selfsync` is enabled.

The `shout` broca supports out-of-band metadata used by e.g. the `mp3` encoder.
