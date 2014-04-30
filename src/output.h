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

struct data_output_t;

/**
 * Eventually dump the output buffer.
 *
 * @param data_output The output buffer is there.
 * @param nb_bits     The number of bits added to the buffer since the last call.
 * @param force_dump  Force the dumping of the buffer (usefull if it is the last call).
 *
 * @return Return 0 if successful, -1 else.
 */
typedef int(*dump_func_t)(struct data_output_t* data_output, int nb_bits, uint8_t force_dump);


/**
 * Represent the output stream.
 */
typedef struct data_output_t {
    dump_func_t dump_func;      /**< Function used to dump the buffer. */
    uint8_t* buffer;            /**< Used to buffer written data. */
    int size;                   /**< Size of the buffer. */
    int write_size;             /**< Size of the written data in the buffer.
                                     FIXME is that even usefull anymore ? */
    int starting_position;      /**< Where does the current frame start in the
                                     buffer. */
    uint8_t starting_shift;     /**< What was the current shift before the
                                     current frame. */
    int position;               /**< The current write position in the buffer. */
    uint8_t shift;              /**< The current bit shift inside the current
                                     byte. */
    uint8_t is_little_endian;   /**< Should the output be little endian style
                                     or not (that is big endian). */
    uint8_t is_signed;          /**< Should the output be signed or not. */
} data_output_t;

/**
 * Init the output to a file descriptor.
 *
 * @param data_output      The structure representing the output to fill out.
 * @param fd               The output file descriptor.
 * @param buffer_size      The size of the output buffer.
 * @param is_little_endian Should the output be in little endian?
 * @param is_signed        Should the output be signed?
 * @param can_pause        Can the output be paused?
 *
 * @return Return 0 if successful, -1 else.
 */
int init_data_output_to_fd(data_output_t* data_output, int fd, int buffer_size, uint8_t is_little_endian, uint8_t is_signed, uint8_t can_pause);

/**
 * We suppose that if value represent a signed integer then it is using two's
 * complement representation.  For now, we just fill the whole most significant
 * (64 - size) bis with 1 if the size-th bit is one.
 *
 * @param value The value to convert.
 * @param size The number of bits making up the value.
 *
 * @return Return the converted value as a signed one on 64 bits.
 */
static inline DECODE_TYPE convert_to_signed(DECODE_UTYPE value, uint8_t size) {

#ifdef DECODE_TYPE_16_BITS
    return value & (((DECODE_UTYPE)1) << (size - 1)) ? (((DECODE_UTYPE)0xFFFFu) << size) | value : value;
#elif defined DECODE_TYPE_32_BITS
    return value & (((DECODE_UTYPE)1) << (size - 1)) ? (((DECODE_UTYPE)0xFFFFFFFFu) << size) | value : value;
#elif defined DECODE_TYPE_64_BITS
    return value & (((DECODE_UTYPE)1) << (size - 1)) ? (((DECODE_UTYPE)0xFFFFFFFFFFFFFFFFull) << size) | value : value;
#else
    #error "Should not happen but kinda did."
#endif

}

/**
 * Eventually dump the output buffer using the setted dump function.
 *
 * @param data_output The output buffer is there.
 * @param nb_bits     The number of bits added to the buffer since the last call.
 * @param force_dump  Force the dumping of the buffer (usefull if it is the last call).
 *
 * @return Return 0 if successful, -1 else.
 */
static inline int dump_buffer(data_output_t* data_output, int nb_bits, uint8_t force_dump) {

    return data_output->dump_func(data_output, nb_bits, force_dump);

}

/**
 * Output a sample while taking care of its size, channel number, channel
 * assignement and buffer remaining space. A sample is not added to the buffer
 * if there is not enough space for it whole.
 *
 * @param data_output         The output buffer is there.
 * @param sample              The sample to output.
 * @param sample_size         The sample size in bits.
 * @param channel_assignement The channel assignement of the sample (mono,
 *                            stereo, whatever...) @param channel_nb The channel
 *                            number of this sample w.r.t. the channel
 *                            assignement.
 * @param channel_nb          The channel number to which appertains the sample.
 *
 * @return Return 1 if successful, 0 if the sample could not be put in the
 *         buffer, -1 else.
 */
int put_shifted_bits(data_output_t* data_output, DECODE_UTYPE sample, uint8_t sample_size, uint8_t channel_assignement, uint8_t channel_nb);

#endif
