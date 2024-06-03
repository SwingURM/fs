MOUNTPOINT=./mountpoint
FS_LOG=./fs.log

start: fuse
	./build/fuse ${MOUNTPOINT} -d 2> ${FS_LOG} &

stop:
	fusermount -u ${MOUNTPOINT}

floppy: ext2.h floppy.h floppy.cpp device.h device.cpp
	mkdir build
	g++ floppy.cpp device.cpp -o ./build/floppy -g -DDEPLOY

fuse: ext2.h floppy.h floppy.cpp device.h device.cpp fuse.cpp
	mkdir build
	g++ floppy.cpp device.cpp fuse.cpp -o ./build/fuse -g -D_FILE_OFFSET_BITS=64 -lfuse3 -DFUSING -DDEPLOY

