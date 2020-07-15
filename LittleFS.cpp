/*
 * LittleFS.cpp
 *
 *  Created on: 11 Jun 2020
 *      Author: Casper
 */
#include "LittleFS.h"
#include <stdio.h>
#include <string.h>

////// Block device operations //////
static int lfs_sd_read(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, void *buffer, lfs_size_t size)
{
    SDCard *sd = (SDCard *)c->context;
    return sd->read(buffer, (uint64_t)block * c->block_size + off, size);
}

static int lfs_sd_prog(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, const void *buffer, lfs_size_t size)
{
    SDCard *sd = (SDCard *)c->context;
    return sd->program(buffer, (uint64_t)block * c->block_size + off, size);
}

static int lfs_sd_erase(const struct lfs_config *c, lfs_block_t block)
{
    SDCard *sd = (SDCard *)c->context;
    return sd->erase((uint64_t)block * c->block_size, c->block_size);
}

static int lfs_sd_sync(const struct lfs_config *c)
{
    SDCard *sd = (SDCard *)c->context;
    return sd->sync();
}

LittleFS* _FSstub;

LittleFS::LittleFS(SDCard *bd, lfs_size_t read_size, lfs_size_t prog_size,
                                   lfs_size_t block_size, lfs_size_t lookahead)
    : _lfs()
    , _config()
    , _bd(0)
    , _read_size(read_size)
    , _prog_size(prog_size)
    , _block_size(block_size)
    , _lookahead(lookahead)
{
    workfile.cfg = &workfile_cfg;
    asyncBuffer._lfs = &_lfs;
    asyncBuffer.workfile = &workfile;
//    if (bd) {
//        mount(bd);
//    }
    _FSstub = this;
    this->userFunction = [](){_FSstub->TaskRun();};

}

LittleFS::~LittleFS()
{
    //unmount();
}

bool LittleFS::notified(){
    if(curOperation != 0){
        return true;
    }else{
        return false;
    }
}


void LittleFS::TaskRun(){
//    Console::log("FileSystemTask: %d", curOperation);
    int err = 0;
    switch(curOperation){
    case 1:
        //Case 1: mounting SD Card
        err = lfs_mount_async(&_lfs, &_config, &asyncBuffer);
        if(err){
            Console::log("Mounting Error: -%d", -err);
            asyncBuffer.operationState = 0;
            curOperation = 0;
            _mounted = false;
            _err = err;
        }
        if(asyncBuffer._operationComplete){
            Console::log("SD Mounted and lookahead populated..");
            asyncBuffer.operationState = 0;
            curOperation = 0;
            asyncBuffer._operationComplete = false;
            _mounted = true;
        }
        break;
    case 2:
        //Case 2: Format SD Card
        //todo
        break;
    case 3:
        //Case 3: Opening File
//        Console::log("OpenState: %d", asyncBuffer.operationState);
        err = lfs_file_open_async(&_lfs, &workfile, &asyncBuffer);
        if(err){
            Console::log("Opening Error: -%d", -err);
            asyncBuffer.operationState = 0;
            curOperation = 0;
            _err = err;
        }
        if(asyncBuffer._operationComplete){
            Console::log("File Opened.");
            asyncBuffer.operationState = 0;
            curOperation = 0;
            asyncBuffer._operationComplete = false;
            _opened = true;
        }
        break;
    case 4:
        //Case 4 5 6: Open Write Close
//        Console::log("OpenState: %d", asyncBuffer.operationState);
        err = lfs_file_open_async(&_lfs, &workfile, &asyncBuffer);
        if(err){
            Console::log("Opening Error: -%d", -err);
            asyncBuffer.operationState = 0;
            curOperation = 0;
            _err = err;
        }
        if(asyncBuffer._operationComplete){
//            Console::log("File Opened.");
            asyncBuffer.operationState = 0;
            curOperation = 5;
            asyncBuffer._operationComplete = false;
            _opened = true;
        }
        break;
    case 5:
        //Case 4 5 6: Open Write Close
        err = file_write(&workfile, writeBuffer, writeSize);
        if(err < 0){
            Console::log("Writing Error: -%d", -err);
            asyncBuffer.operationState = 0;
            curOperation = 0;
            _err = err;
        }else{
            curOperation = 6;
        }
        break;
    case 6:
        err = file_close(&workfile);
        if(err){
            Console::log("Close Error: -%d", -err);
            asyncBuffer.operationState = 0;
            curOperation = 0;
            _err = err;
        }else{
            curOperation = 0;
            Console::log("OWC Succes!");
        }
        break;
    default:
        Console::log("Unknown Operation!");
        curOperation = 0;
        break;
    }
}

bool LittleFS::isBusy(){
    return (curOperation) != 0;
}

