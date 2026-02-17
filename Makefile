CFLAGS = -Wall -Wextra -g -I./src -I./src/vendor -I./src/vendor/raylib/include
LDFLAGS = ./src/vendor/raylib/lib/libraylib.a -lm -lpthread -ldl

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Find all source files in src/ and src/vendor/ (non-recursive)
SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/vendor/*.c)
# Generate object filenames
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
# Handle OBJs that are in vendor subdir
OBJS := $(patsubst $(SRC_DIR)/vendor/%.c, $(OBJ_DIR)/vendor/%.o, $(SRCS))
# Fix the above logic: patsubst is simple string replacement.
# Let's be explicit to avoid issues with directory mapping.

SRCS_MAIN = $(wildcard $(SRC_DIR)/*.c)
SRCS_VENDOR = $(wildcard $(SRC_DIR)/vendor/*.c)

OBJS_MAIN = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_MAIN))
OBJS_VENDOR = $(patsubst $(SRC_DIR)/vendor/%.c, $(OBJ_DIR)/vendor/%.o, $(SRCS_VENDOR))

OBJS = $(OBJS_MAIN) $(OBJS_VENDOR)

# Binary name
TARGET = $(BIN_DIR)/audio_clipper

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/vendor/%.o: $(SRC_DIR)/vendor/%.c | $(OBJ_DIR)/vendor
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/vendor:
	mkdir -p $(OBJ_DIR)/vendor

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
