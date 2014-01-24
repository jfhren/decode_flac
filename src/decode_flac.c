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
 * A linked list element for saving a previously decode value.
 */
typedef struct previous_value_t {
    DECODE_TYPE value;
    struct previous_value_t* next;
} previous_value_t ;

/**
 * Represent the current state of rice code decoding.
 */
typedef struct {
    uint8_t rice_parameter_size;    /**< The size of the rice code parameter (4
                                         or 5 bits). */
    uint8_t rice_parameter;         /**< The rice code parameter. */
    uint8_t partition_order;        /**< The partition order of the rice coded
                                         residuals. */
    uint8_t is_first_partition;     /**< Is the current partition the first
                                         one? */
    uint8_t has_escape_code;        /**< Is the current partition an escaped
                                         one? */
    uint8_t escape_bits_per_sample; /**< Number of bits used for each escaped
                                         residual. */
    uint16_t remaining_nb_samples;  /**< The remaining number of samples to
                                         return in the current partition. */
} rice_coding_info_t;

/**
 * Represent a subframe currently being decode.
 */
typedef struct {
    uint8_t type;                   /**< Tell how the sample are encoded within
                                         the subframe. */
    uint8_t wasted_bits_per_sample; /**< How many bits are wasted per sample. */
    uint8_t bits_per_sample;        /**< How many bits are used per sample while
                                         taking into account stereo encoding
                                         (but not wasted bits). */

    int data_input_position;    /**< If not -1, the current position in the
                                     input stream for this subframe. */
    uint8_t data_input_shift;   /**< The current shift in the input stream for
                                     this subframe. */

    DECODE_UTYPE value; /**< The value of a constant subframe or a previously
                             decoded value of a verbatim subframe. */

    previous_value_t previous_values[32];   /**< Linked list of previous values
                                                 for fixed or lpc subframes. */
    previous_value_t* next_out;             /**< The next value to be popped out
                                                 of the linked list. */

    uint8_t lpc_precision;  /**< The lpc's precision of a lpc subframe. */
    int8_t lpc_shift;       /**< The lpc's shift of a lpc subframe. */
    int16_t coeffs[32];     /**< The coefficients of a lpc subframe. */

    rice_coding_info_t residual_info;   /**< The current state of decoding the
                                             residual. */

    uint8_t has_parameters; /**< Has the parameters fully read? */
} subframe_info_t;

typedef struct {
    uint16_t block_size;            /**< The number of samples in the block
                                         encoded by this frame. */
    uint8_t channel_assignement;    /**< How many channel there is and how
                                         channels are encoded. */
    uint8_t nb_channels;            /**< The number of channels. */
    uint8_t bits_per_sample;        /**< The number of bits used to represent a
                                         sample. Should be the same than in the
                                         stream info. */

    #ifdef STEREO_ONLY
    subframe_info_t subframes_info[2];  /**< The subframes of this frame. */
    #else
    subframe_info_t subframes_info[8];  /**< The subframes of this frame. */
    #endif
} frame_info_t;


/**
 * Read some of the flac stream informations from data_input. The read
 * informations are used to fill the info paramater. Should be at the beginning
 * of the file.
 *
 * @param data_input  The data input for reading the informations.
 * @param stream_info The resulting useful informations are put there.
 *
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
 *
 * @param input_data The metadata are read from there making the stream go to
 *                   the first frame.
 *
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
 *
 * @param data_input      Frame header informations are read from there.
 * @param bits_per_sample Number of bits per sample coming from the stream info
 *                        block.
 * @param frame_info      Frame header informations are put there.
 *
 * @return Return 1 if successful, 0 if the previous frame was probably the
 *         last because we hit an EOF or whatever else relevant in this case or
 *         -1 in case of an unexpected error.
 */
