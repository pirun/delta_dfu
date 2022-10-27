BOARD := nrf9160dk_nrf9160ns
PY := python 

#device flash map
SLOT0_SIZE := 0xa0000
SLOT1_SIZE := 0x50000
HEADER_SIZE := 512
SLOT0_OFFSET := 0x10000
SLOT1_OFFSET := 0xb0000
PATCH_OFFSET := $(SLOT1_OFFSET)
MAX_PATCH_SIZE := $(SLOT1_SIZE)
PATCH_HEADER_SIZE := 0x8 

#SLOT0_SIZE := 0xa0000
#SLOT1_SIZE := 0x30000
#HEADER_SIZE := 512
#SLOT0_OFFSET := 0x10000
#SLOT1_OFFSET := 0xb0000
#PATCH_OFFSET := 0xe0000
#MAX_PATCH_SIZE := 0x20000
#PATCH_HEADER_SIZE := 0x8

#relevant directories that the user might have to update
BOOT_DIR := bootloader/mcuboot/boot/zephyr#bootloader image location
BUILD_DIR := zephyr/build#zephyr build directory
KEY_PATH := bootloader/mcuboot/root-rsa-2048.pem#key for signing images

#Names of generated folders and files (can be changed to whatever)
BIN_DIR := binaries
IMG_DIR := $(BIN_DIR)/signed_images
PATCH_DIR := $(BIN_DIR)/patches
DUMP_DIR := $(BIN_DIR)/flash_dumps

SOURCE_PATH := $(IMG_DIR)/source.bin
TARGET_PATH := $(IMG_DIR)/target.bin
PATCH_PATH := $(PATCH_DIR)/patch.bin
REVERSE_PATCH_PATH := $(PATCH_DIR)/reverse_patch.bin
SLOT0_PATH := $(DUMP_DIR)/slot0.bin
SLOT1_PATH := $(DUMP_DIR)/slot1.bin
PATCH_SLOT_PATH := $(DUMP_DIR)/patch.bin
TARGET_APPLY_PATH := $(DUMP_DIR)/target.bin

#commands + flags and scripts
PYERASE := pyocd erase --sector 
PYFLASH := pyocd flash -e sector 
DETOOLS := detools create_patch --compression heatshrink
IN_PLACE_DETOOLS := detools create_patch_in_place --memory-size 3000 --segment-size 500 --compression heatshrink
BUILD_APP := west build -p auto -b $(BOARD) -d $(BUILD_DIR)
SIGN := west sign -t imgtool -d $(BUILD_DIR)
IMGTOOL_SETTINGS := --version 1.0 --header-size $(HEADER_SIZE) \
                    --slot-size $(SLOT_SIZE) --align 4 --key $(KEY_PATH)
PAD_SCRIPT := $(PY) scripts/pad_patch.py
DUMP_SCRIPT := $(PY) scripts/jflashrw.py read
SET_SCRIPT := $(PY) scripts/set_current.py 

all: build-boot flash-boot build flash-image

help:
	@echo "Make commands that may be utilized:"	
	@echo "all                Build + flash bootloader and build"
	@echo "                   + flash firmware."
	@echo "build              Build the firmware image."		
	@echo "build-boot         Build the bootloader."
	@echo "erase-slot1        erase flash slot1 partirion."
	@echo "flash-image        Flash the firmware image."
	@echo "flash-target       Flash the delta-apply image."
	@echo "flash-slot1        Flash the slot1 image."
	@echo "flash-apply-target       Flash the PC applied image."
	@echo "flash-boot         Erase the flash and flash the bootloader."
	@echo "flash-patch        Flash the patch to the storage partition."
	@echo "create-patch       1. Create a patch based on the firmware"
	@echo "                     image and the upgraded firmware image."
	@echo "                   2. Append NEWPATCH and patch size to"
	@echo "                     the beginning of the image."
	@echo "apply-patch        Applying patch to generate new image and save to binaries/flash_dumps/target.bin."
	@echo "connect            Connect to the device terminal."
	@echo "dump-flash         Dump slot 1 and 0 to files."
	@echo "clean              Remove all generated binaries."
	@echo "tools              Install used tools."

build:
	@echo "Building firmware image..."	
	mkdir -p $(BUILD_DIR)
	mkdir -p $(IMG_DIR)
	$(BUILD_APP) app
	$(SIGN) -B $(TARGET_PATH) -- $(IMGTOOL_SETTINGS)

build-boot:
	@echo "Building bootloader..."	
	mkdir -p $(BOOT_DIR)/build
	cmake -B $(BOOT_DIR)/build -GNinja -DBOARD=$(BOARD) -S $(BOOT_DIR)
	ninja -C $(BOOT_DIR)/build

erase-slot0:
	@echo "erase all the slot0 sector..."
	$(PYERASE) $(SLOT0_OFFSET)+$(SLOT0_SIZE) -t nrf9160_xxaa

