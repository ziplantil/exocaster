
# MP3 encoder

Type: `mp3`

Encodes PCM data into MP3.

## Configuration

A JSON object with the following fields:

* `bitrate` (optional): The bitrate to use, in kilobits per second.
  In CBR, this is constant; in VBR, this is the mean bitrate,
  which may be ignored when not in ABR mode.
  (default: 320). 
* `vbr` (optional): A boolean value. If `true`, allows a variable bit rate.
  (default: `false`)
* `minbitrate` (optional): The minimum bitrate in kilobits per second.
  Only matters if `vbr` is `true`. If specified, enables ABR mode.
  If not specified, the minimum bitrate is not limited.
* `maxbitrate` (optional): The maximum bitrate in kilobits per second.
  Only matters if `vbr` is `true`. If specified, enables ABR mode.
  If not specified, the maximum bitrate is not limited.

## Notes

ID3 is not suitable for streaming purposes, so the mp3 encoder does not
encode metadata in-band. Out-of-band metadata is only supported by
some brocas.
