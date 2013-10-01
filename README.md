decode_flac
===========

Playing around doing a flac decoder implementation. For now, it only output pcm
in an output file or on the standard output.

## Building

Typing `make` should do the trick. The built binary should be in the *bin*
directory.

## Usage

To use it, you have to give it a flac file as first argument. If there is no
second argument or if it is `-`, the pcm stream is outputted on the standard
output. Else the second argument should be the output file.

Checking the md5 of the outputted pcm against the md5 provided in the metadata
of the flac file is the main goal. To do that you can type:

`$ ./bin/decode_flac_to_pcm some_flac_file.flac some_flac_file.pcm && md5sum
some_flac_file.pcm`
or
`$ ./bin/decode_flac_to_pcm some_flac_file.flac | md5sum`
and compare the md5 sums.

You can also add `--big-endian` to output pcm with big endian order. To compare
the md5, you can output big endian order pcm with mplayer for example:
`mplayer -ao pcm:nowaveheader:file=some_filename.pcm -format s16be
some_flac_file.flac`
Be sure to match the number of bits per sample though.
