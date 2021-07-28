#include <string.h>
#include "lxmodem.h"

#define MODEM_TRACE
#ifdef MODEM_TRACE
#include <stdio.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)

#else
#define DBG(...)
#endif /* MODEM_DBG */

static uint8_t lxmodem_calcul_chksum(uint8_t* buffer, uint32_t size);
static void lxmodem_build_and_send_preambule(modem_context_t* pThis);

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

static inline void lmodem_putchar(modem_context_t* pThis, uint8_t* data, uint32_t size)
{
#ifdef MODEM_TRACE
    int32_t i;
    DBG("send %d byte(s): ", size);
    for (i = 0; i < (int32_t) (size - 1); i++)
    {
        DBG("0x%.2x-", data[i] & 0xFF);
    }
    DBG("0x%.2x\n", data[i] & 0xFF);
#endif /* MODEM_TRACE */
    pThis->putchar(pThis, data, size);
}

static inline bool lmodem_getchar(modem_context_t* pThis, uint8_t* data, uint32_t size)
{
    bool b;
    b = pThis->getchar(pThis, data, size);
#ifdef MODEM_TRACE
    if (b)
    {
        int32_t i;
        DBG("get %d byte(s): ", size);
        for (i = 0; i < (int32_t) (size - 1); i++)
        {
            DBG("0x%.2x-", data[i] & 0xFF);
        }
        DBG("0x%.2x\n", data[i] & 0xFF);
    }
    else
    {
        DBG("get 0 byte\n");
    }
#endif /* MODEM_TRACE */
    return b;
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
            pThis->blksize = 128;
            pThis->withCrc = false;
            break;
        case lxmodem_128_with_crc:
            expectedSize = 133; // blk data  =  SOH + no Bko (2 bytes) + data (128) + crc  2(bytes)
            pThis->blksize = 128;
            pThis->withCrc = true;
            break;
        case lxmodem_1k:
            expectedSize = 1029; // blk data = STX + no Blo (2 bytes) + data (1024) + crc (2bytes)
            pThis->blksize = 1024;
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
    pThis->ramfile.buffer = buffer;
    pThis->ramfile.max_size = size;
    pThis->ramfile.current_size = 0;
}

uint32_t lxmodem_receive(modem_context_t* pThis)
{
    uint32_t bytes_received;
    bool bFinished;
    bool bReceived;
    bool bSendAck;
    uint8_t header;
    uint32_t expectedBlksize;
    uint8_t expectedBlkNumber;

    bytes_received = 0;
    expectedBlkNumber = 1;
    if (pThis->withCrc == true)
    {
        expectedBlksize = pThis->blksize + 4;
    }
    else
    {
        expectedBlksize = pThis->blksize + 3;
    }

    //send that we are ready
    lxmodem_build_and_send_preambule(pThis);

    bFinished = false;
    while (!bFinished)
    {
        bSendAck = false;
        //receive one block
        bReceived = lmodem_getchar(pThis, &header, 1);
        if (bReceived)
        {
            switch (header)
            {
                case SOH:
                    //read 128 blzsize
                    bReceived = lmodem_getchar(pThis, pThis->blk_buffer.buffer, expectedBlksize);
                    if (bReceived)
                    {
                        uint8_t complement =  ~expectedBlkNumber;
                        if ((pThis->blk_buffer.buffer[0] == expectedBlkNumber) &&
                            (pThis->blk_buffer.buffer[1] == complement))
                        {
                            //good blocknumber
                            if (pThis->withCrc == true)
                            {

                            }
                            else
                            {
                                uint8_t chksum;
                                chksum = lxmodem_calcul_chksum(pThis->blk_buffer.buffer + 2, pThis->blksize);
                                if (chksum == pThis->blk_buffer.buffer[pThis->blksize + 2])
                                {
                                    DBG("good chksum\n");
                                    bSendAck = true;
                                    bytes_received += 128;
                                }
                            }
                        }
                        else
                        {
                            DBG("wrong blknumber %d received, expected %d\n", pThis->blk_buffer.buffer[0], expectedBlkNumber);
                        }
                    }
                    break;

                case EOT:
                    //end of transfert
                    bSendAck = true;
                    bFinished = true;
                    break;

                case CAN:
                    //TODO check two times before
                    bFinished = true;
                    bytes_received = -1;
                    break;

                default:
                    break;
            }

            uint8_t ack;
            if (bSendAck)
            {
                ack = ACK;
                expectedBlkNumber++;
            }
            else
            {
                ack = NAK;
            }
            lmodem_putchar(pThis, &ack, 1);
        }

    }

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
            //TODO
            break;

        default:
            p = NAK;
            break;
    }

    lmodem_putchar(pThis, (uint8_t*) &p, 1);
}

static uint8_t lxmodem_calcul_chksum(uint8_t* buffer, uint32_t size)
{
    uint8_t t;
    t = 0;
    for (uint32_t i = 0; i < size; i++)
    {
        t += buffer[i];
    }

    return t;
}
