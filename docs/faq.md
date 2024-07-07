
# FAQ

* **Q**: The audio keeps skipping. exocaster prints "buffer overrun".
    * **A**: This happens when the encoder is not taking PCM data from
      the decoder quickly enough.

      The most common reason for this is a sudden switch from a low-bitrate
      to high-bitrate regime, e.g. when a song starts playing after a long
      period of silence, which is well compressed by most codecs. Silence
      results in low-bitrate blocks, which use little data to represent
      a long period of audio, compared to music, which requires more bitrate.

      When broadcasting audio in real time, this will result in low-bitrate
      blocks bogging down every block that follows. The encoder buffer
      will often already be full, so the flow of packets often stops.

      The options you have are:
      * use codecs that output a relatively constant bitrate, or configure
        them to do so,
      * increase `pcmbuffer` `margin` to allow some more play in how much the
        decoder waits for its turn,
      * and/or disable skipping entirely by setting `pcmbuffer` `skip` to
        `false`, if you can afford it.

* **Q**: The audio keeps dropping. exocaster prints "buffer underrun".
    * **A**: Buffer underruns happen if the encoder has to wait too long for
      the decoder to supply data. If this happens, the decoder may be too slow,
      or it might not be able to read data quickly enough.

      If this happens between song changes, try increasing the `duration`
      of the `pcmbuffer`.
