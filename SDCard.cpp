/*
 * SDCard.cpp
 *
 *  Created on: 10 Jun 2020
 *
 *      Based on: https://github.com/ARMmbed/mbed-os/blob/master/components/storage/blockdevice/COMPONENT_SD/SDCard.cpp
 */



#include "SDCard.h"
#include <stdint.h>

#define FILLER 0xff
#define SPI_CMD(x) (0x40 | (x & 0x3f))
#define SD_COMMAND_TIMEOUT  5000 //ms of timeout (TODO)
#define SD_CMD0_GO_IDLE_STATE_RETRIES   10
const uint32_t SDCard::_block_size = BLOCK_SIZE_HC;

SDCard::SDCard(DSPI_A* DSPI_in, uint32_t CS_port, uint32_t CS_pin){
    this->_spi = DSPI_in;
    this->CS_PIN = CS_pin;
    this->CS_PORT = CS_port;
    //chip select
    MAP_GPIO_setAsOutputPin(CS_PORT, CS_PIN);
    MAP_GPIO_setOutputHighOnPin( CS_PORT, CS_PIN );

    _card_type = SDCARD_NONE;

    // Set default to 100kHz for initialisation and 1MHz for data transfer
    _init_sck = SD_INIT_FREQUENCY;
    _transfer_sck = SD_TRX_FREQUENCY;
    _erase_size = BLOCK_SIZE_HC;
    _is_initialized = 0;
    _sectors = 0;
}

SDCard::~SDCard()
{
    if (_is_initialized) {
        deinit();
    }
}

int SDCard::_initialise_card()
{
    uint32_t response, arg;
    _spi_init(); //init SPI with low-speed and send dummy clocks

    // The card is transitioned from SDCard mode to SPI mode by sending the CMD0 + CS Asserted("0")
    if (_go_idle_state() != R1_IDLE_STATE) {
        return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;
    }

    long status = SD_BLOCK_DEVICE_OK;

    // Send CMD8, if the card rejects the command then it's probably using the
    // legacy protocol, or is a MMC, or just flat-out broken
    status = _cmd8();
    if (SD_BLOCK_DEVICE_OK != status && SD_BLOCK_DEVICE_ERROR_UNSUPPORTED != status) {
        return status;
    }

    if (_crc_on) {
        // Enable CRC TODO
        status = _cmd(CMD59_CRC_ON_OFF, _crc_on);
    }

    // Read OCR - CMD58 Response contains OCR register
    if (SD_BLOCK_DEVICE_OK != (status = _cmd(CMD58_READ_OCR, 0x0, 0x0, &response))) {
        return status;
    }

    // Check if card supports voltage range: 3.3V
    if (!(response & OCR_3_3V)) {
        _card_type = CARD_UNKNOWN;
        status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
        return status;
    }

    // HCS is set 1 for HC/XC capacity cards for ACMD41, if supported
    arg = 0x0;
    if (SDCARD_V2 == _card_type) {
        arg |= OCR_HCS_CCS;
    }

    /* Repeatedly issue ACMD41 until R1 is set to "0".
     */
    //TODO: Fix infinite loop problem
    do {
        status = _cmd(ACMD41_SD_SEND_OP_COND, arg, 1, &response);
    } while ((response & R1_IDLE_STATE));


    // Initialisation complete: ACMD41 successful
    if ((SD_BLOCK_DEVICE_OK != status) || (0x00 != response)) {
        _card_type = CARD_UNKNOWN;
        return status;
    }

    if (SDCARD_V2 == _card_type) {
        // Get the card capacity CCS: CMD58
        if (SD_BLOCK_DEVICE_OK == (status = _cmd(CMD58_READ_OCR, 0x0, 0x0, &response))) {
            // High Capacity card
            if (response & OCR_HCS_CCS) {
                _card_type = SDCARD_V2HC;
            }
        }
    } else {
        _card_type = SDCARD_V1;
    }

    if (!_crc_on) {
        // Disable CRC
        status = _cmd(CMD59_CRC_ON_OFF, _crc_on);
    }
    status = _cmd(CMD59_CRC_ON_OFF, 0);

    return status;
}


