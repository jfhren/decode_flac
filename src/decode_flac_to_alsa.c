/**
 * Copyright © 2013 Jean-François Hren <jfhren@gmail.com>
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <alloca.h>

#include <alsa/asoundlib.h>

#include "decode_flac.h"
#include "input.h"
#include "output.h"

static snd_pcm_t* g_pcm_handle = NULL;
static stream_info_t g_stream_info = {0};
static snd_pcm_uframes_t g_offset;
static snd_pcm_uframes_t g_period_size = 120000;


static int xrun_recovery(snd_pcm_t *handle, int err) {

    if(err == -EPIPE) {	/* under-run */
        err = snd_pcm_prepare(handle);

        if (err < 0)
            fprintf(stderr, "Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));

        return 0;
    } else if(err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN);
//            usleep(1);	/* wait until the suspend flag is released */

        if (err < 0) {
            err = snd_pcm_prepare(handle);

            if (err < 0)
                fprintf(stderr, "Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
 
       return 0;
    }

    return err;

}


static int get_alsa_buffer(data_output_t *data_output, uint8_t should_reset_first) {
    static int first = 1;
    static int size = 0;

    const snd_pcm_channel_area_t* areas;
	snd_pcm_uframes_t frames;
	snd_pcm_sframes_t avail;
	snd_pcm_state_t state;
	int err;

    if(should_reset_first)
        first = 1;

    if(size == 0) {
        size = g_period_size;
redo:
        state = snd_pcm_state(g_pcm_handle);

        if (state == SND_PCM_STATE_XRUN) {
            err = xrun_recovery(g_pcm_handle, -EPIPE);

            if (err < 0) {
                fprintf(stderr, "XRUN recovery failed: %s\n", snd_strerror(err));
                return -1;
            }

            first = 1;
        } else if (state == SND_PCM_STATE_SUSPENDED) {
            err = xrun_recovery(g_pcm_handle, -ESTRPIPE);

            if (err < 0) {
                fprintf(stderr, "SUSPEND recovery failed: %s\n", snd_strerror(err));
                return -1;
            }
        }

        avail = snd_pcm_avail_update(g_pcm_handle);

        if (avail < 0) {
            err = xrun_recovery(g_pcm_handle, avail);

            if (err < 0) {
                fprintf(stderr, "avail update failed: %s\n", snd_strerror(err));
                return -1;
            }

            first = 1;
            goto redo;
        }

        if ((snd_pcm_uframes_t)avail < g_period_size) {
            if (first) {
                first = 0;
                err = snd_pcm_start(g_pcm_handle);

                if (err < 0) {
                    fprintf(stderr, "Start error: %s\n", snd_strerror(err));
                    return -1;
                }
            } else {
                err = snd_pcm_wait(g_pcm_handle, -1);

                if (err < 0) {
                    if ((err = xrun_recovery(g_pcm_handle, err)) < 0) {
                        fprintf(stderr, "snd_pcm_wait error: %s\n", snd_strerror(err));
                        return -1;
                    }

                    first = 1;
                }
            }

            goto redo;
        }
    }

    frames = size;
    err = snd_pcm_mmap_begin(g_pcm_handle, &areas, &g_offset, &frames);
    if (err < 0) {
        if ((err = xrun_recovery(g_pcm_handle, err)) < 0) {
            fprintf(stderr, "MMAP begin avail error: %s\n", snd_strerror(err));
            return -1;
        }
        first = 1;
    }
    size -= frames;

    data_output->buffer = (uint8_t*)areas[0].addr + (g_offset * (g_stream_info.bits_per_sample / 8) * g_stream_info.nb_channels);
    data_output->size = (int)frames * (g_stream_info.bits_per_sample / 8) * g_stream_info.nb_channels;
    data_output->write_size = data_output->size;
    data_output->position = 0;
    data_output->shift = 0;

    return 0;

}


