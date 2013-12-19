/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "decode_flac.h"


/**
 * Read some of the flac stream informations from data_input. The read
 * informations are used to fill the info paramater. Should be at the beginning
 * of the file.
 * @param data_input  The data input for reading the informations.
 * @param stream_info The resulting useful informations are put there.
 * @return Return 0 if successful, -1 else.
 */
static int get_flac_stream_info(data_input_t* data_input, stream_info_t* stream_info) {

    uint8_t* buffer = NULL;
    int position = 0;

    if(should_refill_input_buffer(data_input, 42))
        if(refill_input_buffer_at_least(data_input, 42) == -1)
            return -1;

    buffer = data_input->buffer;
    position = data_input->position;

    if((buffer[position] != 'f') || (buffer[position + 1] != 'L') || (buffer[position + 2] != 'a') || (buffer[position + 3] != 'C')) {
        fprintf(stderr, "Not a flac file\n");
        return -1;
    }

    position += 8;

    stream_info->min_block_size = (buffer[position] << 8) | buffer[position + 1];
    position += 2;

    stream_info->max_block_size = (buffer[position] << 8) | buffer[position + 1];
    position += 2;

    stream_info->min_frame_size = (buffer[position] << 16) | (buffer[position + 1] << 8) | buffer[position + 2];
    position += 3;

    stream_info->max_frame_size = (buffer[position] << 16) | (buffer[position + 1] << 8) | buffer[position + 2];
    position += 3;

    stream_info->sample_rate = (buffer[position] << 12) | (buffer[position + 1] << 4) | (buffer[position + 2] >> 4);
    position += 2;

    stream_info->nb_channels = ((buffer[position] >> 1) & 0x07) + 1;

    stream_info->bits_per_sample = (((buffer[position] & 0x01) << 4) | (buffer[position + 1] >> 4)) + 1;
    position += 1;

#ifndef DISALLOW_64_BITS
    stream_info->nb_samples = (((uint64_t)buffer[position] & 0x0F) << 32) | (buffer[position + 1] << 24) | (buffer[position + 2] << 16) | (buffer[position + 3] << 8) | buffer[position + 4];
#endif

    position += 5;

    stream_info->md5[0] = buffer[position++];
    stream_info->md5[1] = buffer[position++];
    stream_info->md5[2] = buffer[position++];
    stream_info->md5[3] = buffer[position++];
    stream_info->md5[4] = buffer[position++];
    stream_info->md5[5] = buffer[position++];
    stream_info->md5[6] = buffer[position++];
    stream_info->md5[7] = buffer[position++];
    stream_info->md5[8] = buffer[position++];
    stream_info->md5[9] = buffer[position++];
    stream_info->md5[10] = buffer[position++];
    stream_info->md5[11] = buffer[position++];
    stream_info->md5[12] = buffer[position++];
    stream_info->md5[13] = buffer[position++];
    stream_info->md5[14] = buffer[position++];
    stream_info->md5[15] = buffer[position++];

    data_input->position = position;

    return 0;  

}


/**
 * Skip the unnecessary metadata stored in the flac stream.
 * @param input_data The metadata are read from there making the stream go to
 *                   the first frame.
 * @return Return 0 if successful, -1 else.
 */
static int skip_metadata(data_input_t* data_input) {

    uint8_t was_last = 0;

    do {
        uint32_t length = 0;
        uint8_t* buffer = NULL;
        int position = 0;

        if(should_refill_input_buffer(data_input, 4))
            if(refill_input_buffer_at_least(data_input, 4) == -1)
                return -1;

        buffer = data_input->buffer;
        position = data_input->position;

        was_last = buffer[position] >> 7;
        position += 1;
        length = (buffer[position] << 16) | (buffer[position + 1] << 8) | buffer[position + 2];
        data_input->position = position + 3;
        while(length != 0) {
            int int_length = length > INT_MAX ? INT_MAX : length;

            if(should_refill_input_buffer(data_input, int_length)) {
                length -= (data_input->read_size - data_input->position);
                int_length = length > INT_MAX ? INT_MAX : length;
                data_input->position = data_input->read_size;
                if(refill_input_buffer_at_least(data_input, int_length) == -1)
                    return -1;
            } else {
                data_input->position += int_length;
                break;
            }
        }
    } while(!was_last);

    return 0;

}


/**
 * Read a frame header from the flac stream and retain useful informations.
 * @param data_input Frame header informations are read from there.
 * @param stream_info Frame header informations can depend of stream
 *                    informations (sample size & rate.)
 * @param frame_info  Frame header informations are put there.
 * @return Return 1 if successful, 0 if the previous frame was probably the
 *         last cause we hit an EOF or whatever else relevant in this case or
 *         -1 in case of unexpected error.
 */
