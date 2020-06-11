/*
 * LittleFS.h
 *
 *  Created on: 11 Jun 2020
 *      Author: Casper
 */

#ifndef LITTLEFS_LITTLEFS_H_
#define LITTLEFS_LITTLEFS_H_

#include "littlefs/lfs.h"
#include "SDCard.h"

#define LFS_READ_SIZE   512
#define LFS_PROG_SIZE   512
#define LFS_BLOCK_SIZE  64
#define LFS_LOOKAHEAD   64

class LittleFS {
public:
    LittleFS(SDCard *sd = 0,
                          lfs_size_t read_size = LFS_READ_SIZE,
                          lfs_size_t prog_size  = LFS_PROG_SIZE,
                          lfs_size_t block_size = LFS_BLOCK_SIZE,
                          lfs_size_t lookahead = LFS_LOOKAHEAD);
    ~LittleFS();

    int mount(SDCard *bd);

    int format(lfs_size_t read_size = LFS_READ_SIZE,
                          lfs_size_t prog_size = LFS_PROG_SIZE,
                          lfs_size_t block_size = LFS_BLOCK_SIZE,
                          lfs_size_t lookahead = LFS_LOOKAHEAD);


private:
    lfs_t _lfs; // The actual file system
    struct lfs_config _config;
    SDCard *_bd; // The block device

    uint8_t readBuf[LFS_READ_SIZE];
    uint8_t progBuf[LFS_PROG_SIZE];
    uint8_t lookaheadBuf[LFS_LOOKAHEAD];
    lfs_file_t fileBuf;

    // default parameters
    const lfs_size_t _read_size;
    const lfs_size_t _prog_size;
    const lfs_size_t _block_size;
    const lfs_size_t _lookahead;
};



#endif /* LITTLEFS_LITTLEFS_H_ */
