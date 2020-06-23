/*
 * SDCard.h
 *
 *  Created on: 10 Jun 2020
 *
 *      Based on https://github.com/ARMmbed/mbed-os/blob/master/components/storage/blockdevice/COMPONENT_SD/SDBlockDevice.h
 */

#ifndef SDCARD_H_
#define SDCARD_H_

#include <stdint.h>
#include <driverlib.h>
#include "DSPI_A.h"
//#include "Console.h"

#define SD_INIT_FREQUENCY 200000
#define SD_TRX_FREQUENCY  20000000
#define SD_CRC_ENABLED    0


#define BLOCK_SIZE_HC                            512    /*!< Block size supported for SD card is 512 bytes  */

// Types
#define SDCARD_NONE              0           /**< No card is present */
#define SDCARD_V1                1           /**< v1.x Standard Capacity */
#define SDCARD_V2                2           /**< v2.x Standard capacity SD card */
#define SDCARD_V2HC              3           /**< v2.x High capacity SD card */
#define CARD_UNKNOWN             4           /**< Unknown or unsupported card */

#define SD_BLOCK_DEVICE_OK                       0      /*!< no error */
#define SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK        -5001  /*!< operation would block */
#define SD_BLOCK_DEVICE_ERROR_UNSUPPORTED        -5002  /*!< unsupported operation */
#define SD_BLOCK_DEVICE_ERROR_PARAMETER          -5003  /*!< invalid parameter */
#define SD_BLOCK_DEVICE_ERROR_NO_INIT            -5004  /*!< uninitialized */
#define SD_BLOCK_DEVICE_ERROR_NO_DEVICE          -5005  /*!< device is missing or not connected */
#define SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED    -5006  /*!< write protected */
#define SD_BLOCK_DEVICE_ERROR_UNUSABLE           -5007  /*!< unusable card */
#define SD_BLOCK_DEVICE_ERROR_NO_RESPONSE        -5008  /*!< No response from device */
#define SD_BLOCK_DEVICE_ERROR_CRC                -5009  /*!< CRC error */
#define SD_BLOCK_DEVICE_ERROR_ERASE              -5010  /*!< Erase error: reset/sequence */
#define SD_BLOCK_DEVICE_ERROR_WRITE              -5011  /*!< SPI Write error: !SPI_DATA_ACCEPTED */

/* SIZE in Bytes */
#define PACKET_SIZE              6           /*!< SD Packet size CMD+ARG+CRC */
#define R1_RESPONSE_SIZE         1           /*!< Size of R1 response */
#define R2_RESPONSE_SIZE         2           /*!< Size of R2 response */
#define R3_R7_RESPONSE_SIZE      5           /*!< Size of R3/R7 response */

/* R1 Response Format */
#define R1_NO_RESPONSE          (0xFF)
#define R1_RESPONSE_RECV        (0x80)
#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)

/* R1b Response */
#define DEVICE_BUSY             (0x00)

/* R2 Response Format */
#define R2_CARD_LOCKED          (1 << 0)
#define R2_CMD_FAILED           (1 << 1)
#define R2_ERROR                (1 << 2)
#define R2_CC_ERROR             (1 << 3)
#define R2_CC_FAILED            (1 << 4)
#define R2_WP_VIOLATION         (1 << 5)
#define R2_ERASE_PARAM          (1 << 6)
#define R2_OUT_OF_RANGE         (1 << 7)

/* R3 Response : OCR Register */
#define OCR_HCS_CCS             (0x1 << 30)
#define OCR_LOW_VOLTAGE         (0x01 << 24)
#define OCR_3_3V                (0x1 << 20)

/* R7 response pattern for CMD8 */
#define CMD8_PATTERN             (0xAA)

/* Control Tokens   */
#define SPI_DATA_RESPONSE_MASK   (0x1F)
#define SPI_DATA_ACCEPTED        (0x05)
#define SPI_DATA_CRC_ERROR       (0x0B)
#define SPI_DATA_WRITE_ERROR     (0x0D)
#define SPI_START_BLOCK          (0xFE)      /*!< For Single Block Read/Write and Multiple Block Read */
#define SPI_START_BLK_MUL_WRITE  (0xFC)      /*!< Start Multi-block write */
#define SPI_STOP_TRAN            (0xFD)      /*!< Stop Multi-block write */

#define SPI_DATA_READ_ERROR_MASK (0xF)       /*!< Data Error Token: 4 LSB bits */
#define SPI_READ_ERROR           (0x1 << 0)  /*!< Error */
#define SPI_READ_ERROR_CC        (0x1 << 1)  /*!< CC Error*/
#define SPI_READ_ERROR_ECC_C     (0x1 << 2)  /*!< Card ECC failed */
#define SPI_READ_ERROR_OFR       (0x1 << 3)  /*!< Out of Range */