static int read_frame_header(data_input_t* data_input, uint8_t bits_per_sample, frame_info_t* frame_info) {

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

    if((buffer[position] != 0xFF) || ((buffer[position + 1] & 0xFC) != 0xF8)) {
        fprintf(stderr, "Something is wrong with the synchro.\n");
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
        frame_info->bits_per_sample = bits_per_sample;
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
 *
 * @param data_input    Subframe header informations are read from there.
 * @param subframe_info Subframe header informations are put there.
 *
 * @return Return 0 if successful, -1 else.
 */
static int read_subframe_header(data_input_t* data_input, subframe_info_t* subframe_info) {

    int error_code = 0;
    uint8_t partial_subframe_header_data = get_shifted_bits(data_input, 8, &error_code);
    if(error_code == -1)
        return -1;

    subframe_info->type = (partial_subframe_header_data >> 1) & 0x3F;
    subframe_info->wasted_bits_per_sample = partial_subframe_header_data & 0x01;

    if(subframe_info->wasted_bits_per_sample) {
        while(get_one_shifted_bit(data_input, &error_code) == 0)
            ++subframe_info->wasted_bits_per_sample;
        if(error_code == -1)
            return -1;
    }

    return 0;

}


/**
 * Get the next residual coded using Rice codes from the flac stream and return
 * it. Residuals are coded using two Rice codes kind which differ by their
 * parameter size.
 *
 * @param data_input      Needed data are fetched from there.
 * @param residual_info   Retain the information about the currently decoded
 *                        residuals across calls to this function.
 * @param predictor_order Used to compute the number of partitions and thus the
 *                        number of samples per partition.
 * @param error_code      Like its name indicates, it is used to tell the caller
 *                        if an error occured or not. If equal to -1, yes else
 *                        no.
 *
 * @return Return the decoded residual.
 */
static DECODE_TYPE get_next_rice_residual(data_input_t* data_input, rice_coding_info_t* residual_info, uint16_t block_size, uint8_t predictor_order, int* error_code) {

    static uint16_t nb_partitions[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
    DECODE_TYPE value = 0;

    if(residual_info->remaining_nb_samples == 0) {
        residual_info->rice_parameter = get_shifted_bits(data_input, residual_info->rice_parameter_size, error_code);
        if(*error_code == -1)
            return 0;

        residual_info->has_escape_code = ((residual_info->rice_parameter_size == 4) && (residual_info->rice_parameter == 0x0F)) || ((residual_info->rice_parameter_size == 5) && (residual_info->rice_parameter == 0x1F));

        if(residual_info->has_escape_code) {
            residual_info->escape_bits_per_sample = get_shifted_bits(data_input, 5, error_code);
            if(*error_code == -1)
                return 0;
        }

        if(residual_info->is_first_partition) {
            residual_info->is_first_partition = 0;
            residual_info->remaining_nb_samples = residual_info->partition_order == 0 ? block_size - predictor_order : (block_size / nb_partitions[residual_info->partition_order]) - predictor_order;
        } else {
            residual_info->remaining_nb_samples = block_size / nb_partitions[residual_info->partition_order];
        }
    }

    if(!residual_info->has_escape_code) {
        DECODE_UTYPE msb = 0;

        while(get_one_shifted_bit(data_input, error_code) == 0)
            ++msb;
        if(*error_code == -1)
            return 0;

        if(residual_info->rice_parameter == 0) {
            if(msb & 0x01)
                value = -(msb >> 1) - 1;
            else
                value = msb >> 1;
            --residual_info->remaining_nb_samples;
            return value;
        } else {
            DECODE_UTYPE lsb = 0;

            lsb = get_shifted_bits(data_input, residual_info->rice_parameter, error_code);
            if(*error_code == -1)
                return 0;
            value = (msb << (residual_info->rice_parameter - 1)) | (lsb >> 1);
            if(lsb & 0x1)
                value = -(value + 1);
        }
        --residual_info->remaining_nb_samples;
        return value;
    } else {
        value = convert_to_signed(get_shifted_bits(data_input, residual_info->escape_bits_per_sample, error_code), residual_info->escape_bits_per_sample);
        if(*error_code == -1)
            return 0;
        --residual_info->remaining_nb_samples;
    }

    return value;

}


/**
 * Decode a constant subframe into the data output sink. A constant subframe
 * is a value repeated a number of time equivalent to the associeted number of
 * samples. Usefull for silent in track and enraging ghost tracks leading.
 *
 * @param data_input  The value is read from there.
 * @param data_output The resulting samples are sent there.
 * @param frame_info  Used to know the number of samples, the current channel
 *                    assignement or the subframe information.
 * @param channel_nb  Indicate which channel in the channel assignement we are
 *                    currently outputing.
 * @param crt_sample  The number of sample already decoded by a call to this
 *                    function for the current subframe (warmup samples
 *                    included).
 * @param error_code  -1 if an error occured, 0 else.
 *
 * @return Return the number of outputed samples for the current subframe.
 */
static uint16_t decode_constant(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    subframe_info_t* subframe = frame_info->subframes_info + channel_nb;

    if(!subframe->has_parameters) {
        subframe->value = get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code) << subframe->wasted_bits_per_sample;
        if(*error_code == -1)
            return 0;

        subframe->has_parameters = 1;
    }

    for(;crt_sample < frame_info->block_size; ++crt_sample) {
       *error_code = put_shifted_bits(data_output, subframe->value, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);

        if(*error_code == -1)
            return 0;

        if(*error_code == 0) {
            if(subframe->data_input_position == -1) {
                subframe->data_input_position = get_position(data_input);
                subframe->data_input_shift = data_input->shift;
            }

            return crt_sample;
        }
    }

    return frame_info->block_size;

}


/**
 * Decode a verbatim subframe into the data output sink. A verbatim subframe
 * is a sequence of raw samples like white noise and whatever random stuff.
 *
 * @param data_input  Samples are read from there.
 * @param data_output The resulting samples are outputed there.
 * @param frame_info  Provide usefull informations like the number of samples
 *                    and information on the subframe.
 * @param channel_nb  Indicate which channel in the channel assignement we are
 *                    currently outputing.
 * @param crt_sample  The number of sample already decoded by a call to this
 *                    function for the current subframe (warmup samples
 *                    included).
 * @param error_code  -1 if an error occured, 0 else.
 *
 * @return Return the number of outputed samples for the current subframe.
 */
static uint16_t decode_verbatim(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    subframe_info_t* subframe = frame_info->subframes_info + channel_nb;

    if(crt_sample != 0) {
        *error_code = put_shifted_bits(data_output, subframe->value, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;
        ++crt_sample;
    }

    for(;crt_sample < frame_info->block_size; ++crt_sample) {
        DECODE_UTYPE value = get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code) << subframe->wasted_bits_per_sample;
        if(*error_code == -1)
            return -1;

        *error_code = put_shifted_bits(data_output, value, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);

        if(*error_code == -1)
            return 0;

        if(*error_code == 0) {
            subframe->value = value;
            subframe->data_input_position = get_position(data_input);
            subframe->data_input_shift = data_input->shift;

#ifdef STEREO_ONLY
            if((channel_nb == 0) && (frame_info->subframes_info[0].data_input_position == -1)) {
#else
            if(((channel_nb + 1) < frame_info->nb_channels) && (frame_info->subframes_info[channel_nb + 1].data_input_position == -1)) {
#endif
                if(skip_nb_bits(data_input, (subframe->bits_per_sample - subframe->wasted_bits_per_sample) * (frame_info->block_size - (crt_sample + 1))) == -1) {
                    *error_code = -1;
                    return 0;
                }
            }

            return crt_sample;
        }
    }

    return frame_info->block_size;

}


/**
 * Decode a fixed subframe into the data output sink. In a fixed subframe,
 * samples are encoded using a fixed linear predictor of zero to fourth order.
 *
 * @param data_input  Warm-up samples and residuals are read from there.
 * @param data_output The decoded samples are outputed there.
 * @param frame_info  Provide usefull information like the number of samples and
 *                    information on the subframe.
 * @param channel_nb  Indicate which channel in the channel assignement we are
 *                    currently outputing.
 * @param crt_sample  The number of sample already decoded by a call to this
 *                    function for the current subframe (warmup samples
 *                    included).
 * @param error_code  -1 if an error occured, 0 else.
 *
 * @return Return the number of outputed samples for the current subframe.
 */
static uint16_t decode_fixed(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    subframe_info_t* subframe = frame_info->subframes_info + channel_nb;
    uint8_t order = subframe->type - 8;

    if(!subframe->has_parameters) {
        uint8_t residual_coding_method = 0;

        if(order) {
            switch(order) {
                case 4:
                    subframe->previous_values[0].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                    if(*error_code == -1)
                        return 0;
                    subframe->previous_values[0].next = subframe->previous_values + 1;

                case 3:
                    subframe->previous_values[order - 3].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                    if(*error_code == -1)
                        return 0;
                    subframe->previous_values[order - 3].next = subframe->previous_values + order - 2;

                case 2:
                    subframe->previous_values[order - 2].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                    if(*error_code == -1)
                        return 0;
                    subframe->previous_values[order - 2].next = subframe->previous_values + order - 1;

                case 1:
                    subframe->previous_values[order - 1].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                    if(*error_code == -1)
                        return 0;
                    subframe->previous_values[order - 1].next = subframe->previous_values + order;
            }

            if(order > 1)
                subframe->previous_values[order - 1].next = subframe->previous_values;
        }

        subframe->next_out = subframe->previous_values;

        residual_coding_method = get_shifted_bits(data_input, 2, error_code);
        if(*error_code == -1)
            return 0;

        switch(residual_coding_method) {
            case 0:
                subframe->residual_info.rice_parameter_size = 4;
                break;

            case 1:
                subframe->residual_info.rice_parameter_size = 5;
                break;

            default:
                fprintf(stderr, "Invalid residual encoding method\n");
                *error_code = -1;
                return 0;
        }

        subframe->residual_info.rice_parameter = 0;
        subframe->residual_info.has_escape_code = 0;
        subframe->residual_info.escape_bits_per_sample = 0;
        subframe->residual_info.remaining_nb_samples = 0;
        subframe->residual_info.is_first_partition = 1;

        subframe->residual_info.partition_order = get_shifted_bits(data_input, 4, error_code);
        if(*error_code == -1)
            return 0;

        subframe->has_parameters = 1;

        if(!order) {
            subframe->previous_values[0].value = get_next_rice_residual(data_input, &(subframe->residual_info), frame_info->block_size, order, error_code);
            if(*error_code == -1)
                return 0;
        }
    }

    if(crt_sample < order) {
        switch(order - crt_sample) {
            case 4:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[0].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 3:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 2:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 1:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;
                goto decode_fixed_subframe;
        }

        if(*error_code == -1)
            return 0;

        if(*error_code == 0) {
            subframe->data_input_position = get_position(data_input);
            subframe->data_input_shift = data_input->shift;

#ifdef STEREO_ONLY
            if((channel_nb == 0) && (frame_info->subframes_info[1].data_input_position == -1)) {
#else
            if(((channel_nb + 1) < frame_info->nb_channels) && (frame_info->subframes_info[channel_nb + 1].data_input_position == -1)) {
#endif
                uint16_t crt_skipped_sample = order;
                rice_coding_info_t residual_info = subframe->residual_info; 

                for(; crt_skipped_sample < frame_info->block_size; ++crt_skipped_sample)
                    get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);

                if(*error_code == -1)
                    return 0;
            }

            return crt_sample;
        }
    } else {
        *error_code = put_shifted_bits(data_output, subframe->next_out->value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;

        ++crt_sample;
        if(order > 1)
            subframe->next_out = subframe->next_out->next;
    }

    decode_fixed_subframe:
    switch(order) {
        case 0:
            for(;crt_sample < frame_info->block_size; ++crt_sample) {
                DECODE_TYPE value = get_next_rice_residual(data_input, &(subframe->residual_info), frame_info->block_size, 0, error_code);
                if(*error_code == -1)
                    return 0;

                *error_code = put_shifted_bits(data_output, value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;

                if(*error_code == 0) {
                    subframe->previous_values[0].value = value;
                    goto buffer_full_error;
                }
            }

            return frame_info->block_size;

        case 1:
            for(;crt_sample < frame_info->block_size; ++crt_sample) {
                DECODE_TYPE value = subframe->previous_values[0].value + get_next_rice_residual(data_input, &(subframe->residual_info), frame_info->block_size, 1, error_code);
                subframe->previous_values[0].value = value;

                if(*error_code == -1)
                    return 0;

                *error_code = put_shifted_bits(data_output, value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;

                if(*error_code == 0)
                    goto buffer_full_error;
            }

            return frame_info->block_size;

        case 2:
            for(;crt_sample < frame_info->block_size; ++crt_sample) {
                DECODE_TYPE value = (subframe->next_out->next->value << 1) - subframe->next_out->value + get_next_rice_residual(data_input, &(subframe->residual_info), frame_info->block_size, 2, error_code);
                subframe->next_out->value = value;

                if(*error_code == -1)
                    return 0;

                *error_code = put_shifted_bits(data_output, value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;

                if(*error_code == 0)
                    goto buffer_full_error;

                subframe->next_out = subframe->next_out->next;
            }

            return frame_info->block_size;

        case 3:
            for(;crt_sample < frame_info->block_size; ++crt_sample) {
                DECODE_TYPE value = ((subframe->next_out->next->next->value << 1) + subframe->next_out->next->next->value) - ((subframe->next_out->next->value << 1) + subframe->next_out->next->value) + subframe->next_out->value + get_next_rice_residual(data_input, &(subframe->residual_info), frame_info->block_size, 3, error_code);
                subframe->next_out->value = value;

                if(*error_code == -1)
                    return 0;

                *error_code = put_shifted_bits(data_output, value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;

                if(*error_code == 0)
                    goto buffer_full_error;

                subframe->next_out = subframe->next_out->next;
            }

            return frame_info->block_size;

        case 4:
            for(;crt_sample < frame_info->block_size; ++crt_sample) {
                DECODE_TYPE value = (subframe->next_out->next->next->next->value << 2) - (((subframe->next_out->next->next->value << 1) + subframe->next_out->next->next->value) << 1) + (subframe->next_out->next->value << 2) - subframe->next_out->value + get_next_rice_residual(data_input, &(subframe->residual_info), frame_info->block_size, 4, error_code);
                subframe->next_out->value = value;

                if(*error_code == -1)
                    return 0;

                *error_code = put_shifted_bits(data_output, value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
                if(*error_code == -1)
                    return 0;

                if(*error_code == 0)
                    goto buffer_full_error;

                subframe->next_out = subframe->next_out->next;
            }

            return frame_info->block_size;
    }

    buffer_full_error:

    subframe->data_input_position = get_position(data_input);
    subframe->data_input_shift = data_input->shift;

#ifdef STEREO_ONLY
    if((channel_nb == 0) && (frame_info->subframes_info[1].data_input_position == -1)) {
#else
    if(((channel_nb + 1) < frame_info->nb_channels) && (frame_info->subframes_info[channel_nb + 1].data_input_position == -1)) {
#endif
        uint16_t crt_skipped_sample = crt_sample + 1;
        rice_coding_info_t residual_info = subframe->residual_info; 

        for(; crt_skipped_sample < frame_info->block_size; ++crt_skipped_sample)
            get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);

        if(*error_code == -1)
            return 0;
    }

    return crt_sample;

}


/**
 * Decode a LPC subframe into the data output sink. In a LPC subframe, samples
 * are encoded using FIR linear prediction of one to thirty-second order.
 *
 * @param data_input  Parameters, warm-up samples and residuals are read from
 *                    there.
 * @param data_output The decoded samples are outputed there.
 * @param frame_info  Provide usefull informations like the number of samples
 *                    and information on the subframe.
 * @param channel_nb  Indicate which channel in the channel assignement we are
 *                    currently outputing.
 * @param crt_sample  The number of sample already decoded by a call to this
 *                    function for the current subframe (warmup samples
 *                    included).
 * @param error_code  -1 if an error occured, 0 else.
 *
 * @return Return the number of outputed samples for the current subframe.
 */
static uint16_t decode_lpc(data_input_t* data_input, data_output_t* data_output, frame_info_t* frame_info, uint8_t channel_nb, uint16_t crt_sample, int* error_code) {

    subframe_info_t* subframe = frame_info->subframes_info + channel_nb;
    uint8_t order = (subframe->type & 0x1F) + 1;
    static uint32_t dividers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};

    if(!subframe->has_parameters) {
        uint8_t i = 0;
        uint8_t residual_coding_method = 0;

        switch(order) {
            case 32:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 31:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 30:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 29:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 28:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 27:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 26:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 25:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 24:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 23:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 22:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 21:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 20:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 19:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 18:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 17:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 16:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 15:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 14:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 13:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 12:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 11:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 10:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 9:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 8:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 7:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 6:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 5:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 4:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 3:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 2:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
                ++i;

            case 1:
                subframe->previous_values[i].value = convert_to_signed(get_shifted_bits(data_input, subframe->bits_per_sample - subframe->wasted_bits_per_sample, error_code), subframe->bits_per_sample - subframe->wasted_bits_per_sample);
                subframe->previous_values[i].next = subframe->previous_values + i + 1;
        }

        if(*error_code == -1)
            return 0;

        subframe->previous_values[order - 1].next = subframe->previous_values;
        subframe->next_out = subframe->previous_values;

        subframe->lpc_precision = get_shifted_bits(data_input, 4, error_code) + 1;
        if(*error_code == -1)
            return 0;

        subframe->lpc_shift = convert_to_signed(get_shifted_bits(data_input, 5, error_code), 5);
        if(*error_code == -1)
            return 0;

        switch(order) {
            case 32:
                subframe->coeffs[0] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 31:
                subframe->coeffs[order - 31] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 30:
                subframe->coeffs[order - 30] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 29:
                subframe->coeffs[order - 29] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 28:
                subframe->coeffs[order - 28] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 27:
                subframe->coeffs[order - 27] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 26:
                subframe->coeffs[order - 26] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 25:
                subframe->coeffs[order - 25] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 24:
                subframe->coeffs[order - 24] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 23:
                subframe->coeffs[order - 23] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 22:
                subframe->coeffs[order - 22] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 21:
                subframe->coeffs[order - 21] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 20:
                subframe->coeffs[order - 20] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 19:
                subframe->coeffs[order - 19] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 18:
                subframe->coeffs[order - 18] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 17:
                subframe->coeffs[order - 17] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 16:
                subframe->coeffs[order - 16] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 15:
                subframe->coeffs[order - 15] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 14:
                subframe->coeffs[order - 14] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 13:
                subframe->coeffs[order - 13] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 12:
                subframe->coeffs[order - 12] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 11:
                subframe->coeffs[order - 11] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 10:
                subframe->coeffs[order - 10] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 9:
                subframe->coeffs[order - 9] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 8:
                subframe->coeffs[order - 8] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 7:
                subframe->coeffs[order - 7] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 6:
                subframe->coeffs[order - 6] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 5:
                subframe->coeffs[order - 5] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 4:
                subframe->coeffs[order - 4] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 3:
                subframe->coeffs[order - 3] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 2:
                subframe->coeffs[order - 2] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);

            case 1:
                subframe->coeffs[order - 1] = convert_to_signed(get_shifted_bits(data_input, subframe->lpc_precision, error_code), subframe->lpc_precision);
        }

        if(*error_code == -1)
            return 0;

        residual_coding_method = get_shifted_bits(data_input, 2, error_code);
        if(*error_code == -1)
            return 0;

        switch(residual_coding_method) {
            case 0:
                subframe->residual_info.rice_parameter_size = 4;
                break;

            case 1:
                subframe->residual_info.rice_parameter_size = 5;
                break;

            default:
                *error_code = -1;
                return 0;
        }

        subframe->residual_info.rice_parameter = 0;
        subframe->residual_info.has_escape_code = 0;
        subframe->residual_info.escape_bits_per_sample = 0;
        subframe->residual_info.remaining_nb_samples = 0;
        subframe->residual_info.is_first_partition = 1;

        subframe->residual_info.partition_order = get_shifted_bits(data_input, 4, error_code);
        if(*error_code == -1)
            return 0;

        subframe->has_parameters = 1;
    }

    if(crt_sample < order) {
        switch(order - crt_sample) {
            case 32:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[0].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 31:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 30:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 29:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 28:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 27:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 26:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 25:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 24:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 23:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 22:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 21:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 20:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 19:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 18:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 17:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 16:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 15:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 14:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 13:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 12:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 11:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 10:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 9:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 8:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 7:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 6:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 5:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 4:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 3:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 2:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;

            case 1:
                if((*error_code = put_shifted_bits(data_output, subframe->previous_values[crt_sample].value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb)) != 1)
                    break;
                ++crt_sample;
                goto decode_lpc_subframe;
        }

        if(*error_code == -1)
            return 0;

        if(*error_code == 0) {
            subframe->data_input_position = get_position(data_input);
            subframe->data_input_shift = data_input->shift;

#ifdef STEREO_ONLY
            if((channel_nb == 0) && (frame_info->subframes_info[1].data_input_position == -1)) {
#else
            if(((channel_nb + 1) < frame_info->nb_channels) && (frame_info->subframes_info[channel_nb + 1].data_input_position == -1)) {
#endif
                uint16_t crt_skipped_sample = order;
                rice_coding_info_t residual_info = subframe->residual_info; 

                for(; crt_skipped_sample < frame_info->block_size; ++crt_skipped_sample)
                    get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);

                if(*error_code == -1)
                    return 0;
            }

            return crt_sample;
        }
    } else {
        *error_code = put_shifted_bits(data_output, subframe->next_out->value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;

        ++crt_sample;
        subframe->next_out = subframe->next_out->next;
    }

    decode_lpc_subframe:
    for(; crt_sample < frame_info->block_size; ++crt_sample) {
        DECODE_TYPE value = 0;
        previous_value_t* crt = subframe->next_out;

        switch(order) {
            case 32:
                value += subframe->coeffs[31] * crt->value;
                crt = crt->next;

            case 31:
                value += subframe->coeffs[30] * crt->value;
                crt = crt->next;

            case 30:
                value += subframe->coeffs[29] * crt->value;
                crt = crt->next;

            case 29:
                value += subframe->coeffs[28] * crt->value;
                crt = crt->next;

            case 28:
                value += subframe->coeffs[27] * crt->value;
                crt = crt->next;

            case 27:
                value += subframe->coeffs[26] * crt->value;
                crt = crt->next;

            case 26:
                value += subframe->coeffs[25] * crt->value;
                crt = crt->next;

            case 25:
                value += subframe->coeffs[24] * crt->value;
                crt = crt->next;

            case 24:
                value += subframe->coeffs[23] * crt->value;
                crt = crt->next;

            case 23:
                value += subframe->coeffs[22] * crt->value;
                crt = crt->next;

            case 22:
                value += subframe->coeffs[21] * crt->value;
                crt = crt->next;

            case 21:
                value += subframe->coeffs[20] * crt->value;
                crt = crt->next;

            case 20:
                value += subframe->coeffs[19] * crt->value;
                crt = crt->next;

            case 19:
                value += subframe->coeffs[18] * crt->value;
                crt = crt->next;

            case 18:
                value += subframe->coeffs[17] * crt->value;
                crt = crt->next;

            case 17:
                value += subframe->coeffs[16] * crt->value;
                crt = crt->next;

            case 16:
                value += subframe->coeffs[15] * crt->value;
                crt = crt->next;

            case 15:
                value += subframe->coeffs[14] * crt->value;
                crt = crt->next;

            case 14:
                value += subframe->coeffs[13] * crt->value;
                crt = crt->next;

            case 13:
                value += subframe->coeffs[12] * crt->value;
                crt = crt->next;

            case 12:
                value += subframe->coeffs[11] * crt->value;
                crt = crt->next;

            case 11:
                value += subframe->coeffs[10] * crt->value;
                crt = crt->next;

            case 10:
                value += subframe->coeffs[9] * crt->value;
                crt = crt->next;

            case 9:
                value += subframe->coeffs[8] * crt->value;
                crt = crt->next;

            case 8:
                value += subframe->coeffs[7] * crt->value;
                crt = crt->next;

            case 7:
                value += subframe->coeffs[6] * crt->value;
                crt = crt->next;

            case 6:
                value += subframe->coeffs[5] * crt->value;
                crt = crt->next;

            case 5:
                value += subframe->coeffs[4] * crt->value;
                crt = crt->next;

            case 4:
                value += subframe->coeffs[3] * crt->value;
                crt = crt->next;

            case 3:
                value += subframe->coeffs[2] * crt->value;
                crt = crt->next;

            case 2:
                value += subframe->coeffs[1] * crt->value;
                crt = crt->next;

            case 1:
                value += subframe->coeffs[0] * crt->value;
        }

        if(subframe->lpc_shift < 0)
            value = value << (uint8_t)(-subframe->lpc_shift);
        else {
            if(value < 0)
                value = -(((-value) + (dividers[subframe->lpc_shift] - 1)) >> subframe->lpc_shift);
            else
                value = value >> subframe->lpc_shift;
        }

        value += get_next_rice_residual(data_input, &(subframe->residual_info), frame_info->block_size, order, error_code);
        if(*error_code == -1)
            return 0;

        subframe->next_out->value = value;

        *error_code = put_shifted_bits(data_output, value << subframe->wasted_bits_per_sample, subframe->bits_per_sample, frame_info->channel_assignement, channel_nb);
        if(*error_code == -1)
            return 0;

        if(*error_code == 0) {
            subframe->data_input_position = get_position(data_input);
            subframe->data_input_shift = data_input->shift;

#ifdef STEREO_ONLY
            if((channel_nb == 0) && (frame_info->subframes_info[1].data_input_position == -1)) {
#else
            if(((channel_nb + 1) < frame_info->nb_channels) && (frame_info->subframes_info[channel_nb + 1].data_input_position == -1)) {
#endif
                uint16_t crt_skipped_sample = crt_sample + 1;
                rice_coding_info_t residual_info = subframe->residual_info; 

                for(; crt_skipped_sample < frame_info->block_size; ++crt_skipped_sample)
                    get_next_rice_residual(data_input, &residual_info, frame_info->block_size, order, error_code);

                if(*error_code == -1)
                    return 0;
            }

            return crt_sample;
        }

        subframe->next_out = subframe->next_out->next;
    }

    return frame_info->block_size;

}


/**
 * Decode an entire frame and send the decoded samples to the output sink when
 * the output buffer is full. The decoded frame consists of a frame header and
 * one or more couple of subframe headers and data.
 *
 * @param data_input      Parameters, warm-up samples and residuals are read
 *                        from there.
 * @param data_output     The decoded samples are outputed there.
 * @param bits_per_sample Number of bits per sample coming from the stream info
 *                        block.
 * @param nb_channels     The number of channels coming from the stream info
 *                        block.
 *
 * @return Return 1 if successful, 0 if the previous frame was probably the
 *         last because we hit an EOF or whatever else relevant in this case or
 *         -1 in case of an unexpected error.
 */
static int decode_frame(data_input_t* data_input, data_output_t* data_output, uint8_t bits_per_sample, uint8_t nb_channels) {

    int error_code = 0;
    uint8_t channel_nb = 0;
    frame_info_t frame_info;
    #ifdef STEREO_ONLY
    uint16_t crt_samples[2] = {0};
    #else
    uint16_t crt_samples[8] = {0};
    #endif
    uint16_t nb_read_samples = 0;

    frame_info.nb_channels = nb_channels;

    error_code = read_frame_header(data_input, bits_per_sample, &frame_info);
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

    /* Initialize some per subframe values */
    for(; channel_nb < nb_channels; ++channel_nb) {
        frame_info.subframes_info[channel_nb].has_parameters = 0;
        frame_info.subframes_info[channel_nb].data_input_position = -1;
    }

    do {
        int nb_bits_to_write = 0;
        int nb_bytes_to_write = 0;

        data_output->starting_position = data_output->position;
        data_output->starting_shift = data_output->shift;

        for(channel_nb = 0; channel_nb < nb_channels; ++channel_nb) {
            /* If it is the first time decoding the subframe, we read its header. */
            if(crt_samples[channel_nb] == 0) {
                if(read_subframe_header(data_input, frame_info.subframes_info + channel_nb) == -1)
                    return -1;

                if((((frame_info.channel_assignement == LEFT_SIDE) || (frame_info.channel_assignement == MID_SIDE)) && (channel_nb == 1)) || ((frame_info.channel_assignement == RIGHT_SIDE) && (channel_nb == 0)))
                    frame_info.subframes_info[channel_nb].bits_per_sample = frame_info.bits_per_sample + 1;
                else
                    frame_info.subframes_info[channel_nb].bits_per_sample = frame_info.bits_per_sample;
            } else if(frame_info.subframes_info[channel_nb].type != SUBFRAME_CONSTANT) {
                /** If it's not, we skip to the rightful position. */
                if(skip_to_position(data_input, frame_info.subframes_info[channel_nb].data_input_position) == -1)
                    return -1;
                data_input->shift = frame_info.subframes_info[channel_nb].data_input_shift;
            }

            /* We read the subframe data and decode them. */
            if(frame_info.subframes_info[channel_nb].type == SUBFRAME_CONSTANT)
                crt_samples[channel_nb] = decode_constant(data_input, data_output, &frame_info, channel_nb, crt_samples[channel_nb], &error_code);
            else if(frame_info.subframes_info[channel_nb].type == SUBFRAME_VERBATIM)
                crt_samples[channel_nb] = decode_verbatim(data_input, data_output, &frame_info, channel_nb, crt_samples[channel_nb], &error_code);
            else if((SUBFRAME_FIXED_LOW <= frame_info.subframes_info[channel_nb].type) && (frame_info.subframes_info[channel_nb].type <= SUBFRAME_FIXED_HIGH))
                crt_samples[channel_nb] = decode_fixed(data_input, data_output, &frame_info, channel_nb, crt_samples[channel_nb], &error_code);
            else if((SUBFRAME_LPC_LOW <= frame_info.subframes_info[channel_nb].type) && (frame_info.subframes_info[channel_nb].type <= SUBFRAME_LPC_HIGH))
                crt_samples[channel_nb] = decode_lpc(data_input, data_output, &frame_info, channel_nb, crt_samples[channel_nb], &error_code);
            else {
                fprintf(stderr, "Invalid subframe type\n");
                return -1;
            }

            if(error_code == -1)
                return -1;

            /* We change the position in the output buffer for the next channel (if any). */
#ifdef STEREO_ONLY
            if(channel_nb == 0) {
#else
            if((channel_nb + 1) < nb_channels) {
#endif
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
            }
        }

        /* Number of read samples since the last iteration (if any). */
        nb_read_samples = crt_samples[0] - nb_read_samples;

        nb_bits_to_write = nb_read_samples * nb_channels * frame_info.bits_per_sample + data_output->starting_shift;
        nb_bytes_to_write = nb_bits_to_write / 8;
        dump_buffer(data_output, nb_bytes_to_write);

        nb_read_samples = crt_samples[0];
        data_output->position = 0;
        if(nb_bits_to_write & 7) {
            data_output->shift = 4;
            data_output->buffer[0] = data_output->buffer[nb_bytes_to_write];
        } else {
            data_output->shift = 0;
        }
    } while(crt_samples[0] < frame_info.block_size);

    /** If the last subframe is a constant one and a position was saved, we skip to it. */
    if((frame_info.subframes_info[channel_nb - 1].type == SUBFRAME_CONSTANT) && (frame_info.subframes_info[channel_nb - 1].data_input_position != -1)) {
        if(skip_to_position(data_input, frame_info.subframes_info[channel_nb - 1].data_input_position) == -1)
            return -1;
        data_input->shift = frame_info.subframes_info[channel_nb - 1].data_input_shift;
    }

    /* padding */
    if(data_input->shift != 0) {
        data_input->shift = 0;
        ++data_input->position;
    }

    /* frame footer */
    get_shifted_bits(data_input, 16, &error_code);
    if(error_code == -1)
        return -1;

    return 1;

}


/**
 * Decode the flac metedata stream info and skip the others.
 *
 * @param data_input  The metadata are read from there.
 * @param stream_info The resulting useful informations are put there.
 *
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
int decode_flac_data(data_input_t* data_input, data_output_t* data_output, uint8_t bits_per_sample, uint8_t nb_channels) {

    int error_code = 0;

    while((error_code = decode_frame(data_input, data_output, bits_per_sample, nb_channels)) > 0);

    if(error_code == -1)
        return -1;

    return 0;

}
