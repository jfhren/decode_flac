/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdint.h>
#include "decode_flac.h"

#ifndef OUTPUT_H
#define OUTPUT_H
typedef struct {
    /* Where to write when dumping. */
    int fd;
    /* Used to buffer written data. */
    uint8_t* buffer;
    /* Size of the buffer. */
    int size;
    /* Size of the written data in the buffer. */
    int write_size;
    /* Where does the current frame start in the buffer. */
    int starting_position;
    /* What was the current shift before the current frame. */
    uint8_t starting_shift;
    /* The current write position in the buffer. */
    int position;
    /* The current bit shift inside the current byte. */
    uint8_t shift;
    /* Should the output be little endian style or not (that is big endian). */
    uint8_t is_little_endian;
    /* Should the output be signed or not. */
    uint8_t is_signed;
} data_output_t;


/**
 * We suppose that if value represent a signed integer then it is using two's
 * complement representation.  For now, we just fill the whole most significant
 * (64, 32 or 16 - size) bis with 1 if the size-th bit is one.
 * @param value The value to convert.
 * @param size The number of bits making up the value.
 * @return Return the converted value as a signed one on 64 bits, 32 bits or 16
 * bits depending on compile time flags.
 */
DECODE_TYPE convert_to_signed(DECODE_UTYPE, uint8_t size);

/**
 * Dump nb_bytes bytes from the output buffer to the output file descriptor
 * starting from 0. Nothing is modified within the data_output structure.
 * @param data_output The output buffer and the output file descriptor are
 *                    there.
 * @param nb_bytes    The number of bytes to dump from the output buffer.
 * @return Return 0 if successful, -1 else.
 */
int dump_buffer(data_output_t* data_output, int nb_bytes);

/**
 * Output a sample while taking care of its size, channel number and channel
 * assignement.  The output buffer should be of the right size (or at least
 * bigger than necessary.)
 * @param data_output         The output buffer is there.
 * @param sample              The sample to output.
 * @param sample_size         The sample size in bits.
 * @param channel_assignement The channel assignement of the sample (mono,
 *                            stereo, whatever...) @param channel_nb The channel
 *                            number of this sample w.r.t. the channel
 *                            assignement.
 * @return Return 0 if successful, -1 else.
 */
int put_shifted_bits(data_output_t* data_output, DECODE_UTYPE sample, uint8_t sample_size, uint8_t channel_assignement, uint8_t channel_nb);

#endif
