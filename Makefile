MOUNTPOINT=./mountpoint
FS_LOG=./fs.log

start: fuse
	./build/fuse ${MOUNTPOINT} -d 2> ${FS_LOG} &

stop:
	fusermount -u ${MOUNTPOINT}

floppy: ext2.h floppy.h floppy.cpp device.h device.cpp util.h util.cpp
	mkdir build -p
	g++ floppy.cpp device.cpp util.cpp -o ./build/floppy -g -DDEPLOY -fsanitize=address

fuse: ext2.h floppy.h floppy.cpp device.h device.cpp fuse.cpp util.h util.cpp
	mkdir build -p
	g++ floppy.cpp device.cpp fuse.cpp util.cpp -o ./build/fuse -g -D_FILE_OFFSET_BITS=64 -lfuse3 -DFUSING -DDEPLOY -fsanitize=address

