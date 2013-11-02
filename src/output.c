/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "output.h"
#include "decode_flac.h"


/**
 * We suppose that if value represent a signed integer then it is using two's
 * complement representation.  For now, we just fill the whole most significant
 * (64 - size) bis with 1 if the size-th bit is one.
 * @param value The value to convert.
 * @param size The number of bits making up the value.
 * @return Return the converted value as a signed one on 64 bits.
 */
DECODE_TYPE convert_to_signed(DECODE_UTYPE value, uint8_t size) {

#if DECODE_UTYPE == uint16_t
    return value & (((DECODE_UTYPE)1) << (size - 1)) ? (((DECODE_UTYPE)0xFFFFu) << size) | value : value;
#elif DECODE_UTYPE == uint32_t
    return value & (((DECODE_UTYPE)1) << (size - 1)) ? (((DECODE_UTYPE)0xFFFFFFFFu) << size) | value : value;
#else
    return value & (((DECODE_UTYPE)1) << (size - 1)) ? (((DECODE_UTYPE)0xFFFFFFFFFFFFFFFFull) << size) | value : value;
#endif 

}


/**
 * Dump nb_bytes bytes from the output buffer to the output file descriptor
 * starting from 0. Nothing is modified within the data_output structure.
 * @param data_output The output buffer and the output file descriptor are
 *                    there.
 * @param nb_bytes    The number of bytes to dump from the output buffer.
 * @return Return 0 if successful, -1 else.
 */
int dump_buffer(data_output_t* data_output, int nb_bytes) {

    int nb_written_bytes_since_start = 0;
    int nb_written_bytes = 0;

    while((nb_written_bytes = write(data_output->fd, data_output->buffer + nb_written_bytes_since_start, nb_bytes - nb_written_bytes_since_start)) > 0) {
        nb_written_bytes_since_start += nb_written_bytes;
        if(nb_written_bytes_since_start == nb_bytes)
            return 0;
    }

    if(nb_written_bytes == -1) {
        perror("An error occured while dumping the buffer");
        return -1;
    }

    return 0;

}


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
int put_shifted_bits(data_output_t* data_output, DECODE_UTYPE sample, uint8_t sample_size, uint8_t channel_assignement, uint8_t channel_nb) {

    uint8_t* buffer = data_output->buffer;
    uint32_t position = data_output->position;
#if defined DECODE_12_BITS || defined DECODE_20_BITS
    uint8_t shift = data_output->shift;
#endif

    switch(channel_assignement) {
        case LEFT_SIDE:
            if(channel_nb == 0) {
                switch(sample_size) {
#ifdef DECODE_8_BITS
                    case 8:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 8) + 128);

                        buffer[position] = sample & 0xFF;
                        data_output->position += 2;
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 12:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 12) + 2048);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 4) & 0xF0;
                        } else {
                            buffer[position] = (sample >> 4) & 0xFF;
                            buffer[position + 1] = (sample << 4) & 0xF0;
                        }
                        data_output->position += 3;
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 16:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 16) + 32768);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                        } else {
                            buffer[position] = (sample >> 8) & 0xFF;
                            buffer[position + 1] = sample & 0xFF;
                        }
                        data_output->position += 4;
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 20:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 20) + 524288);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = (sample >> 12) & 0xF0;
                        } else {
                            buffer[position] = (sample >> 12) & 0xFF;
                            buffer[position + 1] = (sample >> 4) & 0xFF;
                            buffer[position + 2] = (sample << 4) & 0xF0;
                        }
                        data_output->position += 5;
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 24:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 24) + 8388608);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = (sample >> 16) & 0xFF;
                        } else {
                            buffer[position] = (sample >> 16) & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = sample & 0xFF;
                        }
                        data_output->position += 6;
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 32:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 32) + 2147483648u);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = (sample >> 16) & 0xFF;
                            buffer[position + 3] = (sample >> 24) & 0xFF;
                        } else {
                            buffer[position] = (sample >> 24) & 0xFF;
                            buffer[position + 1] = (sample >> 16) & 0xFF;
                            buffer[position + 2] = (sample >> 8) & 0xFF;
                            buffer[position + 3] = sample & 0xFF;
                        }
                        data_output->position += 8;