static int read_frame_header(data_input_t* data_input, stream_info_t* stream_info, frame_info_t* frame_info) {

    uint8_t blocking_strategy = 0;
    uint8_t sample_rate = 0;
    uint8_t* buffer = NULL;
    int position = 0;

    if(should_refill_input_buffer(data_input, 16)) {
        int error_code = refill_input_buffer(data_input);
        if(error_code == -1)
            return -1;
        if(error_code == 0)
            return 0;
    }

    buffer = data_input->buffer;
    position = data_input->position;

    if((buffer[position] != 0xFF) || (buffer[position + 1] != 0xF8)) {
        fprintf(stderr, "Something is wrong with the synchro\n");
        return -1;
    }

    blocking_strategy = buffer[position + 1] & 0x01;
    position +=2;

    frame_info->block_size = buffer[position] >> 4;
    sample_rate = buffer[position] & 0x0F;
    position += 1;

    frame_info->channel_assignement = buffer[position] >> 4;
    frame_info->bits_per_sample = (buffer[position] >> 1) & 0x07;
    position += 1;

    if(frame_info->bits_per_sample == 0) {
        frame_info->bits_per_sample = stream_info->bits_per_sample;
    } else {
        switch(frame_info->bits_per_sample) {
            case 1:
                frame_info->bits_per_sample = 8;
                break;

            case 2:
                frame_info->bits_per_sample = 12;
                break;

            case 4:
                frame_info->bits_per_sample = 16;
                break;

            case 5:
                frame_info->bits_per_sample = 20;
                break;

            case 6:
                frame_info->bits_per_sample = 24;
        }
    }

    if(blocking_strategy) {
#ifndef DISALLOW_64_BITS
        uint64_t sample_nb = 0;
#endif
        uint8_t nb_coding_bytes = 1;

        if((buffer[position] & 0x80) && (buffer[position] & 0x40)) {
            if(buffer[position] & 0x20)
                if(buffer[position] & 0x10)
                    if(buffer[position] & 0x08)
                        if(buffer[position] & 0x04)
                            nb_coding_bytes = 6;
                        else
                            nb_coding_bytes = 5;
                    else
                        nb_coding_bytes = 4;
                else
                    nb_coding_bytes = 3;
            else
                nb_coding_bytes = 2;
        }
        
#ifdef DISALLOW_64_BITS
        position += nb_coding_bytes;
#else
        if(nb_coding_bytes == 1) {
            sample_nb = buffer[position];
            ++position;
        } else {
            sample_nb = buffer[position] & (0xFFu >> nb_coding_bytes);
            ++position;
            --nb_coding_bytes;
            while(nb_coding_bytes) {
                sample_nb <<= 6;
                sample_nb |= buffer[position] & 0x3F;
                ++position;
                --nb_coding_bytes;
            }
        }
#endif

    } else {
        uint32_t frame_nb = 0;
        uint8_t nb_coding_bytes = 1;

        if((buffer[position] & 0x80) && (buffer[position] & 0x40)) {
            if(buffer[position] & 0x20)
                if(buffer[position] & 0x10)
                    if(buffer[position] & 0x08)
                        if(buffer[position] & 0x04)
                            if(buffer[position] & 0x02)
                                nb_coding_bytes = 7;
                            else
                                nb_coding_bytes = 6;
                        else
                            nb_coding_bytes = 5;
                    else
                        nb_coding_bytes = 4;
                else
                    nb_coding_bytes = 3;
            else
                nb_coding_bytes = 2;
        }
        
        if(nb_coding_bytes == 1) {
            frame_nb = buffer[position];
            ++position;
        } else {
            frame_nb = buffer[position] & (0xFFu >> nb_coding_bytes);
            ++position;
            --nb_coding_bytes;
            while(nb_coding_bytes) {
                frame_nb <<= 6;
                frame_nb |= buffer[position] & 0x3F;
                ++position;
                --nb_coding_bytes;
            }
        }
    }

    if(frame_info->block_size == 0x06) {
        frame_info->block_size = buffer[position] + 1;
        position += 1;
    } else if(frame_info->block_size == 0x07) {
        frame_info->block_size = ((buffer[position] << 8) | buffer[position + 1]) + 1;
        position += 2;
    } else {
        switch(frame_info->block_size) {
            case 1:
                frame_info->block_size = 192;
                break;

            case 2:
                frame_info->block_size = 576;
                break;

            case 3:
                frame_info->block_size = 1152;
                break;

            case 4:
                frame_info->block_size = 2304;
                break;

            case 5:
                frame_info->block_size = 4608;
                break;

            case 8:
                frame_info->block_size = 256;
                break;

            case 9:
                frame_info->block_size = 512;
                break;

            case 10:
                frame_info->block_size = 1024;
                break;

            case 11:
                frame_info->block_size = 2048;
                break;

            case 12:
                frame_info->block_size = 4096;
                break;

            case 13:
                frame_info->block_size = 8192;
                break;

            case 14:
                frame_info->block_size = 16384;
                break;

            case 15:
                frame_info->block_size = 32768;
        }
    }

    if(sample_rate == 0x0C)
        position += 1;
    else if((sample_rate == 0x0D) || (sample_rate == 0x0E))
        position += 2;

    data_input->position = position + 1;

    return 1;

}


/**
 * Read a subframe header mainly to get its type and the number of wasted bits
 * per sample if any.
 * @param data_input    Subframe header informations are read from there.
 * @param subframe_info Subframe header informations are put there.
 * @return Return 0 if successful, -1 else.
 */
static int read_subframe_header(data_input_t* data_input, subframe_info_t* subframe_info) {

    int error_code = 0;
    uint8_t tmp = get_shifted_bits(data_input, 8, &error_code);
    subframe_info->type = (tmp >> 1) & 0x3F;
    subframe_info->wasted_bits_per_sample = tmp & 0x01;

    if(subframe_info->wasted_bits_per_sample) {
        uint8_t current_shift = data_input->shift;

        while(((data_input->buffer[data_input->position] & (0x80 >> current_shift)) >> (7 - current_shift)) != 1) {
            ++current_shift;
            ++subframe_info->wasted_bits_per_sample;

            if(current_shift == 8) {
                current_shift = 0;
                data_input->position += 1;

                if(should_refill_input_buffer(data_input, 1)) {
                    if(refill_input_buffer_at_least(data_input, 1)  == -1)
                        return -1;
                }
            }
        }

        data_input->shift = (data_input->shift + subframe_info->wasted_bits_per_sample) % 8;
    }

    return 0;

}


