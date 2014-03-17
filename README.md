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

You can check the md5 sum of the outputted pcm against the md5 sum provided in
the metadata of the flac file. To do that you can type:  
`$ ./bin/decode_flac_to_pcm some_flac_file.flac some_flac_file.pcm && md5sum
some_flac_file.pcm`  
or  
`$ ./bin/decode_flac_to_pcm some_flac_file.flac | md5sum`  
and compare the md5 sums.

You can play the outputed pcm with `aplay`. For flac encoded from a cd, you can
type:  
`$ ./bin/decode_flac_to_pcm some_flac_file.flac | aplay -f cd -`
or use the bash script `play_flac.sh`:  
`$ PATH=$PATH:./bin/decode_flac_to_pcm; export PATH; ./play_flac.sh some_flac_file.flac some_other_flac_file.flac`

Some options are available:  

- `--big-endian`: output pcm with big endian order. To compare the md5 sums, you 
can output big endian order pcm with mplayer for example:  
`mplayer -ao pcm:nowaveheader:file=some_filename.pcm -format s16be
some_flac_file.flac`  
Be sure to match the number of bits per sample though.

- `--unsigned`: output pcm with unsigned samples. To compare the md5 sums, you 
can output unsigned samples pcm with mplayer for example:  
`mplayer -ao pcm:nowaveheader:file=some_filename.pcm -format u16le
some_flac_file.flac`  
Be sure to match the number of bits per sample though. You can combine these
options.

- `--input-size bytes`: define the size of the input buffer. Should be greater 
than 42.

- `--max-output-size`: define the maximum size of the output buffer. It will be
truncated to a multiple of the number of bits per sample times the number of
channels.

- `-i`: add a pause capability by pressing enter.

- `-q`: suppress all informatinal outputs.