#endif
                }
            } else {
#if (defined DECODE_12_BITS) || (defined DECODE_16_BITS) || (defined DECODE_20_BITS) || (defined DECODE_24_BITS) || (defined DECODE_32_BITS)
                DECODE_UTYPE uleft = 0;
#endif
                DECODE_TYPE left = 0;
                DECODE_UTYPE uright = 0;

                switch(sample_size) {
#ifdef DECODE_8_BITS
                    case 9:
                        left = convert_to_signed(buffer[position - 1], 8);
                        uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 9));

                        buffer[position] = uright & 0xFF;
                        data_output->position += 2;
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 13:
                        if(data_output->is_little_endian) {
                            uleft = ((buffer[position] & 0xF0) << 8) | buffer[position - 1];
                            left = convert_to_signed(uleft, 12);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 13));

                            buffer[position] |= (uright >> 4) & 0x0F;
                            buffer[position + 1] = ((uright << 4) & 0xF0) | ((uright >> 8) & 0x0F) ;
                        } else {
                            uleft = (buffer[position - 1] << 4) | ((buffer[position] & 0xF0) >> 4);
                            left = convert_to_signed(uleft, 12);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 13));

                            buffer[position] |= (uright >> 8) & 0x0F;
                            buffer[position + 1] = uright & 0xFF;
                        }
                        data_output->position += 3;
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 17:
                        if(data_output->is_little_endian) {
                            uleft = (buffer[position - 1] << 8) | buffer[position - 2];
                            left = convert_to_signed(uleft, 16);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 17));

                            buffer[position] = uright & 0xFF;
                            buffer[position + 1] = (uright >> 8) & 0xFF;
                        } else {
                            uleft = (buffer[position - 2] << 8) | buffer[position - 1];
                            left = convert_to_signed(uleft, 16);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 17));

                            buffer[position] = (uright >> 8) & 0xFF;
                            buffer[position + 1] = uright & 0xFF;
                        }
                        data_output->position += 4;
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 21:
                        if(data_output->is_little_endian) {
                            uleft = ((buffer[position] & 0xF0) << 16) | (buffer[position - 1] << 8) | buffer[position - 2];
                            left = convert_to_signed(uleft, 20);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 21));

                            buffer[position] |= (uright >> 4) & 0x0F;
                            buffer[position + 1] = ((uright << 4) & 0xF0) | ((uright >> 12) & 0x0F);
                            buffer[position + 2] = ((uright >> 4) & 0xF0) | ((uright >> 16) & 0x0F);
                        } else {
                            uleft = (buffer[position - 2] << 12) | (buffer[position - 1] << 4) | (buffer[position] >> 4);
                            left = convert_to_signed(uleft, 20);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 21));

                            buffer[position] |= (uright >> 16) & 0x0F;
                            buffer[position + 1] = (uright >> 8) & 0xFF;
                            buffer[position + 2] = uright & 0xFF;
                        }
                        data_output->position += 5;
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 25:
                        if(data_output->is_little_endian) {
                            uleft = (buffer[position - 1] << 16) | (buffer[position - 2] << 8) | buffer[position - 3];
                            left = convert_to_signed(uleft, 24);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 25));

                            buffer[position] = uright & 0xFF;
                            buffer[position + 1] = (uright >> 8) & 0xFF;
                            buffer[position + 2] = (uright >> 16) & 0xFF;
                        } else {
                            uleft = (buffer[position - 3] << 16) | (buffer[position - 2] << 8) | buffer[position - 1];
                            left = convert_to_signed(uleft, 24);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 25));

                            buffer[position] = (uright >> 16) & 0xFF;
                            buffer[position + 1] = (uright >> 8) & 0xFF;
                            buffer[position + 2] = uright & 0xFF;
                        }
                        data_output->position += 6;
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 33:
                        if(data_output->is_little_endian) {
                            uleft = (buffer[position - 1] << 24) | (buffer[position - 2] << 16) | (buffer[position - 3] << 8) | buffer[position - 4];
                            left = convert_to_signed(uleft, 32);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 33));

                            buffer[position] = uright & 0xFF;
                            buffer[position + 1] = (uright >> 8) & 0xFF;
                            buffer[position + 2] = (uright >> 16) & 0xFF;
                            buffer[position + 3] = (uright >> 24) & 0xFF;
                        } else {
                            uleft = (buffer[position - 4] << 24) | (buffer[position - 3] << 16) | (buffer[position - 2] << 8) | buffer[position - 1];
                            left = convert_to_signed(uleft, 32);
                            uright = (DECODE_UTYPE)(left - convert_to_signed(sample, 33));

                            buffer[position] = (uright >> 24) & 0xFF;
                            buffer[position + 1] = (uright >> 16) & 0xFF;
                            buffer[position + 2] = (uright >> 8) & 0xFF;
                            buffer[position + 3] = uright & 0xFF;
                        }
                        data_output->position += 8;
