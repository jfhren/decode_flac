/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef INPUT_H
#define INPUT_H
#include <stdint.h>

typedef struct {
    /* A file descriptor of the file being read. */
    int fd;
    /* Used to buffer read data. */
    uint8_t* buffer;
    /* The size of the buffer. */
    int size;
    /* The size of the read data in the buffer. */
    int read_size;
    /* The current read position in the buffer. */
    int position;
    /* The current bit shift inside the current byte. */
    uint8_t shift;
} data_input_t;


int get_position(data_input_t* data_input);

int skip_to_position(data_input_t* data_input, int position);

int skip_nb_bits(data_input_t* data_input, int nb_bits_to_skip);

/**
 * Test to see if the input should be reflled before having access to the
 * desired number of bytes.
 * @param data_input      Contain the input buffer and the necessary
 *                        information to test it.
 * @param nb_needed_bytes The number of needed bytes.
 * @return Return 1 if the buffer should be refilled, 0 else.
 */
int should_refill_input_buffer(data_input_t* data_input, int nb_needed_bytes);

/**
 * Try to refill the input buffer with at least the desired number of bytes.
 * @param data_input      Contain the input buffer and the necessary
 *                        information to refill it.
 * @param nb_needed_bytes The number of needed bytes.
 * @return Return 0 if at least the buffer was refilled of the desired number
 *         of bytes, -1 else.
 */
int refill_input_buffer_at_least(data_input_t* data_input, int nb_needed_bytes);

/**
 * Try to refill the input buffer.
 * Refill the input buffer from the last position while preserving any unused
 * bytes in the input buffer.
 * @param data_input Contain the input buffer and the necessary information to
 *                   refill it.
 * @return Return 1 if the refill was complete, 0 if it was a partial one or -1
 *                  if an error occured.
 */
int refill_input_buffer(data_input_t* data_input);

/**
 * Get bits from the input taking into shift in byte.
 * @param data_input     Bits and bytes are read from there.
 * @param requested_size The number of bits to get.
 * @param error_code     If any error occurs, it will be equal to -1.
 * @return Return the requested bits.
 */
#ifdef DISALLOW_64_BITS
uint32_t get_shifted_bits(data_input_t* data_input, uint8_t requested_size, int* error_code);
#else
uint64_t get_shifted_bits(data_input_t* data_input, uint8_t requested_size, int* error_code);
#endif

#endif
