build:
	touch my_ext2_filesystem.img
	gcc -o create_ext2 1.c -lext2fs -lcom_err -g
	./create_ext2
	# rm my_ext2_filesystem.img

build2:
	touch ext2_image.img
	gcc -o create_ext2 2.c -lext2fs -lcom_err -g
	./create_ext2

block: BlockDeviceTest.cpp device.h device.cpp
	g++ device.cpp BlockDeviceTest.cpp -o block

floppy: ext2.h floppy.h floppy.cpp device.h device.cpp
	g++ floppy.cpp device.cpp -o floppy -g

fuse: ext2.h floppy.h floppy.cpp device.h device.cpp fuse.cpp
	g++ floppy.cpp device.cpp fuse.cpp -o fuse -g -D_FILE_OFFSET_BITS=64 -lfuse3

