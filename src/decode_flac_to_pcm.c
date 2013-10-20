/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "decode_flac.h"
#include "input.h"
#include "output.h"

int main(int argc, char* argv[]) {

    int opt = -1;
    struct option options[] = {{"unsigned", 0, NULL, 'u'}, {"big-endian", 0, NULL, 'b'}, {NULL, 0, NULL, 0}};
    data_input_t data_input = {0};
    data_output_t data_output = {0};
    stream_info_t stream_info = {0};

    data_output.is_little_endian = 1;
    data_output.is_signed = 1;

    while((opt = getopt_long(argc, argv, "", options, NULL)) > -1)
        switch(opt) {
            case 'b':
                data_output.is_little_endian = 0;
                break;

            case 'u':
                data_output.is_signed = 0;
                break;

            case '?':
                fprintf(stderr, "Usage: %s [--big-endian] [--unsigned] flac_file [output_filename]\n", argv[0]);
                return EXIT_FAILURE;
        }

    if(optind == argc) {
        fprintf(stderr, "Usage: %s [--big-endian] [--unsigned] flac_file [output_filename]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if((data_input.fd = open(argv[optind++], O_RDONLY)) == -1) {
        perror("An error occured while opening the flac file");
        return EXIT_FAILURE;
    }

    data_input.size = 10192;
    data_input.buffer = (uint8_t*)malloc(sizeof(uint8_t) * data_input.size);
    if(data_input.buffer == NULL) {
        perror("An error occured while allocating the input buffer");
        return EXIT_FAILURE;
    }
    data_input.read_size = data_input.size;
    data_input.position = data_input.size;
    data_input.shift = 0;

    if(refill_input_buffer(&data_input) != 1)
        return EXIT_FAILURE;

    if(decode_flac_metadata(&data_input, &stream_info) == -1)
        return EXIT_FAILURE;

    if((optind == argc) || (strcmp(argv[optind], "-") == 0)) {
        data_output.fd = 1;
    } else {
        if((data_output.fd = open(argv[optind], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
            perror("An error occured while opening the output_file");
            return EXIT_FAILURE;
        }
    }

    fprintf(stderr, "min_block_size: %u\n", stream_info.min_block_size);
    fprintf(stderr, "max_block_size: %u\n", stream_info.max_block_size);
    fprintf(stderr, "min_frame_size: %u\n", stream_info.min_frame_size);
    fprintf(stderr, "max_frame_size: %u\n", stream_info.max_frame_size);
    fprintf(stderr, "sample_rate: %u\n", stream_info.sample_rate);
    fprintf(stderr, "nb_channels: %u\n", stream_info.nb_channels);
    fprintf(stderr, "bits_per_sample: %u\n", stream_info.bits_per_sample);
    fprintf(stderr, "nb_samples: %" PRIu64 "\n", stream_info.nb_samples);

    data_output.size = (stream_info.nb_samples * stream_info.bits_per_sample * stream_info.nb_channels) / 8;
    data_output.buffer = (uint8_t*)malloc(sizeof(uint8_t) * data_output.size);
    if(data_output.buffer == NULL) {
        perror("An error occured while allocating the output buffer");
        return EXIT_FAILURE;
    }
    data_output.write_size = data_output.size;
    data_output.starting_position = 0;
    data_output.starting_shift = 0;
    data_output.position = 0;
    data_output.shift = 0;

    if(decode_flac_data(&data_input, &data_output, &stream_info) == -1)
        return EXIT_FAILURE;

    if(dump_buffer(&data_output, data_output.size) == -1)
        return EXIT_FAILURE;

    fprintf(stderr, "header md5: %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\n", stream_info.md5[0], stream_info.md5[1], stream_info.md5[2], stream_info.md5[3], stream_info.md5[4], stream_info.md5[5], stream_info.md5[6], stream_info.md5[7], stream_info.md5[8], stream_info.md5[9], stream_info.md5[10], stream_info.md5[11], stream_info.md5[12], stream_info.md5[13], stream_info.md5[14], stream_info.md5[15]);

    free(data_input.buffer);
    free(data_output.buffer);

    close(data_input.fd);
    close(data_output.fd);

    return EXIT_SUCCESS;

}