int LittleFS::mount_async(SDCard *bd)
{
    _bd = bd;
    int err = _bd->init();
    if (err) {
        _bd = 0;
        return err;
    }

    memset(&_config, 0, sizeof(_config));
    _config.context = bd;
    _config.read  = lfs_sd_read;
    _config.prog  = lfs_sd_prog;
    _config.erase = lfs_sd_erase;
    _config.sync  = lfs_sd_sync;

    _config.read_size   = bd->get_read_size();
    if (_config.read_size < _read_size) {
        _config.read_size = _read_size;
    }
    _config.prog_size   = bd->get_program_size();
    if (_config.prog_size < _prog_size) {
        _config.prog_size = _prog_size;
    }
    _config.block_size  = bd->get_erase_size();
    if (_config.block_size < _block_size) {
        _config.block_size = _block_size;
    }
    _config.lookahead_size = _lookahead;
    _config.block_count = bd->size() / _config.block_size;

    // block device configuration
    _config.cache_size = LFS_CACHE_SIZE;
    _config.block_cycles = LFS_BLOCKCYCLES;

    _config.read_buffer = readBuf;
    _config.prog_buffer = progBuf;
    _config.lookahead_buffer = lkahBuf;

    //Initialize with 0, to avoid some random value sitting there.
    _config.name_max = 0;
    _config.file_max = 0;
    _config.attr_max = 0;

//    err = lfs_mount(&_lfs, &_config);
//    if (err) {
//        _bd = NULL;
//        return (err);
//    }
    curOperation = 1; //start mounting process.

    return 0;
}

int LittleFS::mount(SDCard *bd)
{
    _bd = bd;
    int err = _bd->init();
    if (err) {
        _bd = 0;
        return err;
    }

    memset(&_config, 0, sizeof(_config));
    _config.context = bd;
    _config.read  = lfs_sd_read;
    _config.prog  = lfs_sd_prog;
    _config.erase = lfs_sd_erase;
    _config.sync  = lfs_sd_sync;

    _config.read_size   = bd->get_read_size();
    if (_config.read_size < _read_size) {
        _config.read_size = _read_size;
    }
    _config.prog_size   = bd->get_program_size();
    if (_config.prog_size < _prog_size) {
        _config.prog_size = _prog_size;
    }
    _config.block_size  = bd->get_erase_size();
    if (_config.block_size < _block_size) {
        _config.block_size = _block_size;
    }
    _config.lookahead_size = _lookahead;
    _config.block_count = bd->size() / _config.block_size;

    // block device configuration
    _config.cache_size = LFS_CACHE_SIZE;
    _config.block_cycles = LFS_BLOCKCYCLES;

    _config.read_buffer = readBuf;
    _config.prog_buffer = progBuf;
    _config.lookahead_buffer = lkahBuf;

    //Initialize with 0, to avoid some random value sitting there.
    _config.name_max = 0;
    _config.file_max = 0;
    _config.attr_max = 0;

    err = lfs_mount(&_lfs, &_config);
    if (err) {
        _bd = NULL;
        return (err);
    }

    return 0;
}


int LittleFS::unmount()
{
    int res = 0;
    if (_bd) {
        int err = lfs_unmount(&_lfs);  //releases resources..
        if (err && !res) {
            res = (err);
        }

        err = _bd->deinit(); //does actually nothing, deinit an sd?
        if (err && !res) {
            res = err;
        }

        _bd = 0;
    }
    return res;
}

int LittleFS::format(SDCard *bd, lfs_size_t read_size, lfs_size_t prog_size,
                             lfs_size_t block_size, lfs_size_t lookahead)
{
    _bd = bd;
    int err = _bd->init();
    if (err) {
        _bd = 0;
        return err;
    }

    memset(&_config, 0, sizeof(_config));
    _config.context = _bd;
    _config.read  = lfs_sd_read;
    _config.prog  = lfs_sd_prog;
    _config.erase = lfs_sd_erase;
    _config.sync  = lfs_sd_sync;
    _config.read_size   = _bd->get_read_size();
    if (_config.read_size < _read_size) {
        _config.read_size = _read_size;
    }
    _config.prog_size   = _bd->get_program_size();
    if (_config.prog_size < _prog_size) {
        _config.prog_size = _prog_size;
    }
    _config.block_size  = _bd->get_erase_size();
    if (_config.block_size < _block_size) {
        _config.block_size = _block_size;
    }
    _config.lookahead_size = lookahead;
    _config.block_count = _bd->size() / _config.block_size;

    // block device configuration
    _config.cache_size = LFS_CACHE_SIZE;
    _config.block_cycles = LFS_BLOCKCYCLES;

    _config.read_buffer = readBuf;
    _config.prog_buffer = progBuf;
    _config.lookahead_buffer = lkahBuf;

    //Initialize with 0, to avoid some random value sitting there.
    _config.name_max = 0;
    _config.file_max = 0;
    _config.attr_max = 0;

    err = lfs_format(&_lfs, &_config);
    if (err) {
        return (err);
    }

    err = _bd->deinit();
    if (err) {
        return err;
    }

    return 0;
}