int SDCard::init()
{
    int err;

    if (_is_initialized) {
        goto end;
    }

    err = _initialise_card();
    _is_initialized = (err == SD_BLOCK_DEVICE_OK);
    if (!_is_initialized) {
        return err;
    }
    _sectors = _sd_sectors();
    // CMD9 failed
    if (0 == _sectors) {
        return SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;
    }

    // Set block length to 512 (CMD16)
    if (_cmd(CMD16_SET_BLOCKLEN, _block_size) != 0) {
        return SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;
    }

    // Set SCK for data transfer TODO
    err = _freq();
    if (err) {
        return err;
    }

end:
    return SD_BLOCK_DEVICE_OK;
}

int SDCard::deinit()
{
    if (!_is_initialized) {
        goto end;
    }

    // Doesnt really do anything... as you can't exactly de-initialize the hardware..

    _is_initialized = false;
    _sectors = 0;

end:
    return SD_BLOCK_DEVICE_OK;
}


int SDCard::program(const void *b, uint64_t addr, uint64_t size)
{
    if (!is_valid_program(addr, size)) {
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    if (!_is_initialized) {
        return SD_BLOCK_DEVICE_ERROR_NO_INIT;
    }

    const uint8_t *buffer = static_cast<const uint8_t *>(b);
    int status = SD_BLOCK_DEVICE_OK;
    uint8_t response;

    // Get block count
    uint64_t blockCnt = size / _block_size;

    // SDSC Card (CCS=0) uses byte unit address
    // SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
    if (SDCARD_V2HC == _card_type) {
        addr = addr / _block_size;
    }

    // Send command to perform write operation
    if (blockCnt == 1) {
        // Single block write command
        if (SD_BLOCK_DEVICE_OK != (status = _cmd(CMD24_WRITE_BLOCK, addr))) {
            return status;
        }

        // Write data
        response = _write(buffer, SPI_START_BLOCK, _block_size);

        // Only CRC and general write error are communicated via response token
        if (response != SPI_DATA_ACCEPTED) {
            status = SD_BLOCK_DEVICE_ERROR_WRITE;
        }
    } else {
        // Pre-erase setting prior to multiple block write operation
        _cmd(ACMD23_SET_WR_BLK_ERASE_COUNT, blockCnt, 1);

        // Multiple block write command
        if (SD_BLOCK_DEVICE_OK != (status = _cmd(CMD25_WRITE_MULTIPLE_BLOCK, addr))) {
            return status;
        }

        // Write the data: one block at a time
        do {
            response = _write(buffer, SPI_START_BLK_MUL_WRITE, _block_size);
            if (response != SPI_DATA_ACCEPTED) {
                break;
            }
            buffer += _block_size;
        } while (--blockCnt);     // Receive all blocks of data

        /* In a Multiple Block write operation, the stop transmission will be done by
         * sending 'Stop Tran' token instead of 'Start Block' token at the beginning
         * of the next block
         */
        _cmd(CMD12_STOP_TRANSMISSION, 0x0); //_cmd(CMD12_STOP_TRANSMISSION, 0x0); //_spi->transfer(SPI_STOP_TRAN);
    }

    unselect();
    return status;
}

int SDCard::read(void *b, uint64_t addr, uint64_t size)
{
    if (!is_valid_read(addr, size)) {
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    if (!_is_initialized) {
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    uint8_t *buffer = static_cast<uint8_t *>(b);
    int status = SD_BLOCK_DEVICE_OK;
    uint64_t blockCnt =  size / _block_size;

    // SDSC Card (CCS=0) uses byte unit address
    // SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
    if (SDCARD_V2HC == _card_type) {
        addr = addr / _block_size;
    }

    // Write command ro receive data
    if (blockCnt > 1) {
        status = _cmd(CMD18_READ_MULTIPLE_BLOCK, addr);
    } else {
        status = _cmd(CMD17_READ_SINGLE_BLOCK, addr);
    }
    if (SD_BLOCK_DEVICE_OK != status) {
        return status;
    }

    // receive the data : one block at a time
    while (blockCnt) {
        if (0 != _read(buffer, _block_size)) {
            status = SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
            break;
        }
        buffer += _block_size;
        --blockCnt;
    }
    unselect();

    // Send CMD12(0x00000000) to stop the transmission for multi-block transfer
    if (size > _block_size) {
        status = _cmd(CMD12_STOP_TRANSMISSION, 0x0);
    }
    return status;
}

bool SDCard::_is_valid_trim(uint64_t addr, uint64_t size)
{
    return (
               addr % _erase_size == 0 &&
               size % _erase_size == 0 &&
               addr + size <= this->size());
}

int SDCard::trim(uint64_t addr, uint64_t size)
{
    if (!_is_valid_trim(addr, size)) {
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    if (!_is_initialized) {
        return SD_BLOCK_DEVICE_ERROR_NO_INIT;
    }
    int status = SD_BLOCK_DEVICE_OK;

    size -= _block_size;
    // SDSC Card (CCS=0) uses byte unit address
    // SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
    if (SDCARD_V2HC == _card_type) {
        size = size / _block_size;
        addr = addr / _block_size;
    }

    // Start lba sent in start command
    if (SD_BLOCK_DEVICE_OK != (status = _cmd(CMD32_ERASE_WR_BLK_START_ADDR, addr))) {
        //unlock();
        return status;
    }

    // End lba = addr+size sent in end addr command
    if (SD_BLOCK_DEVICE_OK != (status = _cmd(CMD33_ERASE_WR_BLK_END_ADDR, addr + size))) {
        return status;
    }
    status = _cmd(CMD38_ERASE, 0x0);
    return status;
}

uint64_t SDCard::get_read_size() const
{
    return _block_size;
}

uint64_t SDCard::get_program_size() const
{
    return _block_size;
}

uint64_t SDCard::size() const
{
    return _block_size * _sectors;
}

const char *SDCard::get_type() const
{
    return "SD";
}

int SDCard::frequency(uint64_t freq)
{
    _transfer_sck = freq;
    int err = _freq();
    return err;
}

// PRIVATE FUNCTIONS
int SDCard::_freq(void)
{
    // Max frequency supported is 25MHZ
    if (_transfer_sck <= 25000000) {
        _spi->initMaster(_transfer_sck);
        return 0;
    } else {
        _transfer_sck = 25000000;
        _spi->initMaster(_transfer_sck);
        return 1;
    }
}

uint8_t SDCard::_cmd_spi(SDCard::cmdSupported cmd, uint32_t arg)
{
    uint8_t response;
    char cmdPacket[PACKET_SIZE];

    // Prepare the command packet
    cmdPacket[0] = SPI_CMD(cmd);
    cmdPacket[1] = (arg >> 24);
    cmdPacket[2] = (arg >> 16);
    cmdPacket[3] = (arg >> 8);
    cmdPacket[4] = (arg >> 0);

    if (_crc_on) {
        cmdPacket[5] = (getCRC7(cmdPacket, 5) << 1) | 1;
    } else
    {
        // CMD0 is executed in SD mode, hence should have correct CRC
        // CMD8 CRC verification is always enabled
        switch (cmd) {
            case CMD0_GO_IDLE_STATE:
                cmdPacket[5] = 0x95;
                break;
            case CMD8_SEND_IF_COND:
                cmdPacket[5] = 0x87;
                break;
            default:
                cmdPacket[5] = 0xFF;    // Make sure bit 0-End bit is high
                break;
        }
    }

    // send a command
    for (int i = 0; i < PACKET_SIZE; i++) {
        _spi->transfer(cmdPacket[i]);
    }

    // The received byte immediataly following CMD12 is a stuff byte,
    // it should be discarded before receive the response of the CMD12.
    if (CMD12_STOP_TRANSMISSION == cmd) {
        _spi->transfer(FILLER);
    }

    // Loop for response: Response is sent back within command response time (NCR), 0 to 8 bytes for SDC
    for (int i = 0; i < 0x10; i++) {
        response = _spi->transfer(FILLER);
        // Got the response
        if (!(response & R1_RESPONSE_RECV)) {
            break;
        }
    }
    return response;
}

int SDCard::_cmd(SDCard::cmdSupported cmd, uint32_t arg, bool isAcmd, uint32_t *resp)
{
    int32_t status = SD_BLOCK_DEVICE_OK;
    uint32_t response;

    // Select card and wait for card to be ready before sending next command
    // Note: next command will fail if card is not ready
    select();

    // No need to wait for card to be ready when sending the stop command
    if (CMD12_STOP_TRANSMISSION != cmd) {
        _wait_ready(SD_COMMAND_TIMEOUT);
    }

    // Re-try command
    for (int i = 0; i < 3; i++) {
        // Send CMD55 for APP command first
        if (isAcmd) {
            response = _cmd_spi(CMD55_APP_CMD, 0x0);
            // Wait for card to be ready after CMD55
            if (false == _wait_ready(SD_COMMAND_TIMEOUT)) {
            }
        }

        // Send command over SPI interface
        response = _cmd_spi(cmd, arg);
        if (R1_NO_RESPONSE == response) {
            continue;
        }
        break;
    }

    // Pass the response to the command call if required
    if (0 != resp) {
        *resp = response;
    }

    // Process the response R1  : Exit on CRC/Illegal command error/No response
    if (R1_NO_RESPONSE == response) {
        unselect();
        return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;         // No device
    }
    if (response & R1_COM_CRC_ERROR) {
        unselect();
        return SD_BLOCK_DEVICE_ERROR_CRC;                // CRC error
    }
    if (response & R1_ILLEGAL_COMMAND) {
        unselect();
        if (CMD8_SEND_IF_COND == cmd) {                  // Illegal command is for Ver1 or not SD Card
            _card_type = CARD_UNKNOWN;
        }
        return SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;      // Command not supported
    }

    // Set status for other errors
    if ((response & R1_ERASE_RESET) || (response & R1_ERASE_SEQUENCE_ERROR)) {
        status = SD_BLOCK_DEVICE_ERROR_ERASE;            // Erase error
    } else if ((response & R1_ADDRESS_ERROR) || (response & R1_PARAMETER_ERROR)) {
        // Misaligned address / invalid address block length
        status = SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    // Get rest of the response part for other commands
    switch (cmd) {
        case CMD8_SEND_IF_COND:             // Response R7
            _card_type = SDCARD_V2; // fallthrough
        case CMD58_READ_OCR:                // Response R3
            response  = (_spi->transfer(FILLER) << 24);
            response |= (_spi->transfer(FILLER) << 16);
            response |= (_spi->transfer(FILLER) << 8);
            response |= _spi->transfer(FILLER);
            break;

        case CMD12_STOP_TRANSMISSION:       // Response R1b
        case CMD38_ERASE:
            _wait_ready(SD_COMMAND_TIMEOUT);
            break;

        case ACMD13_SD_STATUS:             // Response R2
            response = _spi->transfer(FILLER);
            break;

        default:                            // Response R1
            break;
    }

    // Pass the updated response to the command
    if (0 != resp) {
        *resp = response;
    }

    // Do not deselect card if read is in progress.
    if (((CMD9_SEND_CSD == cmd) || (ACMD22_SEND_NUM_WR_BLOCKS == cmd) ||
            (CMD24_WRITE_BLOCK == cmd) || (CMD25_WRITE_MULTIPLE_BLOCK == cmd) ||
            (CMD17_READ_SINGLE_BLOCK == cmd) || (CMD18_READ_MULTIPLE_BLOCK == cmd))
            && (SD_BLOCK_DEVICE_OK == status)) {
        return SD_BLOCK_DEVICE_OK;
    }
    // Deselect card
    unselect();
    return status;
}

int SDCard::_cmd8()
{
    uint32_t arg = (CMD8_PATTERN << 0);         // [7:0]check pattern
    uint32_t response = 0;
    int32_t status = SD_BLOCK_DEVICE_OK;

    arg |= (0x1 << 8);  // 2.7-3.6V             // [11:8]supply voltage(VHS)

    status = _cmd(CMD8_SEND_IF_COND, arg, 0x0, &response);
    // Verify voltage and pattern for V2 version of card
    if ((SD_BLOCK_DEVICE_OK == status) && (SDCARD_V2 == _card_type)) {
        // If check pattern is not matched, CMD8 communication is not valid
        if ((response & 0xFFF) != arg) {
            _card_type = CARD_UNKNOWN;
            status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
        }
    }
    return status;
}

uint32_t SDCard::_go_idle_state()
{
    uint32_t response;

    /* Reseting the MCU SPI master may not reset the on-board SDCard, in which
     * case when MCU power-on occurs the SDCard will resume operations as
     * though there was no reset. In this scenario the first CMD0 will
     * not be interpreted as a command and get lost. For some cards retrying
     * the command overcomes this situation. */
    for (int i = 0; i < SD_CMD0_GO_IDLE_STATE_RETRIES; i++) {
        _cmd(CMD0_GO_IDLE_STATE, 0x0, 0x0, &response);
        if (R1_IDLE_STATE == response) {
            break;
        }
    }
    return response;
}

int SDCard::_read_bytes(uint8_t *buffer, uint32_t length)
{
    uint16_t crc;

    // read until start byte (0xFE)
    if (false == _wait_token(SPI_START_BLOCK)) {
        unselect();
        return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
    }

    // read data
    for (uint32_t i = 0; i < length; i++) {
        buffer[i] = _spi->transfer(FILLER);
    }

    // Read the CRC16 checksum for the data block
    crc = (_spi->transfer(FILLER) << 8);
    crc |= _spi->transfer(FILLER);

    if (_crc_on) {
        //todo
        uint32_t crc_result;
        if (crc_result != crc) {
            unselect();
            return SD_BLOCK_DEVICE_ERROR_CRC;
        }
    }

    unselect();
    return 0;
}

int SDCard::_read(uint8_t *buffer, uint32_t length)
{
    uint16_t crc;

    // read until start byte (0xFE)
    if (false == _wait_token(SPI_START_BLOCK)) {
        return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
    }

    // read data
    for(int p = 0; p < length; p++){
        ((char *)buffer)[p] = _spi->transfer(FILLER);
    }

    // Read the CRC16 checksum for the data block
    crc = (_spi->transfer(FILLER) << 8);
    crc |= _spi->transfer(FILLER);

    if (_crc_on) {
        //todo
        uint32_t crc_result;
        // Compute and verify checksum
        if ((uint16_t)crc_result != crc) {
            return SD_BLOCK_DEVICE_ERROR_CRC;
        }
    }

    return 0;
}

uint8_t SDCard::_write(const uint8_t *buffer, uint8_t token, uint32_t length)
{

    uint32_t crc = (~0);
    uint8_t response = 0xFF;

    // indicate start of block
    _spi->transfer(token);

    // write the data
    for(int p = 0; p < length; p++){
        _spi->transfer(((char *)buffer)[p]);
    }

    if (_crc_on) {
        //CRC16CCITT crc16(0, false);
        // Compute CRC
        //todo
    }

    // write the checksum CRC16
    _spi->transfer(crc >> 8);
    _spi->transfer(crc);


    // check the response token
    response = _spi->transfer(FILLER);

    // Wait for last block to be written
    _wait_ready(SD_COMMAND_TIMEOUT);

    return (response & SPI_DATA_RESPONSE_MASK);
}

static uint32_t ext_bits(unsigned char *data, int msb, int lsb)
{
    uint32_t bits = 0;
    uint32_t size = 1 + msb - lsb;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t position = lsb + i;
        uint32_t byte = 15 - (position >> 3);
        uint32_t bit = position & 0x7;
        uint32_t value = (data[byte] >> bit) & 1;
        bits |= value << i;
    }
    return bits;
}

uint64_t SDCard::_sd_sectors()
{
    uint32_t c_size, c_size_mult, read_bl_len;
    uint32_t block_len, mult, blocknr;
    uint32_t hc_c_size;
    uint64_t blocks = 0, capacity = 0;

    // CMD9, Response R2 (R1 byte + 16-byte block read)
    if (_cmd(CMD9_SEND_CSD, 0x0) != 0x0) {
        return 0;
    }
    uint8_t csd[16];
    if (_read_bytes(csd, 16) != 0) {
        return 0;
    }

    // csd_structure : csd[127:126]
    int csd_structure = ext_bits(csd, 127, 126);
    switch (csd_structure) {
        case 0:
            c_size = ext_bits(csd, 73, 62);              // c_size        : csd[73:62]
            c_size_mult = ext_bits(csd, 49, 47);         // c_size_mult   : csd[49:47]
            read_bl_len = ext_bits(csd, 83, 80);         // read_bl_len   : csd[83:80] - the *maximum* read block length
            block_len = 1 << read_bl_len;                // BLOCK_LEN = 2^READ_BL_LEN
            mult = 1 << (c_size_mult + 2);               // MULT = 2^C_SIZE_MULT+2 (C_SIZE_MULT < 8)
            blocknr = (c_size + 1) * mult;               // BLOCKNR = (C_SIZE+1) * MULT
            capacity = (uint64_t) blocknr * block_len;  // memory capacity = BLOCKNR * BLOCK_LEN
            blocks = capacity / _block_size;

            // ERASE_BLK_EN = 1: Erase in multiple of 512 bytes supported
            if (ext_bits(csd, 46, 46)) {
                _erase_size = BLOCK_SIZE_HC;
            } else {
                // ERASE_BLK_EN = 1: Erase in multiple of SECTOR_SIZE supported
                _erase_size = BLOCK_SIZE_HC * (ext_bits(csd, 45, 39) + 1);
            }
            break;

        case 1: // this one should fire as an SDXC is used (CSD v2)
            hc_c_size = ext_bits(csd, 69, 48);            // device size : C_SIZE : [69:48]
            blocks = (hc_c_size + 1) << 10;               // block count = C_SIZE+1) * 1K byte (512B is block size)
            _erase_size = BLOCK_SIZE_HC;
            break;

        default:
            return 0;
    };
    return blocks;
}

// SPI function to wait till chip is ready and sends start token
bool SDCard::_wait_token(uint8_t token)
{
    //todo timeout
    int count = 0;
    do {
        if (token == _spi->transfer(FILLER)) {
            //_spi_timer.stop();
            return true;
        };
        count++;
    } while ( count < 50000);
    return false;
}

// SPI function to wait till chip is ready
// The host controller should wait for end of the process until DO goes high (a 0xFF is received).
bool SDCard::_wait_ready(int timeous_ms)//std::chrono::duration<uint32_t, std::milli> timeout)
{
    uint8_t response;
    int c = 0;
    do {
        response = _spi->transfer(FILLER);
        if (response == 0xFF) {
            return true;
        }
    } while ( c < timeous_ms);
    return false;
}

// SPI function to wait for count
void SDCard::_spi_wait(uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i) {
        _spi->transfer(FILLER);
    }
}

void SDCard::_spi_init()
{
    _spi->initMaster(_init_sck);
    sendDummy();
}

void SDCard::sendDummy(){
    for(int k = 0; k < 20;  k++)
    {
    //        while (!(MAP_SPI_getInterruptStatus(EUSCI_A1_SPI_BASE, EUSCI_A_SPI_TRANSMIT_INTERRUPT )));
    //        MAP_SPI_transmitData(EUSCI_A1_SPI_BASE, 0xff);//Write FF for HIGH DI (10 times = 80 cycles)
    //        while (!(MAP_SPI_getInterruptStatus(EUSCI_A1_SPI_BASE, EUSCI_A_SPI_RECEIVE_INTERRUPT )));
        _spi->transfer(FILLER);
    }
}
void SDCard::select(){
    _spi->transfer(FILLER);
    _spi->transfer(FILLER);
    MAP_GPIO_setOutputLowOnPin( CS_PORT, CS_PIN );
}

void SDCard::unselect(){
    _spi->transfer(FILLER);
    MAP_GPIO_setOutputHighOnPin( CS_PORT, CS_PIN );
}


// older deprecated:
uint8_t SDCard::sendCmd(uint8_t cmdNumber, uint32_t payload){
    waitForReady();

    char CMD[6] = {0x40 + cmdNumber, (uint8_t)(payload >> 24), (uint8_t)(payload >> 16), (uint8_t)(payload >> 8), (uint8_t)(payload), (uint8_t) 0};
    CMD[5] = (getCRC7(CMD, 5) << 1) | 1;
    //Console::log(" #CMD: %x %x %x %x %x %x", CMD[0],CMD[1],CMD[2],CMD[3],CMD[4],CMD[5]);

    for(int j = 0; j < 6; j++){
        _spi->transfer(CMD[j]);
    }

    uint8_t R1;
    do {
        R1 = _spi->transfer(FILLER);
    } while ((R1 & 0x80) != 0);

    return R1;
}

void SDCard::waitForReady(){
    uint8_t reply;
    do {
        reply = _spi->transfer(FILLER);
    } while ((reply) != 0xff);
}

void SDCard::getArray(uint8_t Buff[], int size){
    for(int k = 0; k < size; k++){
        Buff[k] = _spi->transfer(0xff);
    }
}
