decode_flac
===========

Playing around doing a flac decoder implementation. For now, it only output pcm
in an output file or on the standard output.

## Building

Typing `make` should do the trick. The built binary should be in the *bin*
directory.
You can tweak the makefile to add the STEREO_ONLY macro to compile a version
supporting only stereo channel assignments (LEFT_RIGHT, LEFT_SIDE, RIGHT_SIDE,
MID_SIDE).

## Usage

To use it, you have to give it a flac file as first argument. If there is no
second argument or if it is `-`, the pcm stream is outputted on the standard
output. Else the second argument should be the output file.

Checking the md5 sum of the outputted pcm against the md5 sum provided in the
metadata of the flac file is the main goal. To do that you can type:  
`$ ./bin/decode_flac_to_pcm some_flac_file.flac some_flac_file.pcm && md5sum
some_flac_file.pcm`  
or  
`$ ./bin/decode_flac_to_pcm some_flac_file.flac | md5sum`  
and compare the md5 sums.

You can add `--big-endian` to output pcm with big endian order. To compare the
md5 sums, you can output big endian order pcm with mplayer for example:  
`mplayer -ao pcm:nowaveheader:file=some_filename.pcm -format s16be
some_flac_file.flac`  
Be sure to match the number of bits per sample though.

You can also add `--unsigned` to output pcm with unsigned samples. To compare
the md5 sums, you can output unsigned samples pcm with mplayer for example:  
`mplayer -ao pcm:nowaveheader:file=some_filename.pcm -format u16le
some_flac_file.flac`  
Be sure to match the number of bits per sample though. You can combine these
options.
