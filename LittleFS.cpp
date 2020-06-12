/*
 * LittleFS.cpp
 *
 *  Created on: 11 Jun 2020
 *      Author: Casper
 */
#include "LittleFS.h"
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
    if (bd) {
        mount(bd);
    }
}

LittleFS::~LittleFS()
{
    //unmount();
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
    _config.block_count = bd->size() / _config.block_size;
//    _config.lookahead = 32 * ((_config.block_count + 31) / 32);
//    if (_config.lookahead > _lookahead) {
//        _config.lookahead = _lookahead;
//    }
    _config.read_buffer = readBuf;
    _config.prog_buffer = progBuf;
    _config.lookahead_buffer = lookaheadBuf;
//    _config.file_buffer = &fileBuf;

    err = lfs_mount(&_lfs, &_config);
    if (err) {
        _bd = NULL;
        return err;
    }

    return 0;
}


int LittleFS::format(lfs_size_t read_size, lfs_size_t prog_size,
                             lfs_size_t block_size, lfs_size_t lookahead)
{
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
    _config.block_count = _bd->size() / _config.block_size;
//    _config.lookahead = 32 * ((_config.block_count + 31) / 32);
//    if (_config.lookahead > _lookahead) {
//        _config.lookahead = _lookahead;
//    }
    _config.read_buffer = readBuf;
    _config.prog_buffer = progBuf;
    _config.lookahead_buffer = lookaheadBuf;
//    _config.file_buffer = &fileBuf;

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