#endif
                }
            }
            break;

        case RIGHT_SIDE:
            if(channel_nb == 0) {       /* Since we read the difference first, we store it in the buffer as big endian (because I can~)*/
                switch(sample_size) {
#ifdef DECODE_8_BITS
                    case 9:
                        buffer[position] = sample >> 1;
                        buffer[position + 1] = sample & 0x01;
                        data_output->position += 2;
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 13:
                        buffer[position] = sample >> 5;
                        buffer[position + 1] = sample & 0x1F;
                        data_output->position += 3;
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 17:
                        buffer[position] = sample >> 9;
                        buffer[position + 1] = (sample >> 1) & 0xFF;
                        buffer[position + 2] = sample & 0x01;
                        data_output->position += 4;
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 21:
                        buffer[position] = sample >> 13;
                        buffer[position + 1] = (sample >> 5) & 0xFF;
                        buffer[position + 2] = sample & 0x1F;
                        data_output->position += 5;
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 25:
                        buffer[position] = sample >> 17;
                        buffer[position + 1] = (sample >> 9) & 0xFF;
                        buffer[position + 2] = (sample >> 1) & 0xFF;
                        buffer[position + 3] = sample & 0x01;
                        data_output->position += 6;
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 33:
                        buffer[position] = sample >> 25;
                        buffer[position + 1] = (sample >> 17) & 0xFF;
                        buffer[position + 2] = (sample >> 9) & 0xFF;
                        buffer[position + 3] = (sample >> 1) & 0xFF;
                        buffer[position + 4] = sample & 0x01;
                        data_output->position += 8;
#endif
                }
            } else {
                switch(sample_size) {
                    DECODE_TYPE difference = 0;
                    DECODE_UTYPE uleft = 0;
                    DECODE_TYPE right = 0;

#ifdef DECODE_8_BITS
                    case 8:
                        difference = convert_to_signed((buffer[position - 1] << 1) | (buffer[position] & 0x01), 9);
                        if(data_output->is_signed)
                            right = convert_to_signed(sample, 8);
                        else
                            right = convert_to_signed(sample, 8) + 128;
                        uleft = (DECODE_UTYPE)(right + difference);

                        buffer[position - 1] = uleft & 0xFF;
                        buffer[position] = right & 0xFF;
                        data_output->position += 2;
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 12:
                        difference = convert_to_signed((buffer[position - 1] << 5) | (buffer[position] & 0x1F), 13);
                        if(data_output->is_signed)
                            right = convert_to_signed(sample, 12);
                        else
                            right = convert_to_signed(sample, 12) + 2048;
                        uleft = (DECODE_UTYPE)(right + difference);

                        if(data_output->is_little_endian) {
                            buffer[position - 1] = uleft & 0xFF;
                            buffer[position] = ((uleft >> 4) & 0xF0) | ((right >> 4) & 0x0F);
                            buffer[position + 1] = ((right << 4) & 0xF0) | ((right >> 8) & 0x0F);
                        } else {
                            buffer[position - 1] = (uleft >> 4) & 0xFF;
                            buffer[position] = ((uleft << 4) & 0xF0) | ((right >> 8) & 0x0F);
                            buffer[position + 1] = right & 0xFF;
                        }
                        data_output->position += 3;
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 16:
                        difference = convert_to_signed((buffer[position - 2] << 9) | (buffer[position - 1] << 1) | (buffer[position] & 0x01), 17);
                        if(data_output->is_signed)
                            right = convert_to_signed(sample, 16);
                        else
                            right = convert_to_signed(sample, 16) + 32768;
                        uleft = (DECODE_UTYPE)(right + difference);

                        if(data_output->is_little_endian) {
                            buffer[position - 2] = uleft & 0xFF;
                            buffer[position - 1] = (uleft >> 8) & 0xFF;
                            buffer[position] = right & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                        } else {
                            buffer[position - 2] = (uleft >> 8) & 0xFF;
                            buffer[position - 1] = uleft & 0xFF;
                            buffer[position] = (right >> 8) & 0xFF;
                            buffer[position + 1] = right & 0xFF;
                        }
                        data_output->position += 4;
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 20:
                        difference = convert_to_signed((buffer[position - 2] << 13) | (buffer[position - 1] << 5) | (buffer[position] & 0x1F), 21);
                        if(data_output->is_signed)
                            right = convert_to_signed(sample, 20);
                        else
                            right = convert_to_signed(sample, 20) + 524288;
                        uleft = (DECODE_UTYPE)(right + difference);

                        if(data_output->is_little_endian) {
                            buffer[position - 2] = uleft & 0xFF;
                            buffer[position - 1] = (uleft >> 8) & 0xFF;
                            buffer[position] = ((uleft >> 12) & 0xF0) | ((right >> 4) & 0x0F);
                            buffer[position + 1] = ((right << 4) & 0xF0) | ((right >> 12) & 0x0F);
                            buffer[position + 2] = ((right >> 4) & 0xF0) | ((right >> 16) & 0x0F);
                        } else {
                            buffer[position - 2] = (uleft >> 12) & 0xFF;
                            buffer[position - 1] = (uleft >> 4) & 0xFF;
                            buffer[position] = ((uleft << 4) & 0xF0) | ((right >> 16) & 0x0F);
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = right & 0xFF;
                        }
                        data_output->position += 5;
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 24:
                        difference = convert_to_signed((buffer[position - 3] << 17) | (buffer[position - 2] << 9) | (buffer[position - 1] << 1) | (buffer[position] & 0x01), 25);
                        if(data_output->is_signed)
                            right = convert_to_signed(sample, 24);
                        else
                            right = convert_to_signed(sample, 24) + 8388608;
                        uleft = (DECODE_UTYPE)(right + difference);

                        if(data_output->is_little_endian) {
                            buffer[position - 3] = uleft & 0xFF;
                            buffer[position - 2] = (uleft >> 8) & 0xFF;
                            buffer[position - 1] = (uleft >> 16) & 0xFF;
                            buffer[position] = right & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = (right >> 16) & 0xFF;
                        } else {
                            buffer[position - 3] = (uleft >> 16) & 0xFF;
                            buffer[position - 2] = (uleft >> 8) & 0xFF;
                            buffer[position - 1] = uleft & 0xFF;
                            buffer[position] = (right >> 16) & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = right & 0xFF;
                        }
                        data_output->position += 6;
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 32:
                        difference = convert_to_signed((buffer[position - 4] << 25) | (buffer[position - 3] << 17) | (buffer[position - 2] << 9) | (buffer[position - 1] << 1) | (buffer[position] & 0x01), 33);
                        if(data_output->is_signed)
                            right = convert_to_signed(sample, 32);
                        else
                            right = convert_to_signed(sample, 32) + 2147483648u;
                        uleft = (DECODE_UTYPE)(right + difference);

                        if(data_output->is_little_endian) {
                            buffer[position - 4] = uleft & 0xFF;
                            buffer[position - 3] = (uleft >> 8) & 0xFF;
                            buffer[position - 2] = (uleft >> 16) & 0xFF;
                            buffer[position - 1] = (uleft >> 24) & 0xFF;
                            buffer[position] = right & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = (right >> 16) & 0xFF;
                            buffer[position + 3] = (right >> 24) & 0xFF;
                        } else {
                            buffer[position - 4] = (uleft >> 24) & 0xFF;
                            buffer[position - 3] = (uleft >> 16) & 0xFF;
                            buffer[position - 2] = (uleft >> 8) & 0xFF;
                            buffer[position - 1] = uleft & 0xFF;
                            buffer[position] = (right >> 24) & 0xFF;
                            buffer[position + 1] = (right >> 16) & 0xFF;
                            buffer[position + 2] = (right >> 8) & 0xFF;
                            buffer[position + 3] = right & 0xFF;
                        }
                        data_output->position += 8;
#endif
                }
            }
            break;

        case MID_SIDE:
            if(channel_nb == 0) {
                switch(sample_size) {
#ifdef DECODE_8_BITS
                    case 8:
                        buffer[position] = sample & 0xFF;
                        data_output->position += 2;
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 12:
                        buffer[position] = (sample >> 4) & 0xFF;
                        buffer[position + 1] = sample & 0x0F;
                        data_output->position += 3;
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 16:
                        buffer[position] = (sample >> 8) & 0xFF;
                        buffer[position + 1] = sample & 0xFF;
                        data_output->position += 4;
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 20:
                        buffer[position] = (sample >> 12) & 0xFF;
                        buffer[position + 1] = (sample >> 4) & 0xFF;
                        buffer[position + 2] = sample & 0x0F;
                        data_output->position += 5;
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 24:
                        buffer[position] = (sample >> 16) & 0xFF;
                        buffer[position + 1] = (sample >> 8) & 0xFF;
                        buffer[position + 2] = sample & 0xFF;
                        data_output->position += 6;
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 32:
                        buffer[position] = (sample >> 24) & 0xFF;
                        buffer[position + 1] = (sample >> 16) & 0xFF;
                        buffer[position + 2] = (sample >> 8) & 0xFF;
                        buffer[position + 3] = sample & 0xFF;
                        data_output->position += 8;
#endif
                }
            } else {
                switch(sample_size) {
                    DECODE_TYPE mid = 0;
                    DECODE_TYPE side = 0;
                    DECODE_UTYPE left = 0;
                    DECODE_UTYPE right = 0;

#ifdef DECODE_8_BITS
                    case 9:
                        mid = convert_to_signed(buffer[position - 1], 8);
                        side = convert_to_signed(sample, 9);
                        left = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) + side) >> 1);
                        right = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) - side) >> 1);

                        if(!data_output->is_signed) {
                            left += 128;
                            right += 128;
                        }

                        buffer[position - 1] = left & 0xFF;
                        buffer[position] = right & 0xFF;
                        data_output->position += 2;
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 13:
                        mid = convert_to_signed((buffer[position - 1] << 4) | (buffer[position] & 0x0F), 12);
                        side = convert_to_signed(sample, 13);
                        left = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) + side) >> 1);
                        right = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) - side) >> 1);

                        if(!data_output->is_signed) {
                            left += 2048;
                            right += 2048;
                        }

                        if(data_output->is_little_endian) {
                            buffer[position - 1] = left & 0xFF;
                            buffer[position] = ((left >> 4) & 0xF0) | ((right >> 4) & 0x0F);
                            buffer[position + 1] = ((right << 4) & 0xF0) | ((right >> 8) & 0x0F);
                        } else {
                            buffer[position - 1] = (left >> 4) & 0xFF;
                            buffer[position] = ((left << 4) & 0xF0) | ((right >> 8) & 0x0F);
                            buffer[position + 1] = right & 0xFF;
                        }
                        data_output->position += 3;
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 17:
                        mid = convert_to_signed((buffer[position - 2] << 8) | buffer[position - 1], 16);
                        side = convert_to_signed(sample, 17);
                        left = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) + side) >> 1);
                        right = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) - side) >> 1);

                        if(!data_output->is_signed) {
                            left += 32768;
                            right += 32768;
                        }

                        if(data_output->is_little_endian) {
                            buffer[position - 2] = left & 0xFF;
                            buffer[position - 1] = (left >> 8) & 0xFF;
                            buffer[position] = right & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                        } else {
                            buffer[position - 2] = (left >> 8) & 0xFF;
                            buffer[position - 1] = left & 0xFF;
                            buffer[position] = (right >> 8) & 0xFF;
                            buffer[position + 1] = right & 0xFF;
                        }
                        data_output->position += 4;
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 21:
                        mid = convert_to_signed((buffer[position - 2] << 12) | (buffer[position - 1] << 4) | (buffer[position] & 0x0F), 20);
                        side = convert_to_signed(sample, 21);
                        left = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) + side) >> 1);
                        right = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) - side) >> 1);

                        if(!data_output->is_signed) {
                            left += 524288;
                            right += 524288;
                        }

                        if(data_output->is_little_endian) {
                            buffer[position - 2] = left & 0xFF;
                            buffer[position - 1] = (left >> 8) & 0xFF;
                            buffer[position] = ((left >> 12) & 0xF0) | ((right >> 4) & 0x0F);
                            buffer[position + 1] = ((right << 4) & 0xF0) | ((right >> 12) & 0x0F);
                            buffer[position + 2] = ((right >> 4) & 0xF0) | ((right >> 16) & 0x0F);
                        } else {
                            buffer[position - 2] = (left >> 12) & 0xFF;
                            buffer[position - 1] = (left >> 4) & 0xFF;
                            buffer[position] = ((left << 4) & 0xF0) | ((right >> 16) & 0x0F);
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = right & 0xFF;
                        }
                        data_output->position += 5;
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 25:
                        mid = convert_to_signed((buffer[position - 3] << 16) | (buffer[position - 2] << 8) | buffer[position - 1], 24);
                        side = convert_to_signed(sample, 25);
                        left = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) + side) >> 1);
                        right = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) - side) >> 1);

                        if(!data_output->is_signed) {
                            left += 8388608;
                            right += 8388608;
                        }

                        if(data_output->is_little_endian) {
                            buffer[position - 3] = left & 0xFF;
                            buffer[position - 2] = (left >> 8) & 0xFF;
                            buffer[position - 1] = (left >> 16) & 0xFF;
                            buffer[position] = right & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = (right >> 16) & 0xFF;
                        } else {
                            buffer[position - 3] = (left >> 16) & 0xFF;
                            buffer[position - 2] = (left >> 8) & 0xFF;
                            buffer[position - 1] = left & 0xFF;
                            buffer[position] = (right >> 16) & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = right & 0xFF;
                        }
                        data_output->position +=6;
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 33:
                        mid = convert_to_signed((buffer[position - 4] << 24) | (buffer[position - 3] << 16) | (buffer[position - 2] << 8) | buffer[position - 1], 32);
                        side = convert_to_signed(sample, 33);
                        left = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) + side) >> 1);
                        right = (DECODE_UTYPE)((((mid << 1) | (side & 0x1)) - side) >> 1);

                        if(!data_output->is_signed) {
                            left += 2147483648;
                            right += 2147483648;
                        }

                        if(data_output->is_little_endian) {
                            buffer[position - 4] = left & 0xFF;
                            buffer[position - 3] = (left >> 8) & 0xFF;
                            buffer[position - 2] = (left >> 16) & 0xFF;
                            buffer[position - 1] = (left >> 24) & 0xFF;
                            buffer[position] = right & 0xFF;
                            buffer[position + 1] = (right >> 8) & 0xFF;
                            buffer[position + 2] = (right >> 16) & 0xFF;
                            buffer[position + 3] = (right >> 24) & 0xFF;
                        } else {
                            buffer[position - 4] = (left >> 24) & 0xFF;
                            buffer[position - 3] = (left >> 16) & 0xFF;
                            buffer[position - 2] = (left >> 8) & 0xFF;
                            buffer[position - 1] = left & 0xFF;
                            buffer[position] = (right >> 24) & 0xFF;
                            buffer[position + 1] = (right >> 16) & 0xFF;
                            buffer[position + 2] = (right >> 8) & 0xFF;
                            buffer[position + 3] = right & 0xFF;
                        }
                        data_output->position += 8;
