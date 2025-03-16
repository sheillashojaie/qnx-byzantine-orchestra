CC = qcc
CFLAGS = -Wall -g
LDFLAGS = -lm
TARGET = byzantine_orchestra
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/conductor.c \
       $(SRC_DIR)/musician.c \
       $(SRC_DIR)/utils.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

INCLUDES = -I$(SRC_DIR)

DIRS = $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean

all: $(DIRS) $(BIN_DIR)/$(TARGET)

$(DIRS):
	mkdir -p $@

$(BIN_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)