/**
 * Get the next residual coded using Rice codes from the flac stream and return
 * it. Residuals are coded using two Rice codes kind which differ by their
 * parameter size. This function is not reentrant so by careful~
 * @param data_input          Needed data are fetched from there.
 * @param partition_order     Used to compute the number of samples coded in
 *                            each partitions.
 * @param is_first_partition  Indicate if we are in the first partition or not.
 *                            Used to compute the number of samples which
 *                            differ if we are in the first partition.
 * @param rice_parameter_size Can be equal to 4 or 5 which is the number of
 *                            bits making of the rice parameter.
 * @param block_size          The number of sample in a block for the current
 *                            frame.
 * @param predictor_order     Used to compute the number of partitions and thus
 *                            the number of samples per partition.
 * @param error_code          Like its name indicates, it is used to tell the
 *                            caller if an error occured or not. If equal to
 *                            -1, yes else no.
 * @return Return the decoded residual.
 */

static DECODE_TYPE get_next_rice_residual(data_input_t* data_input, uint8_t partition_order, uint8_t *is_first_partition, uint8_t rice_parameter_size, uint16_t block_size, uint8_t predictor_order, int* error_code) {

    static uint16_t remaining_nb_samples = 0;
    static uint8_t rice_parameter = 0;
    static uint8_t has_escape_code = 0;
    static uint8_t escape_bits_per_sample = 0;
    static uint16_t nb_partitions[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
    DECODE_TYPE value = 0;

    if(remaining_nb_samples == 0) {
        rice_parameter = get_shifted_bits(data_input, rice_parameter_size, error_code);
        if(*error_code == -1)
            return 0;

        has_escape_code = ((rice_parameter_size == 4) && (rice_parameter == 0x0F)) || ((rice_parameter_size == 5) && (rice_parameter == 0x1F));

        if(has_escape_code) {
            escape_bits_per_sample = get_shifted_bits(data_input, 5, error_code);
            if(*error_code == -1)
                return 0;
        }

        if(*is_first_partition) {
            *is_first_partition = 0;
            remaining_nb_samples = partition_order == 0 ? block_size - predictor_order : (block_size / nb_partitions[partition_order]) - predictor_order;
        } else {
            remaining_nb_samples = block_size / nb_partitions[partition_order];
        }
    }

    if(has_escape_code) {
        value = convert_to_signed(get_shifted_bits(data_input, escape_bits_per_sample, error_code), escape_bits_per_sample);
        if(*error_code == -1)
            return 0;
    } else {
        DECODE_UTYPE msb = 0;

        while(get_shifted_bits(data_input, 1, error_code) != 1) {
            if(*error_code == -1)
                return 0;
            ++msb;
        }
        if(*error_code == -1)
            return 0;

        if(rice_parameter == 0) {
            if(msb & 0x01)
                value = -(msb >> 1) - 1;
            else
                value = msb >> 1;
        } else {
            DECODE_UTYPE lsb = 0;

            lsb = get_shifted_bits(data_input, rice_parameter, error_code);
            if(*error_code == -1)
                return 0;
            value = (msb << (rice_parameter - 1)) | (lsb >> 1);
            if(lsb & 0x1)
                value = (value * -1) - 1;
        }
    }

    --remaining_nb_samples;
    return value;

}


/**
 * Decode a constant subframe into the data output sink. A constant subframe
 * is a value repeated a number of time equivalent to the associeted number of
 * samples. Usefull for silent in track and enraging ghost tracks.
 * @param data_input The value is read from there.
 * @param data_output     The resulting sample are sent there.
 * @param frame_info      Used to know the number of samples or the current
 *                        channel assignement.
 * @param bits_per_sample The number of bits per sample used in this subframe.
 * @param channel_nb      Indicate which channel in the channel assignement we
 *                        are currently outputing.
 * @return Return 0 if successful, -1 else. 
 */
static uint16_t decode_constant(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    if(!frame_info->subframes_info[channel_nb].has_parameters) {
        frame_info->subframes_info[channel_nb].value = get_shifted_bits(data_input, frame_info->subframes_info[channel_nb].bits_per_sample - frame_info->subframes_info[channel_nb].wasted_bits_per_sample, error_code) << frame_info->subframes_info[channel_nb].wasted_bits_per_sample;
        if(*error_code == -1)
            return 0;

        frame_info->subframes_info[channel_nb].has_parameters = 1;
    }

    for(;crt_sample < frame_info->block_size; ++crt_sample) {
       *error_code = put_shifted_bits(data_output, frame_info->subframes_info[channel_nb].value, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;
        if(*error_code == 0) {
            if((channel_nb + 1) < frame_info->nb_channels) {
                if(frame_info->subframes_info[channel_nb + 1].input_data_position == -1) {
                    frame_info->subframes_info[channel_nb].input_data_position = get_position(data_input);
                    frame_info->subframe_info[channel_nb + 1].input_data_position = frame_info->subframes_info[channel_nb].data_input_position;
                } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                    if(skip_to_position(data_input, frame_info->subframes_info[channel_nb + 1].data_input_position) == -1) {
                        *error_code = -1;
                        return 0;
                    }
                }
            }
            return crt_sample;
        }
    }

    return crt_sample;

}


/**
 * Decode a verbatim subframe into the data output sink. A verbatim subframe
 * is a sequence of raw samples like white noise and whatever random stuff.
 * @param data_input      Samples are read from there.
 * @param data_output     The resulting samples are outputed there.
 * @param frame_info      Provide usefull informations like the number of samples.
 * @param bits_per_sample How many bits make up a sample for the current
 *                        subframe and channel.
 * @param channel_nb      Indicate which channel in the channel assignement we
 *                        are currently outputing.
 * @return Return 0 if successful, -1 else.
 */
static uint16_t decode_verbatim(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    if(crt_sample != 0) {
        *error_code = put_shifted_bits(data_output, frame_info->subframes_info[channel_nb].value, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;
        ++crt_sample;
    }

    for(;crt_sample < frame_info->block_size; ++crt_sample) {
        DECODE_UTYPE value = get_shifted_bits(data_input, frame_info->subframes_info[channel_nb].bits_per_sample - frame_info->subframes_info[channel_nb].wasted_bits_per_sample, error_code) << frame_info->subframes_info[channel_nb].wasted_bits_per_sample;
        if(*error_code == -1)
            return -1;

        *error_code = put_shifted_bits(data_output, value, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;
        if(*error_code == 0) {
            frame_info->subframes_info[channel_nb].value = value;
            frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);
            if((channel_nb + 1) < frame_info->nb_channels) {
                if(frame_info->subframe_info[channel_nb + 1].data_input_position == -1) {
                    if(skip_nb_bytes(data_input, (bits_per_sample - frame_info->subframes_info[channel_nb].wasted_bits_per_sample) * (frame_info->block_size - (crt_sample + 1))) == -1) {
                        *error_code = -1;
                        return 0;
                    }
                    frame_info->subframes_info[channel_nb + 1].data_input_position = get_position(data_input);
                } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                    if(skip_to_position(data_input, frame_info->subframe_info[channel_nb + 1].data_input_position) == -1) {
                        *error_code = -1;
                        return 0;
                    }
                }
            }
            return crt_sample;
        }
    }

    return crt_sample;

}


/**
 * Decode a fixed subframe into the data output sink. In a fixed subframe,
 * samples are encoded using a fixed linear predictor of zero to fourth order.
 * @param data_input      Warm-up samples and residuals are read from there.
 * @param data_output     The decoded samples are outputed there.
 * @param frame_info      Provide usefull information like the number of samples.
 * @param subframe_info   Provide information about the fixed subframe being
 *                        decoded.
 * @param bits_per_sample How many bits make up a sample for the current
 *                        subframe and channel.
 * @param channel_nb      Indicate which channel in the channel assignement we
 *                        are currently outputing.
 * @return Return 0 if successful, -1 else.
 */
static uint16_t decode_fixed(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    uint8_t order = subframe_info->type - 8;
    struct rice_coding_info_t residual_info = {0};
    uint8_t should_skip_output = 0;
    uint16_t saved_crt_sample = 0;

    if(order == 0) {
        if(!frame_info->subframes_info[channel_nb].has_parameters) {
            uint8_t residual_coding_method = get_shifted_bits(data_input, 2, error_code);
            if(*error_code == -1)
                return 0;

            switch(residual_coding_method) {
                case 0:
                    frame_info->subframes_info[channel_nb].residual_info.rice_parameter_size = 4;
                    break;

                case 1:
                    frame_info->subframes_info[channel_nb].residual_info.rice_parameter_size = 5;
                    break;

                default:
                    *error_code = -1;
                    return 0;
            }

            residual_info.is_first_partition = 1;
            residual_info.partition_order = get_shifted_bits(data_input, 4, error_code);
            if(*error_code == -1)
                return 0;

            frame_info->subframes_info[channel_nb].has_parameters = 1;
        }

        if(crt_sample != 0) {
            *error_code = put_shifted_bits(data_output, frame_info->subframes_info[channel_nb].value, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
            if(*error_code == -1)
                return 0;
            ++crt_sample;
        }

        for(; crt_sample < frame_info->block_size; ++crt_sample) {
            DECODE_TYPE value = get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code) << subframe_info->wasted_bits_per_sample;
            if(*error_code == -1)
                return 0;

            if(!should_skip_output) {
                *error_code = put_shifted_bits(data_output, value, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;
                if(*error_code == 0) {
                    frame_info->subframes_info[channel_nb].value = value;
                    frame_info->subframes_info[channel_nb].residual_info = residual_info;
                    frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);
                    if((channel_nb + 1) < frame_info->nb_channels) {
                        if(frame_info->subframes_info[channel_nb + 1].data_input_position == -1) {
                            should_skip_output = 1;
                            saved_crt_sample = crt_sample;
                        } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                            if(skip_to_position(data_input, frame_info->subframes_info[channel_nb + 1].data_input) == -1) {
                                *error_code = -1;
                                return 0;
                            }
                            return crt_sample;
                        }
                    } else {
                        return crt_sample;
                    }
                }
            }
        }

        if(should_skip_output) {
            frame_info->subframes_info[channel_nb + 1].data_input_position = get_position(data_input);
            return saved_crt_sample;
        }
    } else if(order == 1) {
        DECODE_TYPE previous_sample = 0;

        if(!frame_info->subframes_info[channel_nb].has_parameters) {
            unint8_t residual_coding_method = 0;

            previous_sample = convert_to_signed(get_shifted_bits(data_input, bits_per_sample - subframe_info->wasted_bits_per_sample, error_code), bits_per_sample - subframe_info->wasted_bits_per_sample);
            if(*error_code == -1)
                return 0;

            residual_coding_method = get_shifted_bits(data_input, 2, error_code);
            if(*error_code == -1)
                return 0;
            switch(residual_coding_method) {
                case 0:
                    residual_info.rice_parameter_size = 4;
                    break;

                case 1:
                    residual_info.rice_parameter_size = 5;
                    break;

                default:
                    *error_code = -1;
                    return 0;
            }

            residual_info.is_first_partition = 1;
            residual_info.partition_order = get_shifted_bits(data_input, 4, error_code);
            if(*error_code == -1)
                return 0;

            frame_info->subframe_info[channel_nb].has_parameters = 1;
        } else {
            previous_sample = frame_info->subframes_info[channel_nb].previous_values[0].value;
        }

        *error_code = put_shifted_bits(data_output, previous_sample << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;
        if(*error_code == 0) {
            frame_info->subframes_info[channel_nb].previous_values[0].value = previous_sample;
            frame_info->subframes_info[channel_nb].residual_info = residual_info;
            frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);

            if((channel_nb + 1) < frame_info->nb_channels) {
                if(frame_info->subframes[channel_nb + 1].data_input_position == -1) {
                    should_skip_output = 1;
                    saved_crt_sample = crt_sample;
                } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                    if(skip_to_position(data_input, frame->subframes_info[channel_nb].data_input_position) == -1) {
                        *error_code = -1;
                        return 0;
                    }
                    return crt_sample;
                }
            } else {
                return crt_sample;
            }
        }

        ++crt_sample;

        for(;crt_sample < frame_info->block_size; ++crt_sample) {
            DECODE_TYPE value = get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);
            if(*error_code == -1)
                return 0;

            if(!should_skip_output) {
                value = previous_sample + value;

                *error_code = put_shifted_bits(data_output, value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;
                if(*error_code == 0) {
                    frame_info->subframes_info[channel_nb].previous_values[0].value = value;
                    frame_info->subframes_info[channel_nb].residual_info = residual_info;
                    frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);

                    if((channel_nb + 1) < frame_info->nb_channels) {
                        if(frame_info->subframes[channel_nb + 1].data_input_position == -1) {
                            should_skip_output = 1;
                            saved_crt_sample = crt_sample;
                        } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                            if(skip_to_position(data_input, frame->subframes_info[channel_nb].data_input_position) == -1) {
                                *error_code = -1;
                                return 0;
                            }
                            return crt_sample;
                        }
                    } else {
                        return crt_sample;
                    }
                }

                previous_sample = value;
            }
        }

        if(should_skip_output) {
            frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);
            return saved_crt_sample;
        }
    } else {
        if(!frame_info->subframes_info[channel_nb].has_parameters) {
            uint8_t i = 0;
            uint8_t residual_coding_method = 0;

            for(; i < order; ++i) {
                frame_info->subframes_info[channel_nb].previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, frame_info->subframes_info[channel_nb].bits_per_sample - frame_info->subframes_info[channel_nb].wasted_bits_per_sample, error_code), frame_info->subframes_info[channel_nb].bits_per_sample - frame_info->subframes_info[channel_nb].wasted_bits_per_sample);
                if(*error_code == -1)
                    return -1;

                frame_info->subframes_info[channel_nb].previous_values[i].next = frame_info->subframes_info[channel_nb].previous_values + i + 1;
            }
            frame_info->subframes_info[channel_nb].previous_values[order - 1].next = frame_info->subframes_info[channel_nb].previous_values;

            residual_coding_method = get_shifted_bits(data_input, 2, error_code);
            if(*error_code == -1)
                return 0;
            switch(residual_coding_method) {
                case 0:
                    frame_info->subframes_info[channel_nb].residual_info.rice_parameter_size = 4;
                    break;

                case 1:
                    frame_info->subframes_info[channel_nb].residual_info.rice_parameter_size = 5;
                    break;

                default:
                    *error_code = -1;
                    return 0;
            }

            frame_info->subframes_info[channel_nb].residual_info.is_first_partition = 1;
            frame_info->subframes_info[channel_nb].residual_info.partition_order = get_shifted_bits(data_input, 4, error_code);
            if(*error_code == -1)
                return 0;

            frame_info->subframes_info[channel_nb].has_parameters = 1;
        }

        if(crt_sample < order) {
            for(; crt_sample < order; ++crt_sample) {
                if(!should_skip_output) {
                    *error_code = put_shifted_bits(data_output, frame_info->subframes_info[channel_nb].previous_values[crt_sample].value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
                    if(*error_code == -1)
                        return 0;
                    if(*error_code == 0) {
                        frame_info->subframes_info[channel_nb].residual_info = residual_info;
                        frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);

                        if((channel_nb + 1) < frame_info->nb_channels) {
                            if(frame_info->subframes[channel_nb + 1].data_input_position == -1) {
                                should_skip_output = 1;
                                saved_crt_sample = crt_sample;
                            } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                                if(skip_to_position(data_input, frame->subframes_info[channel_nb].data_input_position) == -1) {
                                    *error_code = -1;
                                    return 0;
                                }
                                return crt_sample;
                            }
                        } else {
                            return crt_sample;
                        }
                    }
                }
            }
        } else {
            *error_code = put_shifted_bits(data_output, frame_info->subframes_info[channel_nb].next_out->value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
            if(*error_code == -1)
                return 0;

            ++crt_sample;
            frame_info->subframes_info[channel_nb].next_out = frame_info->subframes_info[channel_nb].next_out->next;
        }

        for(;crt_sample < frame_info->block_size; ++crt_sample) {
            DECODE_TYPE value = get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);
            if(*error_code == -1)
                return 0;

            if(!should_skip_output) {
                switch(order) {
                    case 2:
                        value = (frame_info->subframes_info[channel_nb].next_out->next->value * 2) - frame_info->subframes_info[channel_nb].next_out->value + value;
                        break;

                    case 3:
                        value = (frame_info->subframes_info[channel_nb].next_out->next->next->value * 3) - (frame_info->subframes_info[channel_nb].next_out->next->value * 3) + frame_info->subframes_info[channel_nb].next_out->value + value;
                        break;

                    case 4:
                        value = (frame_info->subframes_info[channel_nb].next_out->next->next->next->value * 4) - (frame_info->subframes_info[channel_nb].next_out->next->next->value * 6) + (frame_info->subframes_info[channel_nb].next_out->next->value * 4) - frame_info->subframes_info[channel_nb].next_out->value + value;
                }

                *error_code = put_shifted_bits(data_output, value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;

                frame_info->subframes_info[channel_nb].next_out->value = value;

                if(*error_code == 0) {
                    frame_info->subframes_info[channel_nb].residual_info = residual_info;
                    frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);

                    if((channel_nb + 1) < frame_info->nb_channels) {
                        if(frame_info->subframes[channel_nb + 1].data_input_position == -1) {
                            should_skip_output = 1;
                            saved_crt_sample = crt_sample;
                        } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                            if(skip_to_position(data_input, frame->subframes_info[channel_nb].data_input_position) == -1) {
                                *error_code = -1;
                                return 0;
                            }
                            return crt_sample;
                        }
                    } else {
                        return crt_sample;
                    }
                } else {
                    frame_info->subframes_info[channel_nb].next_out = frame_info->subframes_info[channel_nb].next_out->next;
                }
            }
        }

        if(should_skip_output) {
            frame_info->subframes_info[channel_nb + 1].data_input_position = get_position(data_input);
            return saved_crt_sample;
        }
    }

    return crt_sample;

}


