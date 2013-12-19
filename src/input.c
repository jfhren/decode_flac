/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "input.h"


int get_position(data_input_t* data_input) {
    return 0;
}

int skip_to_position(data_input_t* data_input, int position) {
    return 0;
}

int skip_nb_bytes(data_input_t* data_input, int nb_bytes_to_skip) {
    return 0;
}


/**
 * Test to see if the input should be reflled before having access to the
 * desired number of bytes.
 * @param data_input      Contain the input buffer and the necessary
 *                        information to test it.
 * @param nb_needed_bytes The number of needed bytes.
 * @return Return 1 if the buffer should be refilled, 0 else.
 */
int should_refill_input_buffer(data_input_t* data_input, int nb_needed_bytes) {

   return (data_input->read_size - data_input->position) < nb_needed_bytes;

} 


/**
 * Try to refill the input buffer with at least the desired number of bytes.
 * @param data_input      Contain the input buffer and the necessary
 *                        information to refill it.
 * @param nb_needed_bytes The number of needed bytes.
 * @return Return 0 if at least the buffer was refilled of the desired number
 *         of bytes, -1 else.
 */
int refill_input_buffer_at_least(data_input_t* data_input, int nb_needed_bytes) {

    int error_code = refill_input_buffer(data_input);

    if(error_code == -1)
        return -1;

    if((error_code == 0) && (data_input->size >= nb_needed_bytes) && ((data_input->read_size - data_input->position) < nb_needed_bytes)) {
        fprintf(stderr, "Unexpected end of file.\n");
        return -1;
    }

    return 0;

}


/**
 * Try to refill the input buffer.
 * Refill the input buffer from the last position while preserving any unused
 * bytes in the input buffer.
 * @param data_input Contain the input buffer and the necessary information to
 *                   refill it.
 * @return Return 1 if the refill was complete, 0 if it was a partial one or -1
 *                  if an error occured.
 */
int refill_input_buffer(data_input_t* data_input) {

    int nb_read_bytes = 0;
    int total_nb_read_bytes = data_input->read_size - data_input->position;
    /* Try to read by the biggest chunk possible (might be silly though) */
    int nb_bytes_to_read = data_input->position;

    if(data_input->position > 0) {
        if(data_input->position < (data_input->read_size >> 2))
            memmove(data_input->buffer, data_input->buffer + data_input->position, total_nb_read_bytes);
        else
            memcpy(data_input->buffer, data_input->buffer + data_input->position, total_nb_read_bytes);
    }

    while((nb_read_bytes = read(data_input->fd, data_input->buffer + total_nb_read_bytes, nb_bytes_to_read)) > 0) {
        total_nb_read_bytes += nb_read_bytes;
        if(total_nb_read_bytes == data_input->read_size) {
            data_input->position = 0;
            return 1;
        }
        nb_bytes_to_read = data_input->read_size - total_nb_read_bytes;
    }

    if(nb_read_bytes == -1) {
        perror("An error occured while refilling the input buffer");
        return -1;
    }

    data_input->read_size = total_nb_read_bytes;
    data_input->position = 0;

    if(nb_read_bytes == 0)
        return 0;

    return 1;

}


/**
 * Get bits from the input taking into shift in byte.
 * @param data_input     Bits and bytes are read from there.
 * @param requested_size The number of bits to get.
 * @param error_code     If any error occurs, it will be equal to -1.
 * @return Return the requested bits.
 */
