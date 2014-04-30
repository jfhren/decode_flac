CC := gcc
CFLAGS := -pedantic -W -Wall -Werror -std=c99 -g -DDECODE_8_BITS -DDECODE_12_BITS -DDECODE_16_BITS -DDECODE_20_BITS -DDECODE_24_BITS -DDECODE_32_BITS
#CFLAGS := -pedantic -W -Wall -Werror -std=c99 -pg -DDECODE_16_BITS -DSTEREO_ONLY
SRC_DIR := ./src/
OBJ_DIR := ./obj/
BIN_DIR := ./bin/

all: mkd $(BIN_DIR)decode_flac_to_pcm $(BIN_DIR)decode_flac_to_alsa $(BIN_DIR)get_aplay_param

.SECONDEXPANSION:
$(BIN_DIR)decode_flac_to_pcm: $(OBJ_DIR)decode_flac.o $(OBJ_DIR)input.o $(OBJ_DIR)output.o $(OBJ_DIR)decode_flac_to_pcm.o
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)decode_flac_to_pcm.o: $(SRC_DIR)decode_flac_to_pcm.c $(SRC_DIR)decode_flac.h $(SRC_DIR)input.h $(SRC_DIR)output.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)decode_flac_to_alsa: $(OBJ_DIR)decode_flac.o $(OBJ_DIR)input.o $(OBJ_DIR)output.o $(OBJ_DIR)decode_flac_to_alsa.o
	$(CC) $(CFLAGS) -lasound $^ -o $@

$(OBJ_DIR)decode_flac_to_alsa.o: $(SRC_DIR)decode_flac_to_alsa.c $(SRC_DIR)decode_flac.h $(SRC_DIR)input.h $(SRC_DIR)output.h
	$(CC) $(CFLAGS) -lasound -c $< -o $@

$(BIN_DIR)get_aplay_param: $(OBJ_DIR)decode_flac.o $(OBJ_DIR)input.o $(OBJ_DIR)output.o $(OBJ_DIR)get_aplay_param.o
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)get_aplay_param.o: $(SRC_DIR)get_aplay_param.c $(SRC_DIR)decode_flac.h $(SRC_DIR)input.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)decode_flac.o: $(SRC_DIR)decode_flac.c $(SRC_DIR)decode_flac.h $(SRC_DIR)input.h $(SRC_DIR)output.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)%.o: $(SRC_DIR)%.c $(SRC_DIR)%.h
	$(CC) $(CFLAGS) -c $< -o $@

mkd:
	mkdir -p $(BIN_DIR)
	mkdir -p $(OBJ_DIR)

clean:
	rm -Rf $(BIN_DIR) $(OBJ_DIR)