#endif
                }
            }
            break;

        default:
#if defined DECODE_12_BITS || defined DECODE_20_BITS
            if(shift == 0) {
#endif
                switch(sample_size) {
#ifdef DECODE_8_BITS
                    case 8:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 8) + 128);

                        buffer[position] = sample & 0xFF;

                        #ifdef STEREO_ONLY
                        data_output->position += 2;
                        #else
                        data_output->position += (channel_assignement + 1);
                        #endif
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 12:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 12) + 2048);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] &= 0x0F;
                            buffer[position + 1] |= (sample >> 4) & 0xF0;
                        } else {
                            buffer[position] = (sample >> 4) & 0xFF;
                            buffer[position + 1] &= 0x0F;
                            buffer[position + 1] |= (sample << 4) & 0xF0;
                        }

                        #ifdef STEREO_ONLY
                            data_output->position += 3;
                        #else
                        switch(channel_assignement) {
                            case MONO:
                                data_output->position += 1;
                                data_output->shift = 4;
                                break;
                                 /*84 48*/
                            case LEFT_RIGHT:
                                data_output->position += 3;
                                break;
                                 /*84 4 8   8 4*/
                            case LEFT_RIGHT_CENTER:
                                data_output->position += 4;
                                data_output->shift = 4;
                                break;
                                 /*8 4    4 8     8 4    4 8*/
                            case F_LEFT_F_RIGHT_B_LEFT_B_RIGHT:
                                data_output->position += 6;
                                break;
                                 /*8 4    4 8     8 4      4 8    8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_B_LEFT_B_RIGHT:
                                data_output->position += 7;
                                data_output->shift = 4;
                                break;
                                 /*8 4    4 8     8 4    4 8   8 4    4 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT:
                                data_output->position += 9;
                                break;
                                 /*8 4    4 8     8 4    4 8   8 4      4 8    8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_CENTER_S_LEFT_S_RIGHT:
                                data_output->position += 10;
                                data_output->shift = 4;
                                break;
                                 /*8 4    4 8     8 4    4 8   8 4    4 8     8 4    4 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT_S_LEFT_S_RIGHT:
                                data_output->position += 12;
                        }
                        #endif
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 16:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 16) + 32768);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                        } else {
                            buffer[position] = (sample >> 8) & 0xFF;
                            buffer[position + 1] = sample & 0xFF;
                        }

                        #ifdef STEREO_ONLY
                        data_output->position += 4;
                        #else
                        data_output->position += (2 * (channel_assignement + 1));
                        #endif
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 20:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 20) + 524288);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] &= 0x0F;
                            buffer[position + 2] |= (sample >> 12) & 0xF0;
                        } else {
                            buffer[position] = (sample >> 12) & 0xFF;
                            buffer[position + 1] = (sample >> 4) & 0xFF;
                            buffer[position + 2] &= 0x0F;
                            buffer[position + 2] |= (sample << 4) & 0xF0;
                        }

                        #ifdef STEREO_ONLY
                        data_output->position += 5;
                        #else
                        switch(channel_assignement) {
                            case MONO:
                                data_output->position += 2;
                                data_output->shift = 4;
                                break;
                               /*884  488*/
                            case LEFT_RIGHT:
                                data_output->position += 5;
                                break;
                               /*884  488   884*/
                            case LEFT_RIGHT_CENTER:
                                data_output->position += 7;
                                data_output->shift = 4;
                                break;
                                 /*8 8 4  4 8 8   8 8 4  4 8 8*/
                            case F_LEFT_F_RIGHT_B_LEFT_B_RIGHT:
                                data_output->position += 10;
                                break;
                                 /*8 8 4  4 8 8   8 8 4    4 8 8  8 8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_B_LEFT_B_RIGHT:
                                data_output->position += 12;
                                data_output->shift = 4;
                                break;
                                 /*8 8 4  4 8 8   8 8 4  488   8 8 4  4 8 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT:
                                data_output->position += 15;
                                break;
                                 /*8 8 4  4 8 8   8 8 4  488   8 8 4    4 8 8  8 8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_CENTER_S_LEFT_S_RIGHT:
                                data_output->position += 17;
                                data_output->shift = 4;
                                break;
                                 /*8 8 4  4 8 8   8 8 4  488   8 8 4  4 8 8   8 8 4  4 8 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT_S_LEFT_S_RIGHT:
                                data_output->position += 20;
                        }
                        #endif
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 24:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 24) + 8388608);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = (sample >> 16) & 0xFF;
                        } else {
                            buffer[position] = (sample >> 16) & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = sample & 0xFF;
                        }

                        #ifdef STEREO_ONLY
                        data_output->position += 6;
                        #else
                        data_output->position += (3 * (channel_assignement + 1));
                        #endif
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 32:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 32) + 2147483648);

                        if(data_output->is_little_endian) {
                            buffer[position] = sample & 0xFF;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = (sample >> 16) & 0xFF;
                            buffer[position + 3] = (sample >> 24) & 0xFF;
                        } else {
                            buffer[position] = (sample >> 24) & 0xFF;
                            buffer[position + 1] = (sample >> 16) & 0xFF;
                            buffer[position + 2] = (sample >> 8) & 0xFF;
                            buffer[position + 3] = sample & 0xFF;
                        }

                        #ifdef STEREO_ONLY
                        data_output->position += 8;
                        #else
                        data_output->position += (4 * (channel_assignement + 1));
                        #endif
#endif
                }
#if defined DECODE_12_BITS || defined DECODE_20_BITS
            } else {                    /* If the shift is not 0 then it should 4 */
                switch(sample_size) {
#ifdef DECODE_12_BITS
                    case 12:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 12) + 2048);

                        if(data_output->is_little_endian) {
                            buffer[position] &= 0xF0;
                            buffer[position] |= ((sample >> 4) & 0x0F);
                            buffer[position + 1] = ((sample & 0x0F) << 4) | ((sample & 0xF00) >> 8);
                        } else {
                            buffer[position] &= 0xF0;
                            buffer[position] |= (sample >> 8) & 0x0F;
                            buffer[position + 1] = sample & 0xFF;
                        }

                        #ifdef STEREO_ONLY
                        data_output->position += 2;
                        #else
                        switch(channel_assignement) {
                            case MONO:
                                data_output->position += 2;
                                data_output->shift = 0;
                                break;
                                 /*48 84*/
                            case LEFT_RIGHT:
                                data_output->position += 2;
                                break;
                                 /*48 8 4   4 8*/
                            case LEFT_RIGHT_CENTER:
                                data_output->position += 5;
                                data_output->shift = 0;
                                break;
                                 /*4 8    8 4     4 8    8 4*/
                            case F_LEFT_F_RIGHT_B_LEFT_B_RIGHT:
                                data_output->position += 6;
                                break;
                                 /*4 8    8 4     4 8      8 4    4 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_B_LEFT_B_RIGHT:
                                data_output->position += 8;
                                data_output->shift = 0;
                                break;
                                 /*4 8    8 4     4 8    8 4   4 8    8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT:
                                data_output->position += 9;
                                break;
                                 /*4 8    8 4     4 8    8 4   4 8      8 4    4 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_CENTER_S_LEFT_S_RIGHT:
                                data_output->position += 11;
                                data_output->shift = 0;
                                break;
                                 /*4 8    8 4     4 8    8 4   4 8    8 4     4 8    8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT_S_LEFT_S_RIGHT:
                                data_output->position += 12;
                        }
                        #endif
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 20:
                        if(!data_output->is_signed)
                            sample = (DECODE_UTYPE)(convert_to_signed(sample, 20) + 524288);

                        if(data_output->is_little_endian) {
                            buffer[position] &= 0xF0;
                            buffer[position] |= (sample >> 4) & 0x0F;
                            buffer[position + 1] = (sample & 0xF0) | ((sample >> 12) & 0x0F);
                            buffer[position + 2] = ((sample >> 8) & 0xF0) | ((sample >> 16) & 0x0F);
                        } else {
                            buffer[position] &= 0xF0;
                            buffer[position] |= (sample >> 16) & 0x0F;
                            buffer[position + 1] = (sample >> 8) & 0xFF;
                            buffer[position + 2] = sample & 0xFF;
                        }

                        #ifdef STEREO_ONLY
                        data_output->position += 5;
                        #else
                        switch(channel_assignement) {
                            case MONO:
                                data_output->position += 1;
                                data_output->shift = 4;
                                break;
                               /*488  884*/
                            case LEFT_RIGHT:
                                data_output->position += 5;
                                break;
                               /*488  884   488*/
                            case LEFT_RIGHT_CENTER:
                                data_output->position += 8;
                                data_output->shift = 0;
                                break;
                                 /*4 8 8  8 8 4   4 8 8  8 8 4*/
                            case F_LEFT_F_RIGHT_B_LEFT_B_RIGHT:
                                data_output->position += 10;
                                break;
                                 /*4 8 8  8 8 4   4 8 8    8 8 4  4 8 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_B_LEFT_B_RIGHT:
                                data_output->position += 13;
                                data_output->shift = 0;
                                break;
                                 /*4 8 8  8 8 4   4 8 8  884   4 8 8  8 8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT:
                                data_output->position += 15;
                                break;
                                 /*4 8 8  8 8 4   4 8 8  884   4 8 8    8 8 4  4 8 8*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_CENTER_S_LEFT_S_RIGHT:
                                data_output->position += 18;
                                data_output->shift = 0;
                                break;
                                 /*4 8 8  8 8 4   4 8 8  884   4 8 8  8 8 4   4 8 8  8 8 4*/
                            case F_LEFT_F_RIGHT_F_CENTER_LFE_B_LEFT_B_RIGHT_S_LEFT_S_RIGHT:
                                data_output->position += 20;
                        }
                        #endif
#endif
                }

            }
#endif
    }

    return 0;

}