class SDCard
{
private:
    DSPI_A* _spi;

    char CRC7Table[256] = {
        0x00, 0x09, 0x12, 0x1B, 0x24, 0x2D, 0x36, 0x3F,
        0x48, 0x41, 0x5A, 0x53, 0x6C, 0x65, 0x7E, 0x77,
        0x19, 0x10, 0x0B, 0x02, 0x3D, 0x34, 0x2F, 0x26,
        0x51, 0x58, 0x43, 0x4A, 0x75, 0x7C, 0x67, 0x6E,
        0x32, 0x3B, 0x20, 0x29, 0x16, 0x1F, 0x04, 0x0D,
        0x7A, 0x73, 0x68, 0x61, 0x5E, 0x57, 0x4C, 0x45,
        0x2B, 0x22, 0x39, 0x30, 0x0F, 0x06, 0x1D, 0x14,
        0x63, 0x6A, 0x71, 0x78, 0x47, 0x4E, 0x55, 0x5C,
        0x64, 0x6D, 0x76, 0x7F, 0x40, 0x49, 0x52, 0x5B,
        0x2C, 0x25, 0x3E, 0x37, 0x08, 0x01, 0x1A, 0x13,
        0x7D, 0x74, 0x6F, 0x66, 0x59, 0x50, 0x4B, 0x42,
        0x35, 0x3C, 0x27, 0x2E, 0x11, 0x18, 0x03, 0x0A,
        0x56, 0x5F, 0x44, 0x4D, 0x72, 0x7B, 0x60, 0x69,
        0x1E, 0x17, 0x0C, 0x05, 0x3A, 0x33, 0x28, 0x21,
        0x4F, 0x46, 0x5D, 0x54, 0x6B, 0x62, 0x79, 0x70,
        0x07, 0x0E, 0x15, 0x1C, 0x23, 0x2A, 0x31, 0x38,
        0x41, 0x48, 0x53, 0x5A, 0x65, 0x6C, 0x77, 0x7E,
        0x09, 0x00, 0x1B, 0x12, 0x2D, 0x24, 0x3F, 0x36,
        0x58, 0x51, 0x4A, 0x43, 0x7C, 0x75, 0x6E, 0x67,
        0x10, 0x19, 0x02, 0x0B, 0x34, 0x3D, 0x26, 0x2F,
        0x73, 0x7A, 0x61, 0x68, 0x57, 0x5E, 0x45, 0x4C,
        0x3B, 0x32, 0x29, 0x20, 0x1F, 0x16, 0x0D, 0x04,
        0x6A, 0x63, 0x78, 0x71, 0x4E, 0x47, 0x5C, 0x55,
        0x22, 0x2B, 0x30, 0x39, 0x06, 0x0F, 0x14, 0x1D,
        0x25, 0x2C, 0x37, 0x3E, 0x01, 0x08, 0x13, 0x1A,
        0x6D, 0x64, 0x7F, 0x76, 0x49, 0x40, 0x5B, 0x52,
        0x3C, 0x35, 0x2E, 0x27, 0x18, 0x11, 0x0A, 0x03,
        0x74, 0x7D, 0x66, 0x6F, 0x50, 0x59, 0x42, 0x4B,
        0x17, 0x1E, 0x05, 0x0C, 0x33, 0x3A, 0x21, 0x28,
        0x5F, 0x56, 0x4D, 0x44, 0x7B, 0x72, 0x69, 0x60,
        0x0E, 0x07, 0x1C, 0x15, 0x2A, 0x23, 0x38, 0x31,
        0x46, 0x4F, 0x54, 0x5D, 0x62, 0x6B, 0x70, 0x79
    };

    char appendCRC7(char CRC, char message_byte)
    {
        return CRC7Table[(CRC << 1) ^ message_byte];
    }

    char getCRC7(char message[], int length)
    {
      int i;
      char CRC = 0;

      for (i = 0; i < length; ++i)
        CRC = appendCRC7(CRC, message[i]);

      return CRC;
    }

    uint32_t CS_PIN;
    uint32_t CS_PORT;