erase-slot1:
	@echo "erase all the slot1 sector..."
	$(PYERASE) $(SLOT1_OFFSET)+$(SLOT1_SIZE) -t nrf9160_xxaa
	
flash-image:
	@echo "Flashing latest source image to slot 0..."
#	$(SET_SCRIPT) $(TARGET_PATH) $(SOURCE_PATH)
	$(PYFLASH) -a $(SLOT0_OFFSET) -t nrf9160_xxaa $(SOURCE_PATH)

flash-target:
	@echo "Flashing latest source image to slot 0..."
	$(PYFLASH) -a $(SLOT0_OFFSET) -t nrf9160_xxaa $(TARGET_PATH)

flash-slot0:
	@echo "Flashing delta-apply image to slot 0..."
	$(PYFLASH) -a $(SLOT0_OFFSET) -t nrf9160_xxaa $(SLOT0_PATH)

flash-slot1:
	@echo "Flashing delta-apply image to slot 0..."
	$(PYFLASH) -a $(SLOT0_OFFSET) -t nrf9160_xxaa $(SLOT1_PATH)

flash-apply-target:
	@echo "Flashing PC delta-applied image to slot 0..."
	$(PYFLASH) -a $(SLOT0_OFFSET) -t nrf9160_xxaa $(TARGET_APPLY_PATH)

flash-boot:
	@echo "Flashing latest bootloader image..."	
	ninja -C $(BOOT_DIR)/build flash

flash-patch:
	@echo "Flashing latest patch to patch partition..."
	$(PYFLASH) -a $(PATCH_OFFSET) -t nrf9160_xxaa $(PATCH_PATH)
	$(SET_SCRIPT) $(TARGET_PATH) $(SOURCE_PATH)
	
create-patch:
	@echo "Creating patch..."
	mkdir -p $(PATCH_DIR)
	rm -f $(PATCH_PATH)
	$(DETOOLS) $(SOURCE_PATH) $(TARGET_PATH) $(PATCH_PATH)
	$(PAD_SCRIPT) $(PATCH_PATH) $(MAX_PATCH_SIZE) $(PATCH_HEADER_SIZE)
#	$(IN_PLACE_DETOOLS) $(SOURCE_PATH) $(TARGET_PATH) $(PATCH_PATH)		//test in-place patch 

apply-patch:
	@echo "Applying patch..."
	mkdir -p $(PATCH_DIR)
	rm -f $(TARGET_APPLY_PATH)
	detools apply_patch $(SOURCE_PATH) $(PATCH_PATH) $(TARGET_APPLY_PATH)

create-reverse-patch:
	@echo "Creating reverse patch..."
	mkdir -p $(PATCH_DIR)
	rm -f $(REVERSE_PATCH_PATH)
	$(DETOOLS) $(TARGET_PATH) $(SOURCE_PATH) $(REVERSE_PATCH_PATH)
	$(PAD_SCRIPT) $(REVERSE_PATCH_PATH) $(MAX_PATCH_SIZE) $(PATCH_HEADER_SIZE)
	
connect:
	@echo "Connecting to device console.."
	JLinkRTTLogger -device nRF9160 -if SWD -speed 5000 -rttchannel 0 /dev/stdout

dump-flash: dump-slot0 dump-slot1 dump-patch

dump-slot0:
	@echo "Dumping slot 0 contents.."
	mkdir -p $(DUMP_DIR)
	rm -f $(SLOT0_PATH)
	touch $(SLOT0_PATH)
	$(DUMP_SCRIPT) --start $(SLOT0_OFFSET) --length $(SLOT0_SIZE) --file $(SLOT0_PATH)

dump-slot1:
	@echo "Dumping slot 1 contents.."
	mkdir -p $(DUMP_DIR)
	rm -f $(SLOT1_PATH)
	touch $(SLOT1_PATH)
	$(DUMP_SCRIPT) --start $(SLOT1_OFFSET) --length $(SLOT1_SIZE) --file $(SLOT1_PATH)

dump-patch:
	@echo "Dumping patch contents.."
	mkdir -p $(DUMP_DIR)
	rm -f $(PATCH_SLOT_PATH)
	touch $(PATCH_SLOT_PATH)
	$(DUMP_SCRIPT) --start $(PATCH_OFFSET) --length $(MAX_PATCH_SIZE) --file $(PATCH_SLOT_PATH)

clean:
	rm -r -f $(BIN_DIR)

tools:
	@echo "Installing tools..."
	pip3 install --user detools
	pip3 install --user -r bootloader/mcuboot/scripts/requirements.txt
	pip3 install --user pyocd
	pip3 install --user pynrfjprog
	pip3 install --user imgtool
	pyocd pack install nRF9160_xxAA
	@echo "Done"