/**
 * Decode a LPC subframe into the data output sink. In a LPC subframe, samples
 * are encoded using FIR linear prediction of one to thirty-second order.
 * @param data_input      Parameters, warm-up samples and residuals are read
 *                        from there.
 * @param data_output     The decoded samples are outputed there.
 * @param frame_info      Provide usefull informations like the number of
 *                        samples.
 * @param subframe_info   Provide information about the fixed subframe being
 *                        decoded.
 * @param bits_per_sample How many bits make up a sample for the current
 *                        subframe and channel.
 * @param channel_nb      Indicate which channel in the channel assignement we
 *                        are currently outputing.
 * @return Return 0 if successful, -1 else.
 */
static uint16_t decode_lpc(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    uint8_t order = (subframe_info->type & 0x1F) + 1;
    uint8_t i = 0;
    static uint32_t dividers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
    struct rice_coding_info_t residual_info;
    uint8_t should_skip_output = 0;
    uint16_t saved_crt_sample = 0;

    if(!frame_info->subframes_info[channel_nb].has_parameters) {
        uint8_t residual_coding_method = 0;

        for(; i < order; ++i) {
            frame_info->subframes_info[channel_nb].previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, frame_info->subframes_info[channel_nb].bits_per_sample - frame_info->subframes_info[channel_nb].wasted_bits_per_sample, error_code), frame_info->subframes_info[channel_nb].bits_per_sample - frame_info->subframes_info[channel_nb].wasted_bits_per_sample);
            if(*error_code == -1)
                return -1;

            frame_info->subframes_info[channel_nb].previous_values[i].next = frame_info->subframes_info[channel_nb].previous_values + i + 1;
        }
        frame_info->subframes_info[channel_nb].previous_values[order - 1].next = frame_info->subframes_info[channel_nb].previous_values;

        frame_info->subframes_info[channel_nb].lpc_precision = get_shifted_bits(data_input, 4, error_code) + 1;
        if(*error_code == -1)
            return 0;

        frame_info->subframes_info[channel_nb].lpc_shift = convert_to_signed(get_shifted_bits(data_input, 5, error_code), 5);
        if(*error_code == -1)
            return 0;

        for(i = 0; i < order; ++i) {
            frame_info->subframes_info[channel_nb].coeffs[i] = convert_to_signed(get_shifted_bits(data_input, frame_info->subframes_info[channel_nb].lpc_precision, error_code), frame_info->subframes_info[channel_nb].lpc_precision);
            if(*error_code == -1)
                return 0;
        }

        residual_coding_method = get_shifted_bits(data_input, 2, error_code);
        if(*error_code == -1)
            return 0;
        switch(residual_coding_method) {
            case 0:
                residual_info.rice_parameter_size = 4;
                break;

            case 1:
                residual_info.rice_parameter_size = 5;
                break;

            default:
                *error_code = -1;
                return 0;
        }

        residual_info.is_first_partition = 1;
        residual_info.partition_order = get_shifted_bits(data_input, 4, error_code);
        if(*error_code == -1)
            return 0;

        frame_info->subframes_info[channel_nb].has_parameters = 1;
    }

    if(crt_sample < order) {
        for(; crt_sample < order; ++crt_sample) {
            if(!should_skip_output) {
                *error_code = put_shifted_bits(data_output, frame_info->subframes_info[channel_nb].previous_values[crt_sample].value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;
                if(*error_code == 0) {
                    frame_info->subframes_info[channel_nb].residual_info = residual_info;
                    frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);

                    if((channel_nb + 1) < frame_info->nb_channels) {
                        if(frame_info->subframes[channel_nb + 1].data_input_position == -1) {
                            should_skip_output = 1;
                            saved_crt_sample = crt_sample;
                        } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                            if(skip_to_position(data_input, frame->subframes_info[channel_nb].data_input_position) == -1) {
                                *error_code = -1;
                                return 0;
                            }
                            return crt_sample;
                        }
                    } else {
                        return crt_sample;
                    }
                }
            }
        }
    } else {
        *error_code = put_shifted_bits(data_output, frame_info->subframes_info[channel_nb].next_out->value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;

        ++crt_sample;
        frame_info->subframes_info[channel_nb].next_out = frame_info->subframes_info[channel_nb].next_out->next;
    }

    if(order == 1) {
        for(; crt_sample < frame_info->block_size; ++crt_sample) {
            DECODE_TYPE value = 0;
            if(!should_skip_output) {
                value = frame_info->subframes_info[channel_nb].coeffs[0] * frame_info->subframes_info[channel_nb].previous_values[0].value;

                if(frame_info->subframes_info[channel_nb].lpc_shift < 0)
                    value = value << (uint8_t)(-frame_info->subframes_info[channel_nb].lpc_shift);
                else {
                    if(value < 0)
                        value = -(((-value) + (dividers[frame_info->subframes_info[channel_nb].lpc_shift] - 1)) >> frame_info->subframes_info[channel_nb].lpc_shift);
                    else
                        value = value >> frame_info->subframes_info[channel_nb].lpc_shift;
                }
            }

            value += get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);
            if(*error_code == -1)
                return 0;

            if(!should_skip_output) {
                *error_code = put_shifted_bits(data_output, value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return -1;

                frame_info->subframes_info[channel_nb].previous_values[0].value = value;

                if(*error_code == 0) {
                    frame_info->subframes_info[channel_nb].residual_info = residual_info;
                    frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);

                    if((channel_nb + 1) < frame_info->nb_channels) {
                        if(frame_info->subframes[channel_nb + 1].data_input_position == -1) {
                            should_skip_output = 1;
                            saved_crt_sample = crt_sample;
                        } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                            if(skip_to_position(data_input, frame->subframes_info[channel_nb].data_input_position) == -1) {
                                *error_code = -1;
                                return 0;
                            }
                            return crt_sample;
                        }
                    } else {
                        return crt_sample;
                    }
                }
            }
        }
    } else {
        for(; crt_sample < frame_info->block_size; ++crt_sample) {
            DECODE_TYPE value = 0;
            if(!should_skip_output) {
                previous_value_t* crt = frame_info->subframes_info[channel_nb].next_out;
                for(i = 0; i < order; ++i) {
                    value += frame_info->subframes_info[channel_nb].coeffs[order - i - 1] * crt->value;
                        crt = crt->next;
                }

                if(frame_info->subframes_info[channel_nb].lpc_shift < 0)
                    value = value << (uint8_t)(-frame_info->subframes_info[channel_nb].lpc_shift);
                else {
                    if(value < 0)
                        value = -(((-value) + (dividers[frame_info->subframes_info[channel_nb].lpc_shift] - 1)) >> frame_info->subframes_info[channel_nb].lpc_shift);
                    else
                        value = value >> frame_info->subframes_info[channel_nb].lpc_shift;
                }
            }

            value += get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);
            if(*error_code == -1)
                return 0;

            if(!should_skip_output) {
                *error_code = put_shifted_bits(data_output, value << frame_info->subframes_info[channel_nb].wasted_bits_per_sample, frame_info->subframes_info[channel_nb].bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;

                frame_info->subframes_info[channel_nb].next_out->value = value;

                if(*error_code == 0) {
                    frame_info->subframes_info[channel_nb].residual_info = residual_info;
                    frame_info->subframes_info[channel_nb].data_input_position = get_position(data_input);

                    if((channel_nb + 1) < frame_info->nb_channels) {
                        if(frame_info->subframes[channel_nb + 1].data_input_position == -1) {
                            should_skip_output = 1;
                            saved_crt_sample = crt_sample;
                        } else if(frame_info->subframes_info[channel_nb + 1].type != SUBFRAME_CONSTANT) {
                            if(skip_to_position(data_input, frame->subframes_info[channel_nb].data_input_position) == -1) {
                                *error_code = -1;
                                return 0;
                            }
                            return crt_sample;
                        }
                    } else {
                        return crt_sample;
                    }
                } else {
                    frame_info->subframes_info[channel_nb].next_out = frame_info->subframes_info[channel_nb].next_out->next;
                }
            }
        }
    }

    if(should_skip_output) {
        frame_info->subframes_info[channel_nb + 1].data_input_position = get_position(data_input);
        return saved_crt_sample;
    }

    return crt_sample;

}


