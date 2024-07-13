
# libavcodec decoder

Type: `lavc`

Decodes audio files using the libav stack: libavformat, libavcodec,
libavfilter, libavutil, libswscale and libswresample.

## Argument

Either a string or a JSON object. If an object, the fields are:

* `file` (required): The path to the file to decode, as a string.

If a string, it is taken as the `file` parameter.

## Configuration

Either `null` or a JSON object. If an object, the fields are:

* `applyReplayGain` (optional): A boolean value. If `true`, the ReplayGain
  track gain and peak are used to apply ReplayGain to the decoded audio
  stream. Note that ReplayGain tags are always stripped prior to encoding.
  (Default: `false`)
* `replayGainPreamp` (optional): A preamp in dB as a number. (Default: 0.0)
* `replayGainAntipeak` (optional): A boolean value. Whether to apply clipping
  prevention using the ReplayGain peak value. (Default: `true`)
* `r128Fix` (optional): A boolean value. If `true`, codecs that use R128
  gain (e.g. Opus) are automatically adjusted when applying ReplayGain so
  that the final amplitude is equivalent. If running a hypothetical future
  version of the libav stack that correctly supports R128_ tags, this
  will no longer be applicable. (Default: `false`)
* `normalizeVorbisComment` (optional): A boolean value. If `true`, normalizes
  the keys of metadata fields (Vorbis comment fields) to use the conventional
  names (e.g. track number is `TRACKNUMBER`, not `track`). libavformat changes
  some of the names around to its own standard, which may not
  be compatible. (Default: `true`)
* `metadataBlockPicture` (optional): A boolean value. If `true`, tries to
  decode the album art from a music file and encode it into the stream as
  a `METADATA_BLOCK_PICTURE` Vorbis comment, if supported by the encoder
  and broca. Only one picture will be encoded, and it will be identified
  as the front cover. This feature is experimental. (Default: `false`)
* `metadataBlockPictureMaxSize` (optional): If the width or height of the
  embedded `METADATA_BLOCK_PICTURE` image would be larger in pixels
  than this integer, it will be resized down. (Default: 256)

If `null`, default values are used.
