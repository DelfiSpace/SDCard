/*
 * LittleFS.h
 *
 *  Created on: 11 Jun 2020
 *  Heavily based on: https://github.com/ARMmbed/mbed-os/blob/master/features/storage/filesystem/littlefsv2/LittleFileSystem2.h
 *
 */

#ifndef LITTLEFS_LITTLEFS_H_
#define LITTLEFS_LITTLEFS_H_

#include "littlefs/lfs.h"
#include "SDCard.h"
#include "Task.h"
#include "Console.h"

#define LFS_BLOCK_SIZE  512
#define LFS_READ_SIZE   512
#define LFS_PROG_SIZE   512
#define LFS_CACHE_SIZE  512
#define LFS_LOOKAHEAD   8192 //10*8192
#define LFS_BLOCKCYCLES -1

typedef signed   int  ssize_t;  ///< Signed size type, usually encodes negative errors
typedef unsigned int  size_t;
typedef signed   long off_t;    ///< Offset in a data stream
typedef unsigned long long fsblkcnt_t;  ///< Count of file system blocks

typedef struct statvfs {
     unsigned long  f_bsize;    ///< Filesystem block size
     unsigned long  f_frsize;   ///< Fragment size (block size)
     fsblkcnt_t     f_blocks;   ///< Number of blocks
     fsblkcnt_t     f_bfree;    ///< Number of free blocks
     fsblkcnt_t     f_bavail;   ///< Number of free blocks for unprivileged users
     unsigned long  f_fsid;     ///< Filesystem ID
     unsigned long  f_namemax;  ///< Maximum filename length
 } statvfs_t;

class LittleFS : public Task{
public:
    LittleFS(SDCard *sd = 0,
                          lfs_size_t read_size = LFS_READ_SIZE,
                          lfs_size_t prog_size  = LFS_READ_SIZE,
                          lfs_size_t block_size = LFS_BLOCK_SIZE,
                          lfs_size_t lookahead = LFS_LOOKAHEAD);
    ~LittleFS();
    int format(SDCard *sd,
                          lfs_size_t read_size = LFS_READ_SIZE,
                          lfs_size_t prog_size = LFS_READ_SIZE,
                          lfs_size_t block_size = LFS_BLOCK_SIZE,
                          lfs_size_t lookahead = LFS_LOOKAHEAD);

    int mount(SDCard *bd);
    int mount_async(SDCard *bd);
    int unmount();

    //Remove a file from the file system.
    int remove(const char *path);

    // Rename a file in the file system.
    int rename(const char *path, const char *newpath);

    // Store information about the file in a stat structure
    int stat(const char *path, lfs_info *st);

    // Create a directory in the file system.
    int mkdir(const char *path);

    // Store information about the mounted file system in a statvfs structure.
    int statvfs(const char *path, struct statvfs *buf);

    // Open a file on the file system.
    int file_open_async(char *path, int flags);

    // Close a file
    int file_close(lfs_file_t *file);

    // Read the contents of a file into a buffer
    ssize_t file_read(lfs_file_t *file, void *buffer, size_t size);

    // Write the contents of a buffer to a file
    ssize_t file_write(lfs_file_t *file, const void *buffer, size_t size);

    // Flush any buffers associated with the file
    int file_sync(lfs_file_t *file);

    // Move the file position to a given offset from a given location
    off_t file_seek(lfs_file_t *file, off_t offset, int whence);

    // Get the file position of the file
    off_t file_tell(lfs_file_t *file);

    // Get the size of the file
    off_t file_size(lfs_file_t *file);

    // Truncate or extend a file.
    int file_truncate(lfs_file_t *file, off_t length);

    // Open a directory on the file system.
    int dir_open(lfs_dir_t *dir, const char *path);

    // Close a directory
    int dir_close(lfs_dir_t *dir);

    // Read the next directory entry
    ssize_t dir_read(lfs_dir_t *dir, lfs_info *ent);

    // Set the current position of the directory
    void dir_seek(lfs_dir_t *dir, off_t offset);

    // Get the current position of the directory
    off_t dir_tell(lfs_dir_t *dir);

    // Rewind the current position to the beginning of the directory
    void dir_rewind(lfs_dir_t *dir);

    lfs_t _lfs; // The actual file system
    lfs_file_t workfile; //filebuffer
    lfs_workbuffer asyncBuffer;

    uint8_t writeBuffer[1024]; //twoblock writeBuffer;
    int writeSize; //number of bytes to write.

    bool isBusy();
    int file_open_write_close_async(char *path, int flags, const void *buffer, size_t size);

    void TaskRun();

    virtual bool notified();
    bool _mounted = false;
    bool _opened = false;
    int _err = 0;

    uint8_t curOperation = 0; //0: idle, 1: mounting, 2: formatting, 3: writing, 4: reading

private:
    struct lfs_config _config;
    SDCard *_bd; // The block device

    uint8_t readBuf[LFS_READ_SIZE];
    uint8_t progBuf[LFS_PROG_SIZE];
    uint8_t lkahBuf[LFS_LOOKAHEAD];

    // default parameters
    const lfs_size_t _read_size;
    const lfs_size_t _prog_size;
    const lfs_size_t _block_size;
    const lfs_size_t _lookahead;

    //file handling object
    uint8_t fileBuf[LFS_CACHE_SIZE];
    const struct lfs_file_config workfile_cfg = {.buffer=fileBuf};
};



#endif /* LITTLEFS_LITTLEFS_H_ */