/**
 * Decode a subframe into the data output sink. A subframe can be constant,
 * verbatim, fixed or LPC.
 * @param data_input    Parameters, warm-up samples and residuals are read from
 *                      there.
 * @param data_output   The decoded samples are outputed there.
 * @param frame_info    Provide usefull informations like the number of
 *                      samples.
 * @param subframe_info Provide the type of subframe to decode.
 * @param channel_nb    Indicate which channel in the channel assignement we
 *                      are currently outputing.
 * @return Return 0 if successful, -1 else.
 */
static uint16_t decode_subframe_data(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    if(subframe_info->type == SUBFRAME_CONSTANT)
        return decode_constant(data_input, data_output, frame_info, channel_nb, crt_sample, error_code);
    else if(subframe_info->type == SUBFRAME_VERBATIM)
        return decode_verbatim(data_input, data_output, frame_info, channel_nb, crt_sample, error_code);
    else if((SUBFRAME_FIXED_LOW <= subframe_info->type) && (subframe_info->type <= SUBFRAME_FIXED_HIGH))
        return decode_fixed(data_input, data_output, frame_info, channel_nb, crt_sample, error_code);
    else if((SUBFRAME_LPC_LOW <= subframe_info->type) && (subframe_info->type <= SUBFRAME_LPC_HIGH))
        return decode_lpc(data_input, data_output, frame_info, channel_nb, crt_sample, error_code);
            return -1;

    fprintf(stderr, "Invalid subframe type\n");
    *error_code = -1;

    return 0;

}


