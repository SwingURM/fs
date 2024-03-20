build:
	touch my_ext2_filesystem.img
	gcc -o create_ext2 1.c -lext2fs -lcom_err -g
	./create_ext2
	# rm my_ext2_filesystem.img


build2:
	touch ext2_image.img
	gcc -o create_ext2 2.c -lext2fs -lcom_err -g
	./create_ext2


run: device.h device.cpp
	g++ device.cpp -o fs
	./fs