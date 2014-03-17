#!/bin/bash

while [ -e "$1" ]
do
    echo "$1"
    decode_flac_to_pcm "$1" -q -i | aplay -q -t raw -V stereo `get_aplay_param "$1"` --disable-softvol -
    shift
done
