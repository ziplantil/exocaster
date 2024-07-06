
# FAQ

* **Q**: The audio keeps skipping. exocaster prints "buffer overrun".
    * **A**: This happens when the encoder is not taking PCM data from
      the decoder quickly enough. You have multiple options: increase the
      ize of the buffers, particularly the encoder buffer; configure codecs
      to not use a minimum bitrate significantly if any lower than the
      nominal bitrate, and, if you don't want the decoder to ever skip
      samples, even if encoders are falling behind others, set the
      `pcmbuffer` `waitabs` value to be something high
      (e.g. `3600.0` for one hour).
