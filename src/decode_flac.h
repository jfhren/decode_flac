/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef DECODE_FLAC_H
#define DECODE_FLAC_H
#include <stdint.h>

#include "input.h"
#include "output.h"

#define SUBFRAME_CONSTANT   0
#define SUBFRAME_VERBATIM   1
#define SUBFRAME_FIXED_LOW  8
#define SUBFRAME_FIXED_HIGH 12
#define SUBFRAME_LPC_LOW    32
#define SUBFRAME_LPC_HIGH   63

#define MONO                                                      0
#define LEFT_RIGHT                                                1
#define LEFT_RIGHT_CENTER                                         2
#define F_LEFT_F_RIGHT_B_LEFT_B_RIGHT                             3
#define F_LEFT_F_RIGHT_F_CENTER_B_LEFT_B_RIGHT                    4
#define F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT                5
#define F_LEFT_F_RIGHT_F_CENTER_LFE_B_CENTER_S_LEFT_S_RIGHT       6
#define F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT_S_LEFT_S_RIGHT 7
#define LEFT_SIDE                                                 8
#define RIGHT_SIDE                                                9
#define MID_SIDE                                                  10

typedef struct {
    /* The minimum number of samples in a block (across channels.) */
    uint16_t min_block_size;
    /* The maximum number of samples in a black (across channels.) Might be
       usefull for data output buffer allocation. */
    uint16_t max_block_size;
    /* The minimum size in bytes of a frame. 0 if unknow. */
    uint32_t min_frame_size;
    /* The maximum size in bytes of a frame (might be usefull for data input
       buffer allocation.) 0 if unknow. */
    uint32_t max_frame_size;
    /* The sample rate of the stream. */
    uint32_t sample_rate;
    /* The number of channels of the stream. */
    uint8_t nb_channels;
    /* The number of bits used to represent a sample. */
    uint8_t bits_per_sample;
    /* The number of encoded samples. 0 if unknow. */
    uint64_t nb_samples;
    /* The md5 of the original pcm */
    uint8_t md5[16];
} stream_info_t;

typedef struct {
    /* The number of samples in the block encoded by this frame. */
    uint16_t block_size;
    /* How many channel there is and how channels are encoded. */
    uint8_t channel_assignement;
    /* The number of bits used to represent a sample. Should be the same than
       in the stream info. */
    uint8_t bits_per_sample;
} frame_info_t;

typedef struct {
    /* Tell how the sample are encoded within the subframe. */
    uint8_t type;
    /* How many bits are waster per sample (not sure it is correctly
       implemented for now but it is rare so whatever for now.) */
    uint8_t wasted_bits_per_sample;
} subframe_info_t;


/**
 * Decode the flac metedata stream info and skip the others.
 * @param data_input  The metadata are read from there.
 * @param stream_info The resulting useful informations are put there.
 * @return Return 0 if successful, -1 else.
 */
int decode_flac_metadata(data_input_t* data_input, stream_info_t* stream_info);

/**
 * Decode flac stream into the output sink until the end is reached.
 * @param data_input  The stream is read from there.
 * @param data_output The decoded samples are outputed there.
 * @param stream_info Useful for the decoding frame headers.
 * @return Return 0 if successful, -1 else.
 */
int decode_flac_data(data_input_t* data_input, data_output_t* data_output, stream_info_t* stream_info);

#endif