static int dump_buffer_to_alsa(data_output_t* data_output, int nb_bits, uint8_t force_dump) {

	snd_pcm_sframes_t commitres;
    snd_pcm_uframes_t tocommit;
    uint8_t should_reset_first = 0;
    int err;

    (void)nb_bits;

    if( !force_dump && (data_output->position < data_output->size) )
        return 0;

    tocommit = (snd_pcm_uframes_t)((data_output->position > data_output->size ? data_output->size : data_output->position)/((g_stream_info.bits_per_sample / 8) * g_stream_info.nb_channels));
    commitres = snd_pcm_mmap_commit(g_pcm_handle, g_offset, tocommit);
    if (commitres < 0 || commitres != (snd_pcm_sframes_t)tocommit) {
        if ((err = xrun_recovery(g_pcm_handle, commitres >= 0 ? -EPIPE : commitres)) < 0) {
            fprintf(stderr, "MMAP commit error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }
        should_reset_first = 1;
    }

    return force_dump ? 0 : get_alsa_buffer(data_output, should_reset_first);

}


/**
 * TODO should take into account endianess of the input and output
 */
static snd_pcm_format_t get_alsa_format(uint8_t is_signed) {

    switch(g_stream_info.bits_per_sample) {
        case 8:
            if(is_signed)
                return SND_PCM_FORMAT_S8;
            else
                return SND_PCM_FORMAT_U8;

        case 16:
            if(is_signed)
                return SND_PCM_FORMAT_S16;
            else
                return SND_PCM_FORMAT_U16;

        case 24:
            if(is_signed)
#if BYTE_ORDER == LITTLE_ENDIAN
                return SND_PCM_FORMAT_S24_3LE;
#elif BYTE_ORDER == BIG_ENDIAN
                return SND_PCM_FORMAT_S24_3BE;
#else
                return SND_PCM_FORMAT_UNKNOWN;
#endif
            else
#if BYTE_ORDER == LITTLE_ENDIAN
                return SND_PCM_FORMAT_U24_3LE;
#elif BYTE_ORDER == BIG_ENDIAN
                return SND_PCM_FORMAT_U24_3BE;
#else
                return SND_PCM_FORMAT_UNKNOWN;
#endif

        case 32:
            if(is_signed)
                return SND_PCM_FORMAT_S32;
            else
                return SND_PCM_FORMAT_U32;
    }

    return SND_PCM_FORMAT_UNKNOWN;

} 


static int init_alsa(const char* pcm_name, uint8_t is_signed) {

    snd_pcm_hw_params_t* hw_params;
    unsigned int rrate;
    int err;
(void)is_signed;
	snd_pcm_hw_params_alloca(&hw_params);

    if ((err = snd_pcm_open(&g_pcm_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Could not open audio device: %s\n", snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_any(g_pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Could not get alsa hardware parameters space: %s\n", snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_set_access(g_pcm_handle, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "Could not set mmap interleaved access: %s\n", snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_set_format(g_pcm_handle, hw_params, get_alsa_format(is_signed));
    if (err < 0) {
        fprintf(stderr, "Could not set format: %s\n", snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_set_channels(g_pcm_handle, hw_params, (unsigned int)g_stream_info.nb_channels);
    if (err < 0) {
        fprintf(stderr, "Could not set %u channels: %s\n", g_stream_info.nb_channels, snd_strerror(err));
        return -1;
    }

    rrate = g_stream_info.sample_rate;
    err = snd_pcm_hw_params_set_rate_near(g_pcm_handle, hw_params, &rrate, 0);
    if (err < 0) {
        fprintf(stderr, "Could not set rate to %uHz: %s\n", g_stream_info.sample_rate, snd_strerror(err));
        return -1;
    }

    if (rrate != g_stream_info.sample_rate) {
        fprintf(stderr, "Setted rate does not match requested one (requested %uHz, get %uHz)\n", g_stream_info.sample_rate, rrate);
        return -1;
    }

    err = snd_pcm_hw_params_set_periods(g_pcm_handle, hw_params, 2, 0);
    if ( err < 0) {
        fprintf(stderr, "Could not set 2 periods: %s\n", snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_set_buffer_size(g_pcm_handle, hw_params, g_period_size * 2);
    if (err < 0) {
        fprintf(stderr, "Coud not set buffer size: %s\n", snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params(g_pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Could not set hw params: %s\n", snd_strerror(err));
        return -1;
    }

    return 0;

}


int main(int argc, char** argv) {

    data_input_t data_input = {0};
    data_output_t data_output = {0};

    int input_fd = -1;

    if( argc != 3) {
        fprintf(stderr, "Usage: %s pcm_device flac_file\n", argv[0]);
        return EXIT_FAILURE;
    }

    data_output.dump_func = dump_buffer_to_alsa;
    data_output.is_little_endian = 1;
    data_output.is_signed = 1;
    data_output.starting_position = 0;
    data_output.starting_shift = 0;
    data_output.position = 0;

    if((input_fd = open(argv[2], O_RDONLY)) == -1) {
        perror("An error occured while opening the flac file");
        return EXIT_FAILURE;
    }

    if(init_data_input_from_fd(&data_input, input_fd, 1024) == -1)
        return EXIT_FAILURE;

    if(decode_flac_metadata(&data_input, &g_stream_info) == -1)
        return EXIT_FAILURE;

    if(init_alsa(argv[1], 1) == -1)
        return EXIT_FAILURE;

    if(get_alsa_buffer(&data_output, 0) == -1)
        return EXIT_FAILURE;

    if(decode_flac_data(&data_input, &data_output, g_stream_info.bits_per_sample, g_stream_info.nb_channels) == -1)
        return EXIT_FAILURE;

    if(dump_buffer_to_alsa(&data_output, 0, 1) == -1)
        return EXIT_FAILURE;

    snd_pcm_drain(g_pcm_handle);

    free(data_input.buffer);
    close(data_input.fd);
    snd_pcm_close(g_pcm_handle);

    return EXIT_SUCCESS;

}