int LittleFS::remove(const char *filename)
{
    int err = lfs_remove(&_lfs, filename);
    return (err);
}

int LittleFS::rename(const char *oldname, const char *newname)
{
    int err = lfs_rename(&_lfs, oldname, newname);
    return (err);
}

int LittleFS::stat(const char *name, lfs_info *info)
{
    int err = lfs_stat(&_lfs, name, info);
    return (err);
}

int LittleFS::mkdir(const char *name)
{
    int err = lfs_mkdir(&_lfs, name);
    return (err);
}

int LittleFS::statvfs(const char *name, statvfs_t *st)
{
    memset(st, 0, sizeof(struct statvfs));

    lfs_ssize_t in_use = 0;
    in_use = lfs_fs_size(&_lfs);
    if (in_use < 0) {
        return in_use;
    }

    st->f_bsize  = _config.block_size;
    st->f_frsize = _config.block_size;
    st->f_blocks = _config.block_count;
    st->f_bfree  = _config.block_count - in_use;
    st->f_bavail = _config.block_count - in_use;
    st->f_namemax = LFS_NAME_MAX;
    return 0;
}


int LittleFS::file_open_async(char *path, int flags)
{
    if (workfile.flags & LFS_F_OPENED) {
        return LFS_ERR_OPEN;
    }
    if(_mounted){
        curOperation = 3; //open routine
        asyncBuffer.operationState = 0;
        snprintf(asyncBuffer.workpath, sizeof(asyncBuffer.workpath), "%s", path);
        Console::log("new WorkPath: %s", asyncBuffer.workpath);
        asyncBuffer.workflags = flags;
        asyncBuffer.workint0 = 0;
        return 0;
    }else{
        return -1;
    }
}

int LittleFS::file_open_write_close_async(char *path, int flags, const void *buffer, size_t size)
{
    if (workfile.flags & LFS_F_OPENED) {
        return LFS_ERR_OPEN;
    }
    if(_mounted){
        curOperation = 4; //open/write/close routine
        asyncBuffer.operationState = 0;
        snprintf(asyncBuffer.workpath, sizeof(asyncBuffer.workpath), "%s", path);
//        Console::log("new WorkPath: %s", asyncBuffer.workpath);
        asyncBuffer.workflags = flags;
        asyncBuffer.workint0 = 0;

        writeSize = size;
        //copy data to writebuffer
        for(int i = 0; i < writeSize; i++){
            writeBuffer[i] = ((uint8_t*) buffer)[i];
        }

        return 0;
    }else{
        return -1;
    }
}

int LittleFS::file_close(lfs_file_t *file)
{
    int err = 0;
    if (!(file->flags & LFS_F_OPENED)) {
        err = LFS_ERR_NOTOPEN;
    }else{
        err = lfs_file_close(&_lfs, file);
    }
    _opened = false;
    return (err);
}

ssize_t LittleFS::file_read(lfs_file_t *file, void *buffer, size_t len)
{
    lfs_ssize_t res = lfs_file_read(&_lfs, file, buffer, len);
    return (res);
}

ssize_t LittleFS::file_write(lfs_file_t *file, const void *buffer, size_t len)
{
    lfs_ssize_t res = lfs_file_write(&_lfs, file, buffer, len);
    return (res);
}

int LittleFS::file_sync(lfs_file_t *file)
{
    int err = lfs_file_sync(&_lfs, file);
    return (err);
}

off_t LittleFS::file_seek(lfs_file_t *file, off_t offset, int whence)
{
    off_t res = lfs_file_seek(&_lfs, file, offset, whence);
    return (res);
}

off_t LittleFS::file_tell(lfs_file_t *file)
{
    off_t res = lfs_file_tell(&_lfs, file);
    return (res);
}

off_t LittleFS::file_size(lfs_file_t *file)
{
    off_t res = lfs_file_size(&_lfs, file);
    return (res);
}

int LittleFS::file_truncate(lfs_file_t *file, off_t length)
{
    int err = lfs_file_truncate(&_lfs, file, length);
    return (err);
}


////// Dir operations //////
int LittleFS::dir_open(lfs_dir_t *dir, const char *path)
{
    int err = lfs_dir_open(&_lfs, dir, path);
    return (err);
}

int LittleFS::dir_close(lfs_dir_t *dir)
{
    int err = lfs_dir_close(&_lfs, dir);
    return (err);
}

ssize_t LittleFS::dir_read(lfs_dir_t *dir, lfs_info *info)
{
    int res = lfs_dir_read(&_lfs, dir, info);
    return (res);
}

void LittleFS::dir_seek(lfs_dir_t *dir, off_t offset)
{
    lfs_dir_seek(&_lfs, dir, offset);
}

off_t LittleFS::dir_tell(lfs_dir_t *dir)
{
    lfs_soff_t res = lfs_dir_tell(&_lfs, dir);
    return (res);
}

void LittleFS::dir_rewind(lfs_dir_t *dir)
{
    lfs_dir_rewind(&_lfs, dir);
}
