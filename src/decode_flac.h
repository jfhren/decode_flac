/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef DECODE_FLAC_H
#define DECODE_FLAC_H
#include <stdint.h>


/**
 * If 64 bits integers are disallowed then 32 bits flac decoding is not
 * available.
 */
#ifdef DISALLOW_64_BITS
    #undef DECODE_32_BITS
#endif

/**
 * We define the types for decoded data.
 */
#ifdef DECODE_32_BITS
    #define DECODE_TYPE_64_BITS
    #define DECODE_UTYPE uint64_t
    #define DECODE_TYPE int64_t
#elif defined DECODE_16_BITS || defined DECODE_20_BITS || defined DECODE_24_BITS
    #define DECODE_TYPE_32_BITS
    #define DECODE_UTYPE uint32_t
    #define DECODE_TYPE int32_t
#else
    #define DECODE_TYPE_16_BITS
    #define DECODE_UTYPE uint16_t
    #define DECODE_TYPE int16_t
#endif

/**
 * We include then now since they need the previously defined preprocessor values.
 */
#include "input.h"
#include "output.h"

#define SUBFRAME_CONSTANT   0
#define SUBFRAME_VERBATIM   1
#define SUBFRAME_FIXED_LOW  8
#define SUBFRAME_FIXED_HIGH 12
#define SUBFRAME_LPC_LOW    32
#define SUBFRAME_LPC_HIGH   63

#ifndef STEREO_ONLY
    #define MONO                                                      0
#endif

#define LEFT_RIGHT                                                    1

#ifndef STEREO_ONLY
    #define LEFT_RIGHT_CENTER                                         2
    #define F_LEFT_F_RIGHT_B_LEFT_B_RIGHT                             3
    #define F_LEFT_F_RIGHT_F_CENTER_B_LEFT_B_RIGHT                    4
    #define F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT                5
    #define F_LEFT_F_RIGHT_F_CENTER_LFE_B_CENTER_S_LEFT_S_RIGHT       6
    #define F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT_S_LEFT_S_RIGHT 7
#endif

#define LEFT_SIDE                                                     8
#define RIGHT_SIDE                                                    9
#define MID_SIDE                                                      10

/**
 * Information about the flac stream.
 */
typedef struct {
    uint16_t min_block_size;    /**< The minimum number of samples in a block
                                     (across channels.) */
    uint16_t max_block_size;    /**< The maximum number of samples in a block
                                     (across channels.) Might be usefull for
                                     data output buffer allocation. */
    uint32_t min_frame_size;    /**< The minimum size in bytes of a frame. 0 if
                                     unknow. */
    uint32_t max_frame_size;    /**< The maximum size in bytes of a frame
                                     (might be usefull for data input buffer
                                     allocation.) 0 if unknow. */
    uint32_t sample_rate;       /**< The sample rate of the stream. */
    uint8_t nb_channels;        /**< The number of channels of the stream. */
    uint8_t bits_per_sample;    /**< The number of bits used to represent a
                                     sample. */
#ifndef DISALLOW_64_BITS
    uint64_t nb_samples;        /**< The number of encoded samples. 0 if
                                     unknow. */
#endif
    uint8_t md5[16];            /**< The md5 of the original pcm */
} stream_info_t;

#ifndef DISALLOW_64_BITS
#define STREAM_INFO_INIT() {.min_block_size = 0, .max_block_size = 0, .min_frame_size = 0, .max_frame_size = 0, .sample_rate = 0, .nb_channels = 0, .bits_per_sample = 0, .nb_samples = 0, .md5 = {0}}
#else
#define STREAM_INFO_INIT() {.min_block_size = 0, .max_block_size = 0, .min_frame_size = 0, .max_frame_size = 0, .sample_rate = 0, .nb_channels = 0, .bits_per_sample = 0, .md5 = {0}}
#endif

/**
 * Decode the flac metedata stream info and skip the others.
 *
 * @param data_input  The metadata are read from there.
 * @param stream_info The resulting useful informations are put there.
 *
 * @return Return 0 if successful, -1 else.
 */
int decode_flac_metadata(data_input_t* data_input, stream_info_t* stream_info);

/**
 * Decode flac stream into the output sink until the end is reached.
 *
 * @param data_input      The stream is read from there.
 * @param data_output     The decoded samples are outputed there.
 * @param bits_per_sample Number of bits per sample coming from the stream info
 *                        block.
 * @param nb_channels     The number of channels coming from the stream info
 *                        block.
 *
 * @return Return 0 if successful, -1 else.
 */
int decode_flac_data(data_input_t* data_input, data_output_t* data_output, uint8_t bits_per_sample, uint8_t nb_channels);

#endif
