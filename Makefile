MOUNTPOINT = ./mountpoint
FS_LOG = ./fs.log
BUILD_DIR = ./build
CXX = g++
# CXXFLAGS = -g -DDEPLOY -fsanitize=address -Wall -Wextra
CXXFLAGS = -g -DDEPLOY -fsanitize=address
FUSE_FLAGS = -D_FILE_OFFSET_BITS=64 -lfuse3 -DFUSING
SRC_FILES = floppy.cpp device.cpp util.cpp BlockManager.cpp InodeManager.cpp
FUSE_SRC_FILES = $(SRC_FILES) fuse.cpp
HEADERS = ext2.h floppy.h device.h util.h BlockManager.h InodeManager.cpp

.PHONY: all clean start stop floppy fuse

all: floppy fuse

start: fuse
	./build/fuse ${MOUNTPOINT} -d 2> ${FS_LOG} &

stop:
	fusermount -u ${MOUNTPOINT}

floppy: $(HEADERS) $(SRC_FILES)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(SRC_FILES) -o $(BUILD_DIR)/floppy $(CXXFLAGS)

fuse: $(HEADERS) $(FUSE_SRC_FILES)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(FUSE_SRC_FILES) -o $(BUILD_DIR)/fuse $(CXXFLAGS) $(FUSE_FLAGS)

clean:
	rm -rf $(BUILD_DIR) ${FS_LOG}