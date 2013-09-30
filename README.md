decode_flac
===========

Playing around doing a flac decoder implementation. For now, it only output pcm
in an output file or on the standard output.

## Building

Typing `make` should do the trick. The built binary should be in the *bin*
directory.

## Usage

To use it, you have to give it a flac file as first argument. If there is no
second argument or if it is `-`, the pcm stream is outputed out the standard
output. Else the second argument should be the output file.

Checking the md5 of the outputted pcm against the md5 privided in the metadata
of the flac file is the main goal. To do that you type:
`$ ./bin/decode_flac_to_pcm some_flac_file.flac some_flac_file.pcm && md5sum
some_flac_file.pcm`
or
`$ ./bin/decode_flac_to_pcm some_flac_file.flac | md5sum`
and compare the md5 sums.
