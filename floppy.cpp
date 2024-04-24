#include "floppy.h"
#include <iostream>
#include <cstring>
#include <ctime>

FloppyDisk::FloppyDisk(const std::string& filename) : bd_(filename) {
    if (!bd_.initialize()) {
        std::cerr << "Unable to initialize floppy disk." << std::endl;
        exit(-1);
    }
    auto sb = bd_.bread(1);
    memcpy(&sb_, sb->s_, sizeof(SuperBlock));
    free(sb);
}

void FloppyDisk::initialize() {
    // Initialize superblock
    // sb_.s_inodes_count_ = ;
    sb_.s_blocks_count_ = 1439;
    // sb_.s_r_blocks_count_ = ;
    // sb_.s_free_blocks_count_ = 1439;
    // sb_.s_free_inodes_count_ = ;
    sb_.s_first_data_block_ = 1;
    // sb_.s_log_block_size_ = 0;
    // sb_.s_log_frag_size_ = ;
    sb_.s_blocks_per_group_ = 1439;
    // sb_.s_frags_per_group_ = ;
    sb_.s_inodes_per_group_ = sb_.s_blocks_per_group_ * (PAGE_SIZE / sizeof(inode));
    sb_.s_mtime_ = time(nullptr);
    sb_.s_wtime_ = time(nullptr);
    // sb_.s_mnt_count_ = ;
    // sb_.s_max_mnt_count_ = ;
    sb_.s_magic_ = EXT2_SUPER_MAGIC;
    // TODO 假设这里是挂载
    sb_.s_state_ = EXT2_ERROR_FS;



    // Initialize block group descriptor table

    // Initialize block bitmap

    // Initialize inode bitmap

    // Initialize inode table
}


FloppyDisk::~FloppyDisk() {
    struct block b;
    b.blockNo_ = 1;
    memcpy(b.s_, &sb_, sizeof(SuperBlock));
    bd_.bwrite(b);
}