#ifdef DISALLOW_64_BITS
uint32_t get_shifted_bits(data_input_t* data_input, uint8_t requested_size, int* error_code) {

    uint64_t value = 0;
#else
uint64_t get_shifted_bits(data_input_t* data_input, uint8_t requested_size, int* error_code) {

    uint64_t value = 0;
#endif

    uint8_t nb_needed_bits = (requested_size + data_input->shift);
    uint8_t nb_needed_bytes = nb_needed_bits / 8;
    uint8_t* buffer = NULL;
    int position = 0;
    uint8_t shift = 0;

    *error_code = 0;

    if((data_input->shift != 0) || (nb_needed_bits % 8) != 0)
        ++nb_needed_bytes;

    if((data_input->read_size - data_input->position) < nb_needed_bytes) {
        *error_code = refill_input_buffer(data_input);

        if(*error_code == -1)
            return 0;

        if((*error_code == 0) && ((data_input->read_size - data_input->position) < nb_needed_bytes)) {
            fprintf(stderr, "Unexpected end of file.\n");
            *error_code = -1;
            return 0;
        }
    }

    buffer = data_input->buffer;
    position = data_input->position;
    shift = data_input->shift;

    if(shift == 0) {
        switch(requested_size) {
            case 8:
                return buffer[data_input->position++];
                
            case 16:
                value = (buffer[position] << 8) | buffer[position + 1];
                data_input->position += 2;
                return value;

            case 24:
                value = (buffer[position] << 16) | (buffer[position + 1] << 8) | buffer[position + 2];
                data_input->position += 3;
                return value;

            case 32:
                value = (buffer[position] << 24) | (buffer[position + 1] << 16) | (buffer[position + 2] << 8) | buffer[position + 3];
                data_input->position += 4;
                return value;

#ifndef DISALLOW_64_BITS
            case 40:
                value = ((int64_t)buffer[position] << 32) | (buffer[position + 1] << 24) | (buffer[position + 2] << 16) | (buffer[position + 3] << 8) | buffer[position + 4];
                data_input->position += 5;
                return value;

            case 48:
                value = ((int64_t)buffer[position] << 40) | ((int64_t)buffer[position + 1] << 32) | (buffer[position + 2] << 24) | (buffer[position + 3] << 16) | (buffer[position + 4] << 8) | buffer[position + 5];
                data_input->position += 6;
                return value;

            case 56:
                value = ((int64_t)buffer[position] << 48) | ((int64_t)buffer[position + 1] << 40) | ((int64_t)buffer[position + 2] << 32) | (buffer[position + 3] << 24) | (buffer[position + 4] << 16) | (buffer[position + 5] << 8) | buffer[position + 6];
                data_input->position += 7;
                return value;

            case 64:
                value = ((int64_t)buffer[position] << 56) | ((int64_t)buffer[position + 1] << 48) | ((int64_t)buffer[position + 2] << 40) | ((int64_t)buffer[position + 3] << 32) | (buffer[position + 4] << 24) | (buffer[position + 5] << 16) | (buffer[position + 6] << 8) | buffer[position + 7];
                data_input->position += 8;
                return value;
#endif
        }
    }

    if(nb_needed_bits < 8) {
        value = ((buffer[position] & (0xFF >> shift)) >> (8 - nb_needed_bits));
        data_input->shift += requested_size;
    } else if(nb_needed_bits < 16) {
        value = ((buffer[position] & (0xFF >> shift)) << (nb_needed_bits - 8)) | (buffer[position + 1] >> (16 - nb_needed_bits));
        data_input->position +=1;
        data_input->shift = nb_needed_bits - 8;
    } else if(nb_needed_bits < 24) {
        value = ((buffer[position] & (0xFF >> shift)) << (nb_needed_bits - 8)) | (buffer[position + 1] << (nb_needed_bits - 16)) | (buffer[position + 2] >> (24 - nb_needed_bits));
        data_input->position += 2;
        data_input->shift = nb_needed_bits - 16;
    } else if(nb_needed_bits < 32) {
        value = ((buffer[position] & (0xFF >> shift)) << (nb_needed_bits - 8)) | (buffer[position + 1] << (nb_needed_bits - 16)) | (buffer[position + 2] << (nb_needed_bits - 24)) | (buffer[position + 3] >> (32 - nb_needed_bits));
        data_input->position += 3;
        data_input->shift = nb_needed_bits - 24;
    }
#ifndef DISALLOW_64_BITS
     else if(nb_needed_bits < 40) {
        value = ((int64_t)(buffer[position] & (0xFF >> shift)) << (nb_needed_bits - 8)) | (buffer[position + 1] << (nb_needed_bits - 16)) | (buffer[position + 2] << (nb_needed_bits - 24)) | (buffer[position + 3] << (nb_needed_bits - 32)) | (buffer[position + 4] >> (40 - nb_needed_bits));
        data_input->position += 4;
        data_input->shift = nb_needed_bits - 32;
    } else if(nb_needed_bits < 48) {
        value = ((int64_t)(buffer[position] & (0xFF >> shift)) << (nb_needed_bits - 8)) | ((int64_t)buffer[position + 1] << (nb_needed_bits - 16)) | (buffer[position + 2] << (nb_needed_bits - 24)) | (buffer[position + 3] << (nb_needed_bits - 32)) | (buffer[position + 4] << (nb_needed_bits - 40)) | (buffer[position + 5] >> (48 - nb_needed_bits));
        data_input->position += 5;
        data_input->shift = nb_needed_bits - 40;
    } else if(nb_needed_bits < 56) {
        value = ((int64_t)(buffer[position] & (0xFF >> shift)) << (nb_needed_bits - 8)) | ((int64_t)buffer[position + 1] << (nb_needed_bits - 16)) | ((int64_t)buffer[position + 2] << (nb_needed_bits - 24)) | (buffer[position + 3] << (nb_needed_bits - 32)) | (buffer[position + 4] << (nb_needed_bits - 40)) | (buffer[position + 5] << (nb_needed_bits - 48)) | (buffer[position + 6] >> (56 - nb_needed_bits));
        data_input->position += 6;
        data_input->shift = nb_needed_bits - 48;
    } else if(nb_needed_bits < 64) {
        value = ((int64_t)(buffer[position] & (0xFF >> shift))  << (nb_needed_bits - 8)) | ((int64_t)buffer[position + 1] << (nb_needed_bits - 16)) | ((int64_t)buffer[position + 2] << (nb_needed_bits - 24)) | ((int64_t)buffer[position + 3] << (nb_needed_bits - 32)) | (buffer[position + 4] << (nb_needed_bits - 40)) | (buffer[position + 5] << (nb_needed_bits - 48)) | (buffer[position + 6] << (nb_needed_bits - 56)) | (buffer[position + 7] >> (64 - nb_needed_bits));
        data_input->position += 7;
        data_input->shift = nb_needed_bits - 56;
    } else if(nb_needed_bits < 72) {
        value = ((int64_t)(buffer[position] & (0xFF >> shift))  << (nb_needed_bits - 8)) | ((int64_t)buffer[position + 1] << (nb_needed_bits - 16)) | ((int64_t)buffer[position + 2] << (nb_needed_bits - 24)) | ((int64_t)buffer[position + 3] << (nb_needed_bits - 32)) | (buffer[position + 4] << (nb_needed_bits - 40)) | (buffer[position + 5] << (nb_needed_bits - 48)) | (buffer[position + 6] << (nb_needed_bits - 56)) | (buffer[position + 7] << (nb_needed_bits - 64)) | (buffer[position + 8] >> (72 - nb_needed_bits));
        data_input->position += 8;
        data_input->shift = nb_needed_bits - 64;
    }
#endif

    return value;

}
