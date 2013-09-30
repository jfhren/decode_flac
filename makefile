CC := gcc
CFLAGS := -pedantic -W -Wall -Werror -std=c99 -g
SRC_DIR := ./src/
OBJ_DIR := ./obj/
BIN_DIR := ./bin/

all: mkd $(BIN_DIR)decode_flac_to_pcm

.SECONDEXPANSION:
$(BIN_DIR)decode_flac_to_pcm: $(OBJ_DIR)decode_flac.o $(OBJ_DIR)input.o $(OBJ_DIR)output.o $(OBJ_DIR)decode_flac_to_pcm.o
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)decode_flac_to_pcm.o: $(SRC_DIR)decode_flac_to_pcm.c $(SRC_DIR)decode_flac.h $(SRC_DIR)input.h $(SRC_DIR)output.h
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
