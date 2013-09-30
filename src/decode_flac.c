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

    stream_info->nb_samples = (((uint64_t)buffer[position] & 0x0F) << 32) | (buffer[position + 1] << 24) | (buffer[position + 2] << 16) | (buffer[position + 3] << 8) | buffer[position + 4];

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
        uint64_t sample_nb = 0;
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
static int64_t get_next_rice_residual(data_input_t* data_input, uint8_t partition_order, uint8_t *is_first_partition, uint8_t rice_parameter_size, uint16_t block_size, uint8_t predictor_order, int* error_code) {

    static uint16_t remaining_nb_samples = 0;
    static uint8_t rice_parameter = 0;
    static uint8_t has_escape_code = 0;
    static uint8_t escape_bits_per_sample = 0;
    static uint16_t nb_partitions[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
    int64_t value = 0;

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
        uint64_t msb = 0;

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
            uint32_t lsb = 0;

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
static int decode_constant(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, subframe_info_t* subframe_info, uint8_t bits_per_sample, uint8_t channel_nb) {

    uint16_t crt_sample = 0;
    uint64_t value = 0;
    int error_code = 0;

    value = get_shifted_bits(data_input, bits_per_sample - subframe_info->wasted_bits_per_sample, &error_code);
    if(error_code == -1)
        return -1;

    for(;crt_sample < frame_info->block_size; ++crt_sample)
       if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
            return -1;

    return 0;

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
static int decode_verbatim(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, subframe_info_t* subframe_info, uint8_t bits_per_sample, uint8_t channel_nb) {

    uint16_t crt_sample = 0;
    int error_code = 0;

    for(;crt_sample < frame_info->block_size; ++crt_sample) {
        uint64_t value = get_shifted_bits(data_input, bits_per_sample - subframe_info->wasted_bits_per_sample, &error_code);
        if(error_code == -1)
            return -1;

        if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
            return -1;
    }

    return 0;

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
static int decode_fixed(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, subframe_info_t* subframe_info, uint8_t bits_per_sample, uint8_t channel_nb) {

    int error_code = 0;
    uint8_t order = subframe_info->type - 8;
    uint8_t residual_coding_method = 0;
    uint8_t partition_order = 0;        /* for rice coding */ 
    uint8_t is_first_partition = 1;     /* for rice coding */
    uint8_t rice_parameter_size = 0;    /* for rice coding */

    if(order == 0) {
        uint16_t crt_sample = 0;
        residual_coding_method = get_shifted_bits(data_input, 2, &error_code);
        if(error_code == -1)
            return -1;
        switch(residual_coding_method) {
            case 0:
                rice_parameter_size = 4;
                break;

            case 1:
                rice_parameter_size = 5;
                break;

            default:
                return -1;
        }

        is_first_partition = 1;
        partition_order = get_shifted_bits(data_input, 4, &error_code);
        if(error_code == -1)
            return -1;

        for(; crt_sample < frame_info->block_size; ++crt_sample) {
            int64_t value = get_next_rice_residual(data_input, partition_order, &is_first_partition, rice_parameter_size, frame_info->block_size, order, &error_code);

            if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                return -1;
        }
    } else if(order == 1) {
        uint16_t crt_sample = 1;
        int64_t previous_sample = convert_to_signed(get_shifted_bits(data_input, bits_per_sample - subframe_info->wasted_bits_per_sample, &error_code), bits_per_sample - subframe_info->wasted_bits_per_sample);
        if(error_code == -1)
            return -1;

        if(put_shifted_bits(data_output, previous_sample << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
            return -1;

        residual_coding_method = get_shifted_bits(data_input, 2, &error_code);
        if(error_code == -1)
            return -1;
        switch(residual_coding_method) {
            case 0:
                rice_parameter_size = 4;
                break;

            case 1:
                rice_parameter_size = 5;
                break;

            default:
                return -1;
        }

        is_first_partition = 1;
        partition_order = get_shifted_bits(data_input, 4, &error_code);
        if(error_code == -1)
            return -1;

        for(;crt_sample < frame_info->block_size; ++crt_sample) {
            int64_t value = get_next_rice_residual(data_input, partition_order, &is_first_partition, rice_parameter_size, frame_info->block_size, order, &error_code);

            value = previous_sample + value;

            if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                return -1;

            previous_sample = value;
        }
    } else {
        uint8_t i = 0;
        uint16_t crt_sample = order;
        typedef struct warm_up_t {int64_t value; struct warm_up_t* next;} warm_up_t;
        warm_up_t warm_ups[4];
        warm_up_t* next_out = warm_ups;

        for(; i < order; ++i) {
            warm_ups[i].value = convert_to_signed(get_shifted_bits(data_input, bits_per_sample - subframe_info->wasted_bits_per_sample, &error_code), bits_per_sample - subframe_info->wasted_bits_per_sample);
            if(error_code == -1)
                return -1;

            if(put_shifted_bits(data_output, warm_ups[i].value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                return -1;

            warm_ups[i].next = warm_ups + i + 1;
        }
        warm_ups[order - 1].next = warm_ups;

        residual_coding_method = get_shifted_bits(data_input, 2, &error_code);
        if(error_code == -1)
            return -1;
        switch(residual_coding_method) {
            case 0:
                rice_parameter_size = 4;
                break;

            case 1:
                rice_parameter_size = 5;
                break;

            default:
                return -1;
        }

        is_first_partition = 1;
        partition_order = get_shifted_bits(data_input, 4, &error_code);
        if(error_code == -1)
            return -1;

        switch(order) {
            case 2:
                for(;crt_sample < frame_info->block_size; ++crt_sample) {
                    int64_t value = get_next_rice_residual(data_input, partition_order, &is_first_partition, rice_parameter_size, frame_info->block_size, order, &error_code);

                    value = (next_out->next->value * 2) - next_out->value + value;
                    if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                        return -1;

                    next_out->value = value;
                    next_out = next_out->next;
                }
                break;

            case 3:
                for(;crt_sample < frame_info->block_size; ++crt_sample) {
                    int64_t value = get_next_rice_residual(data_input, partition_order, &is_first_partition, rice_parameter_size, frame_info->block_size, order, &error_code);

                    value = (next_out->next->next->value * 3) - (next_out->next->value * 3) + next_out->value + value;
                    if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                        return -1;

                    next_out->value = value;
                    next_out = next_out->next;
                }
                break;

            case 4:
                for(;crt_sample < frame_info->block_size; ++crt_sample) {
                    int64_t value = get_next_rice_residual(data_input, partition_order, &is_first_partition, rice_parameter_size, frame_info->block_size, order, &error_code);

                    value = (next_out->next->next->next->value * 4) - (next_out->next->next->value * 6) + (next_out->next->value * 4) - next_out->value + value;
                    if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                        return -1;

                    next_out->value = value;
                    next_out = next_out->next;
                }
        }
    }

    return 0;

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
static int decode_lpc(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, subframe_info_t* subframe_info, uint8_t bits_per_sample, uint8_t channel_nb) {

    int error_code = 0;
    uint8_t order = (subframe_info->type & 0x1F) + 1;
    uint16_t crt_sample = order;
    uint8_t i = 0;
    uint8_t lpc_precision = 0;
    int8_t lpc_shift = 0;
    int16_t coeffs[32];
    uint8_t residual_coding_method = 0;
    uint8_t partition_order = 0;        /* for rice coding */ 
    uint8_t is_first_partition = 1;     /* for rice coding */
    uint8_t rice_parameter_size = 0;    /* for rice coding */
    uint32_t dividers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
    int64_t previous_sample = 0;
    typedef struct warm_up_t {int64_t value; struct warm_up_t* next;} warm_up_t;
    warm_up_t warm_ups[32];
    warm_up_t* next_out = warm_ups;


    if(order == 1) {
        previous_sample = convert_to_signed(get_shifted_bits(data_input, bits_per_sample - subframe_info->wasted_bits_per_sample, &error_code), bits_per_sample - subframe_info->wasted_bits_per_sample);
        if(error_code == -1)
            return -1;
        if(put_shifted_bits(data_output, previous_sample << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
            return -1;
    } else {
        for(; i < order; ++i) {
            warm_ups[i].value = convert_to_signed(get_shifted_bits(data_input, bits_per_sample - subframe_info->wasted_bits_per_sample, &error_code), bits_per_sample - subframe_info->wasted_bits_per_sample);
            if(error_code == -1)
                return -1;

            if(put_shifted_bits(data_output, warm_ups[i].value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                return -1;
            warm_ups[i].next = warm_ups + i + 1;
        }
        warm_ups[order - 1].next = warm_ups;
    }

    lpc_precision = get_shifted_bits(data_input, 4, &error_code) + 1;
    if(error_code == -1)
        return -1;

    lpc_shift = convert_to_signed(get_shifted_bits(data_input, 5, &error_code), 5);
    if(error_code == -1)
        return -1;

    for(i = 0; i < order; ++i) {
        coeffs[i] = convert_to_signed(get_shifted_bits(data_input, lpc_precision, &error_code), lpc_precision);
        if(error_code == -1)
            return -1;
    }

    residual_coding_method = get_shifted_bits(data_input, 2, &error_code);
    if(error_code == -1)
        return -1;
    switch(residual_coding_method) {
        case 0:
            rice_parameter_size = 4;
            break;

        case 1:
            rice_parameter_size = 5;
            break;

        default:
            return -1;
    }

    is_first_partition = 1;
    partition_order = get_shifted_bits(data_input, 4, &error_code);
    if(error_code == -1)
        return -1;

    if(order == 1) {
        for(; crt_sample < frame_info->block_size; ++crt_sample) {
            int64_t value = coeffs[0] * previous_sample;

            if(lpc_shift < 0)
                value = value << (uint8_t)(-lpc_shift);
            else {
                if(value < 0)
                    value = -(((-value) + (dividers[lpc_shift] - 1)) >> lpc_shift);
                else
                    value = value >> lpc_shift;
            }

            value += get_next_rice_residual(data_input, partition_order, &is_first_partition, rice_parameter_size, frame_info->block_size, order, &error_code);

            if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                return -1;

            previous_sample = value;
        }
    } else {
        for(; crt_sample < frame_info->block_size; ++crt_sample) {
            int64_t value = 0;
            warm_up_t* crt = next_out;
            for(i = 0; i < order; ++i) {
                value += coeffs[order - i - 1] * crt->value;
                crt = crt->next;
            }

            if(lpc_shift < 0)
                value = value << (uint8_t)(-lpc_shift);
            else {
                if(value < 0)
                    value = -(((-value) + (dividers[lpc_shift] - 1)) >> lpc_shift);
                else
                    value = value >> lpc_shift;
            }

            value += get_next_rice_residual(data_input, partition_order, &is_first_partition, rice_parameter_size, frame_info->block_size, order, &error_code);

            if(put_shifted_bits(data_output, value << subframe_info->wasted_bits_per_sample, bits_per_sample, frame_info->channel_assignement, channel_nb) == -1)
                return -1;

            next_out->value = value;
            next_out = next_out->next;
        }
    }

    return 0;

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
static int decode_subframe_data(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, subframe_info_t* subframe_info, uint8_t channel_nb) {

    uint8_t bits_per_sample = frame_info->bits_per_sample;

    if((((frame_info->channel_assignement == LEFT_SIDE) || (frame_info->channel_assignement == MID_SIDE)) && (channel_nb == 1)) || ((frame_info->channel_assignement == RIGHT_SIDE) && (channel_nb == 0)))
        ++bits_per_sample;

    if(subframe_info->type == SUBFRAME_CONSTANT) {
        if(decode_constant(data_input, data_output, frame_info, subframe_info, bits_per_sample, channel_nb) == -1)
            return -1;
    } else if(subframe_info->type == SUBFRAME_VERBATIM) {
        if(decode_verbatim(data_input, data_output, frame_info, subframe_info, bits_per_sample, channel_nb) == -1)
            return -1;
    } else if((SUBFRAME_FIXED_LOW <= subframe_info->type) && (subframe_info->type <= SUBFRAME_FIXED_HIGH)) {
        if(decode_fixed(data_input, data_output, frame_info, subframe_info, bits_per_sample, channel_nb) == -1)
            return -1;
    } else if((SUBFRAME_LPC_LOW <= subframe_info->type) && (subframe_info->type <= SUBFRAME_LPC_HIGH)) {
        if(decode_lpc(data_input, data_output, frame_info, subframe_info, bits_per_sample, channel_nb) == -1)
            return -1;
    } else {
        fprintf(stderr, "Invalid subframe type\n");
        return -1;
    }

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

    error_code = read_frame_header(data_input, stream_info, &frame_info);
    if(error_code == -1)
        return -1;
    if(error_code == 0)
        return 0;

    data_output->starting_position = data_output->position;
    data_output->starting_shift = data_output->shift;

    for(; channel_nb < stream_info->nb_channels; ++channel_nb) {
        subframe_info_t subframe_info;

        if(read_subframe_header(data_input, &subframe_info) == -1)
            return -1;

        if(decode_subframe_data(data_input, data_output, &frame_info, &subframe_info, channel_nb) == -1)
            return -1;

        if((channel_nb + 1) < stream_info->nb_channels) {
            switch(frame_info.bits_per_sample) {
                case 8:
                    data_output->position = data_output->starting_position + (channel_nb + 1);
                    break;
                case 12:
                    data_output->position = data_output->starting_position + (channel_nb + 1);
                    if(data_output->shift == 4) {
                        data_output->shift = 0;
                        ++data_output->position;
                    } else {
                        data_output->shift = 4;
                    }
                    break;

                case 16:
                    data_output->position = data_output->starting_position + ((channel_nb + 1) * 2);
                    break;

                case 20:
                    data_output->position = data_output->starting_position + ((channel_nb + 1) * 2);
                    if(data_output->shift == 4) {
                        data_output->shift = 0;
                        ++data_output->position;
                    } else {
                        data_output->shift = 4;
                    }
                    break;

                case 24:
                    data_output->position = data_output->starting_position + ((channel_nb + 1) * 3);
                    break;

                case 32:
                    data_output->position = data_output->starting_position + ((channel_nb + 1) * 4);
            }
        }
    }

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
