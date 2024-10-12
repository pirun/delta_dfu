CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar


BIN_DIR := ../../binaries
IMG_DIR := $(BIN_DIR)/signed_images
PATCH_DIR := $(BIN_DIR)/patches
PATCH_PATH := $(PATCH_DIR)/patch.bin

DETOOLS := detools create_patch --compression heatshrink

# Install prefix.
PREFIX ?= /usr/local

OPT ?= -O2
CFLAGS += -g
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Wdouble-promotion
CFLAGS += -Wfloat-equal
CFLAGS += -Wformat=2
CFLAGS += -Wshadow
CFLAGS += -Werror
CFLAGS += -std=c99
CFLAGS += $(OPT)
CFLAGS += -Iheatshrink
CFLAGS += $(CFLAGS_EXTRA)

SRC += heatshrink/heatshrink_decoder.c
SRC += detools.c
SRC += main.c

all:
	$(CC) $(CFLAGS) $(SRC) -o detools

clean:
	rm -f detools detools.o libdetools.a $(PATCH_PATH) $(IMG_DIR)/new.bin

create-patch:
	@echo "Creating patch..."
	mkdir -p $(PATCH_DIR)
	rm -f $(PATCH_PATH)
	$(DETOOLS) $(IMG_DIR)/source_1.0.0.bin $(IMG_DIR)/target_1.1.0.bin $(PATCH_PATH)
	@echo "printing patch information..."
	detools patch_info $(PATCH_PATH)

apply-patch:
	@echo "Applying patch with C solution..." 
	./detools apply_patch \
	    $(IMG_DIR)/source_1.0.0.bin \
	    $(PATCH_DIR)/patch.bin \
	    $(IMG_DIR)/new.bin

# library:
# 	$(CC) $(CFLAGS) detools.c -c -o detools.o
# 	$(AR) cr libdetools.a detools.o

# install:
# 	mkdir -p $(PREFIX)/include
# 	install -c -m 644 detools.h $(PREFIX)/include
# 	mkdir -p $(PREFIX)/lib
# 	install -c -m 644 libdetools.a $(PREFIX)/lib