    enum cmdSupported {
        CMD_NOT_SUPPORTED = -1,             /**< Command not supported error */
        CMD0_GO_IDLE_STATE = 0,             /**< Resets the SD Memory Card */
        CMD1_SEND_OP_COND = 1,              /**< Sends host capacity support */
        CMD6_SWITCH_FUNC = 6,               /**< Check and Switches card function */
        CMD8_SEND_IF_COND = 8,              /**< Supply voltage info */
        CMD9_SEND_CSD = 9,                  /**< Provides Card Specific data */
        CMD10_SEND_CID = 10,                /**< Provides Card Identification */
        CMD12_STOP_TRANSMISSION = 12,       /**< Forces the card to stop transmission */
        CMD13_SEND_STATUS = 13,             /**< Card responds with status */
        CMD16_SET_BLOCKLEN = 16,            /**< Length for SC card is set */
        CMD17_READ_SINGLE_BLOCK = 17,       /**< Read single block of data */
        CMD18_READ_MULTIPLE_BLOCK = 18,     /**< Card transfers data blocks to host until interrupted
                                                 by a STOP_TRANSMISSION command */
        CMD24_WRITE_BLOCK = 24,             /**< Write single block of data */
        CMD25_WRITE_MULTIPLE_BLOCK = 25,    /**< Continuously writes blocks of data until
                                                 'Stop Tran' token is sent */
        CMD27_PROGRAM_CSD = 27,             /**< Programming bits of CSD */
        CMD32_ERASE_WR_BLK_START_ADDR = 32, /**< Sets the address of the first write
                                                 block to be erased. */
        CMD33_ERASE_WR_BLK_END_ADDR = 33,   /**< Sets the address of the last write
                                                 block of the continuous range to be erased.*/
        CMD38_ERASE = 38,                   /**< Erases all previously selected write blocks */
        CMD55_APP_CMD = 55,                 /**< Extend to Applications specific commands */
        CMD56_GEN_CMD = 56,                 /**< General Purpose Command */
        CMD58_READ_OCR = 58,                /**< Read OCR register of card */
        CMD59_CRC_ON_OFF = 59,              /**< Turns the CRC option on or off*/
        // App Commands
        ACMD6_SET_BUS_WIDTH = 6,
        ACMD13_SD_STATUS = 13,
        ACMD22_SEND_NUM_WR_BLOCKS = 22,
        ACMD23_SET_WR_BLK_ERASE_COUNT = 23,
        ACMD41_SD_SEND_OP_COND = 41,
        ACMD42_SET_CLR_CARD_DETECT = 42,
        ACMD51_SEND_SCR = 51,
    };

    uint8_t _card_type;
    int _cmd(SDCard::cmdSupported cmd, uint32_t arg, bool isAcmd = 0, uint32_t *resp = 0);


    int _cmd8();
    uint32_t _go_idle_state();
    int _initialise_card();

    uint64_t _sectors;
    uint64_t _sd_sectors();

    bool _is_valid_trim(uint64_t addr, uint64_t size);

    uint32_t _init_sck;             /**< Initial SPI frequency */
    uint32_t _transfer_sck;         /**< SPI frequency during data transfer/after initialization */

    /* SPI initialization function */
    void _spi_init();
    uint8_t _cmd_spi(SDCard::cmdSupported cmd, uint32_t arg);
    void _spi_wait(uint8_t count);

    bool _wait_token(uint8_t token);        /**< Wait for token */
    bool _wait_ready(int timeous_ms = 300);    /**< 300ms default wait for card to be ready */
    int _read(uint8_t *buffer, uint32_t length);
    int _read_bytes(uint8_t *buffer, uint32_t length);
    uint8_t _write(const uint8_t *buffer, uint8_t token, uint32_t length);
    int _freq(void);

    static const uint32_t _block_size;
    uint32_t _erase_size;
    bool _is_initialized;
    bool _crc_on = 0;  //please leave off for now
    uint32_t _init_ref_count;

public:
    SDCard(DSPI_A* DSPI_in, uint32_t CS_port, uint32_t CS_pin);
    ~SDCard();

    bool is_valid_read(uint64_t addr, uint64_t size) const
        {
            return (
                       addr % get_read_size() == 0 &&
                       size % get_read_size() == 0 &&
                       addr + size <= this->size());
        }

    bool is_valid_program(uint64_t addr, uint64_t size) const
    {
        return (
                   addr % get_program_size() == 0 &&
                   size % get_program_size() == 0 &&
                   addr + size <= this->size());
    }

    int init();
    int deinit();
    int read(void *buffer, uint64_t addr, uint64_t size);
    int program(const void *buffer, uint64_t addr, uint64_t size);
    int trim(uint64_t addr, uint64_t size);
    uint64_t get_read_size() const;
    uint64_t get_program_size() const;
    uint64_t size() const;
    int frequency(uint64_t freq);
    const char *get_type() const;

    void select();
    void unselect();

    uint8_t sendCmd(uint8_t cmdNumber, uint32_t payload);
    uint8_t transmitCmd(uint8_t cmdNumber, uint32_t payload);
    void waitForReady();
    void getArray(uint8_t Buff[], int size);
    void sendDummy();
    int sync()
        {
            return 0;
        }
    int erase(uint64_t addr, uint64_t size)
        {
            return 0;
        }
    uint64_t get_erase_size() const
        {
            return get_program_size();
        }

};


#endif /* SDCARD_H_ */