/**
 * Decode and entire frame and send the decoded samples to the output sink.
 * The decoded frame consists of a frame header and one or more couple of
 * subframe headers and data.
 * @param data_input  Parameters, warm-up samples and residuals are read from
 *                    there.
 * @param data_output The decoded samples are outputed there.
 * @param stream_info Useful for the decoding the frame header.
 * @return Return 1 if successful, 0 if the previous frame was probably the
 *         last cause we hit an EOF or whatever else relevant in this case or
 *         -1 in case of unexpected error.
 */
static int decode_frame(data_input_t* data_input, data_output_t* data_output, stream_info_t* stream_info) {

    int error_code = 0;
    uint8_t channel_nb = 0;
    frame_info_t frame_info;
    int nb_bits_to_write = 0;
    #ifdef STEREO_ONLY
    uint16_t crt_samples[2] = {0};
    #else
    uint16_t crt_samples[8] = {0};
    #endif

    error_code = read_frame_header(data_input, stream_info, &frame_info);
    if(error_code == -1)
        return -1;
    if(error_code == 0)
        return 0;

    #ifdef STEREO_ONLY
    if((frame_info.channel_assignement != LEFT_RIGHT) && (frame_info.channel_assignement != LEFT_SIDE) && (frame_info.channel_assignement != RIGHT_SIDE) && (frame_info.channel_assignement != MID_SIDE)) {
        fprintf(stderr, "Stereo only is supported\n");
        return -1;
    }
    #endif

    data_output->starting_position = data_output->position;
    data_output->starting_shift = data_output->shift;

    for(; channel_nb < stream_info->nb_channels; ++channel_nb) {
        frame_info.subframes_info[channel_nb].has_parameters = 0;
        frame_info.subframes_info[channel_nb].data_input_position = -1;

        if(read_subframe_header(data_input, frame_info.subframe_info + channel_nb) == -1)
            return -1;

        if((((frame_info->channel_assignement == LEFT_SIDE) || (frame_info->channel_assignement == MID_SIDE)) && (channel_nb == 1)) || ((frame_info->channel_assignement == RIGHT_SIDE) && (channel_nb == 0)))
            frame_info.subframes_info[channel_nb].bits_per_sample = frame_info.bits_per_sample + 1;
        else
            frame_info.subframes_info[channel_nb].bits_per_sample = frame_info.bits_per_sample;
    }

    do {
        for(channel_nb = 0; channel_nb < stream_info->nb_channels; ++channel_nb) {
            crt_samples[channel_nb] = decode_subframe_data(data_input, data_output, &frame_info, channel_nb, crt_samples[channel_nb], &error_code)
            if(error_code == -1)
                return -1;

            if((channel_nb + 1) < stream_info->nb_channels) {
                switch(frame_info.bits_per_sample) {
#ifdef DECODE_8_BITS 
                    case 8:
                        data_output->position = data_output->starting_position + (channel_nb + 1);
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 12:
                        data_output->position = data_output->starting_position + (channel_nb + 1);
                        if(data_output->shift == 4) {
                            data_output->shift = 0;
                            ++data_output->position;
                        } else {
                            data_output->shift = 4;
                        }
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 16:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 2);
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 20:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 2);
                        if(data_output->shift == 4) {
                            data_output->shift = 0;
                            ++data_output->position;
                        } else {
                            data_output->shift = 4;
                        }
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 24:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 3);
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 32:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 4);
#endif
                }
            } else {
                switch(frame_info.bits_per_sample) {
#ifdef DECODE_8_BITS 
                    case 8:
                        data_output->position = data_output->starting_position + (crt_samples[channel_nb] * frame_info.nb_channels);
                        break;
#endif
#ifdef DECODE_12_BITS
                    case 12:
                        data_output->position = data_output->starting_position + (crt_samples[channel_nb] * frame_info.nb_channels);
                        if(data_output->shift == 4) {
                            data_output->shift = 0;
                            ++data_output->position;
                        } else {
                            data_output->shift = 4;
                        }
                        break;
#endif
#ifdef DECODE_16_BITS
                    case 16:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 2);
                        break;
#endif
#ifdef DECODE_20_BITS
                    case 20:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 2);
                        if(data_output->shift == 4) {
                            data_output->shift = 0;
                            ++data_output->position;
                        } else {
                            data_output->shift = 4;
                        }
                        break;
