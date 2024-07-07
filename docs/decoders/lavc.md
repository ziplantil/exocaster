
# libavcodec decoder

Type: `lavc`

Decodes audio files using the libav stack: libavformat, libavcodec,
libavfilter, libavutil and libswresample.

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
* `replayGainAntipeak` (optional): A boolean value. Whether to apply clippin
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

If `null`, default values are used.
