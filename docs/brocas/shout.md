
# libshout broca

Type: `shout`

Broadcasts encoded audio through an Icecast server.

## Configuration

A JSON object with the following fields:

* `protocol` (required): A string, one of the following:
    * `http`: HTTP
    * `icy`: ICY
    * `roaraudio`: RoarAudio
* `host`: The host to connect to, as a string.
* `port`: The port to connect to, as an integer.
* `user`: The user to connect as, as a string.
* `password`: The password for authentication, as a string.
* `mount`: The mountpoint to connect with, as a string.

## Notes

The supported codecs are MP3 and Ogg Vorbis. Ogg FLAC support is experimental
and depends on a modified version of libshout that can accommodate
OGG FLAC streams. It must be enabled when compiling Exocaster with the
`EXO_SHOUT_ALLOW_OGGFLAC` define.