#endif
#ifdef DECODE_24_BITS
                    case 24:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 3);
                        break;
#endif
#ifdef DECODE_32_BITS
                    case 32:
                        data_output->position = data_output->starting_position + ((channel_nb + 1) * 4);
#endif
                }
            }
        }
    }while();

    if(data_input->shift != 0) {
        data_input->shift = 0;
        ++data_input->position;
    }

    /* frame footer */
    get_shifted_bits(data_input, 16, &error_code);
    if(error_code == -1)
        return -1;

    nb_bits_to_write = frame_info.block_size * stream_info->nb_channels * frame_info.bits_per_sample;
    data_output->position = data_output->starting_position + (nb_bits_to_write / 8);
    data_output->shift = data_output->starting_shift;
    if(nb_bits_to_write % 8) {
        if(data_output->shift == 4) {
            data_output->shift = 0;
            data_output->position += 1;
        } else
            data_output->shift = 4;
    }

    return 1;

}


/**
 * Decode the flac metedata stream info and skip the others.
 * @param data_input  The metadata are read from there.
 * @param stream_info The resulting useful informations are put there.
 * @return Return 0 if successful, -1 else.
 */
int decode_flac_metadata(data_input_t* data_input, stream_info_t* stream_info) {

    if(get_flac_stream_info(data_input, stream_info) == -1)
        return -1;

    if(skip_metadata(data_input) == -1)
        return -1;

    return 0;

}


/**
 * Decode flac stream into the output sink until the end is reached.
 * @param data_input  The stream is read from there.
 * @param data_output The decoded samples are outputed there.
 * @param stream_info Useful for the decoding frame headers.
 * @return Return 0 if successful, -1 else.
 */
int decode_flac_data(data_input_t* data_input, data_output_t* data_output, stream_info_t* stream_info) {

    int error_code = 0;

    

    while((error_code = decode_frame(data_input, data_output, stream_info)) > 0)
    if(error_code == -1)
        return -1;

    return 0;

}
