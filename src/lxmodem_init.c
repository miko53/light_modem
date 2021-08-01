
#include "lxmodem.h"
#include "lxmodem_priv.h"
#include "lmodem_buffer.h"
#include <string.h>

#define LXMODEM_CRC16_CCITT_POLYNOME   (0x1021)

void lmodem_init(modem_context_t* pThis, lxmodem_opts opts)
{
    memset(pThis, 0, sizeof(modem_context_t));
    pThis->opts = opts;
    if ((opts == lxmodem_128_with_crc) || (opts == lxmodem_1k))
    {
        crc16_init(&pThis->crc16, LXMODEM_CRC16_CCITT_POLYNOME);
    }
}


void lmodem_set_putchar_cb(modem_context_t* pThis, void (*putchar)(modem_context_t* pThis, uint8_t* data, uint32_t size))
{
    pThis->putchar = putchar;
}

void lmodem_set_getchar_cb(modem_context_t* pThis, bool (*getchar)(modem_context_t* pThis, uint8_t* data, uint32_t size))
{
    pThis->getchar = getchar;
}

bool lmodem_set_line_buffer(modem_context_t* pThis, uint8_t* buffer, uint32_t size)
{
    bool bOk;
    uint32_t expectedSize;

    bOk = false;
    expectedSize = 0;

    switch (pThis->opts)
    {
        case lxmodem_128_with_chksum:
            // blk data  =  no Bko (2 bytes) + data (128) + chksum 1(byte)
            expectedSize = LXMODEM_HEADER_SIZE + LXMODEM_BLOCK_SIZE_128 + LXMODEM_CHKSUM_SIZE;
            pThis->withCrc = false;
            break;
        case lxmodem_128_with_crc:
            // blk data  =  no Bko (2 bytes) + data (128) + crc  2(bytes)
            expectedSize = LXMODEM_HEADER_SIZE + LXMODEM_BLOCK_SIZE_128 + LXMODEM_CRC16_SIZE;
            pThis->withCrc = true;
            break;
        case lxmodem_1k:
            // blk data = no Blo (2 bytes) + data (1024) + crc (2bytes)
            expectedSize = LXMODEM_HEADER_SIZE + LXMODEM_BLOCK_SIZE_1024 + LXMODEM_CRC16_SIZE;
            pThis->withCrc = true;
            break;
    }

    if (size >= expectedSize)
    {
        pThis->blk_buffer.buffer = buffer;
        pThis->blk_buffer.max_size = size;
        pThis->blk_buffer.current_size = 0;
        bOk = true;
    }

    return bOk;
}

void lmodem_set_file_buffer(modem_context_t* pThis, uint8_t* buffer, uint32_t size)
{
    lmodem_buffer_init(&pThis->ramfile, buffer, size);
}
