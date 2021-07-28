#include <string.h>
#include "lxmodem.h"

void lmodem_init(modem_context_t* pThis, lxmodem_opts opts)
{
    memset(pThis, 0, sizeof(modem_context_t));
    pThis->opts = opts;
}

void lmodem_set_putchar_cb(modem_context_t* pThis, void (*putchar)(modem_context_t* pThis, uint8_t* data, uint32_t size))
{
    pThis->putchar = putchar;
}

void lmodem_set_getchar_cb(modem_context_t* pThis, bool (*getchar)(modem_context_t* pThis, uint8_t* data, uint32_t size))
{
    pThis->getchar = getchar;
}


bool lmodem_set_buffer(modem_context_t* pThis, uint8_t* buffer, uint32_t size)
{
    bool bOk;
    uint32_t expectedSize;

    bOk = false;
    expectedSize = 0;

    switch (pThis->opts)
    {
        case lxmodem_128_with_chksum:
            expectedSize = 132; // blk data  =  SOH + no Bko (2 bytes) + data (128) + chksum 1(byte)
            break;
        case lxmodem_128_with_crc:
            expectedSize = 133; // blk data  =  SOH + no Bko (2 bytes) + data (128) + crc  2(bytes)
            break;
        case lxmodem_1k:
            expectedSize = 1029; // blk data = STX + no Blo (2 bytes) + data (1024) + crc (2bytes)
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
    pThis->ramfile.buffer = buffer;
    pThis->ramfile.max_size = size;
    pThis->ramfile.current_size = 0;
}

static void lxmodem_build_and_send_preambule(modem_context_t* pThis);

uint32_t lxmodem_receive(modem_context_t* pThis)
{
    uint32_t bytes_received;
    bool bReceived;
    bytes_received = 0;

    //send that we are ready
    lxmodem_build_and_send_preambule(pThis);

    bReceived = pThis->getchar(pThis, pThis->blk_buffer.buffer, 1);



    return bytes_received;
}


void lxmodem_build_and_send_preambule(modem_context_t* pThis)
{
    char p;

    switch (pThis->opts)
    {
        case lxmodem_128_with_chksum:
            p = NAK;
            break;

        case lxmodem_128_with_crc:
            p = 'C';
            break;

        case lxmodem_1k:
            break;

        default:
            p = NAK;
            break;
    }

    pThis->putchar(pThis, (uint8_t*) &p, 1);
}
