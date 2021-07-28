#include <string.h>
#include "lxmodem.h"
#include <assert.h>

#define LXMODEM_CRC16_CCITT_POLYNOME   (0x1021)
#define LXMODEM_HEADER_SIZE            (2)
#define LXMODEM_CHKSUM_SIZE            (1)
#define LXMODEM_CRC16_SIZE             (2)

#define MODEM_TRACE
#ifdef MODEM_TRACE
#include <stdio.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)

#else
#define DBG(...)
#endif /* MODEM_DBG */

static uint8_t lxmodem_calcul_chksum(uint8_t* buffer, uint32_t size);
static void lxmodem_build_and_send_preambule(modem_context_t* pThis);
static bool lxmodem_receive_block(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t expectedBlksize);
static bool lxmodem_check_block_no_and_crc(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t requestedBlksize);
static bool lxmodem_check_crc(modem_context_t* pThis, uint32_t requestedBlksize);

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
            expectedSize = LXMODEM_HEADER_SIZE + 128 + LXMODEM_CHKSUM_SIZE; // blk data  =  no Bko (2 bytes) + data (128) + chksum 1(byte)
            pThis->withCrc = false;
            break;
        case lxmodem_128_with_crc:
            expectedSize = LXMODEM_HEADER_SIZE + 128 + LXMODEM_CRC16_SIZE; // blk data  =  no Bko (2 bytes) + data (128) + crc  2(bytes)
            pThis->withCrc = true;
            break;
        case lxmodem_1k:
            expectedSize = LXMODEM_HEADER_SIZE + 1024 + LXMODEM_CRC16_SIZE; // blk data = no Blo (2 bytes) + data (1024) + crc (2bytes)
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
    uint8_t expectedBlkNumber;
    uint32_t blksize;

    bytes_received = 0;
    expectedBlkNumber = 1;

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
                    blksize = 128;
                    bSendAck = lxmodem_receive_block(pThis, expectedBlkNumber, blksize);
                    break;

                case STX:
                    //read 1k blzsize
                    DBG("request 1k\n");
                    if (pThis->opts == lxmodem_1k)
                    {
                        blksize = 1024;
                        bSendAck = lxmodem_receive_block(pThis, expectedBlkNumber, blksize);
                    }
                    else
                    {
                        ; // not compatible
                        /* TODO seems not usefull, emiter does not fall back to 128bytes blksize */
                        /*for(uint32_t i = 0; i < 8; i++)
                          lmodem_getchar(pThis, pThis->blk_buffer.buffer, 128);
                        lmodem_getchar(pThis, pThis->blk_buffer.buffer, 4);*/
                    }
                    break;

                case EOT:
                    //end of transfert
                    bSendAck = true;
                    blksize = 0;
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
                bytes_received += blksize;
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
        case lxmodem_1k:
            p = 'C';
            break;

        default:
            p = NAK;
            break;
    }

    lmodem_putchar(pThis, (uint8_t*) &p, 1);
}

static bool lxmodem_receive_block(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t requestedBlksize)
{
    bool bReceived;
    bool bBlockCorrectlyRetrieved;
    uint32_t requestedData;

    requestedData = requestedBlksize + LXMODEM_HEADER_SIZE;
    bBlockCorrectlyRetrieved = false;

    if ((pThis->withCrc == true) || (requestedBlksize > 128))
    {
        requestedData += LXMODEM_CRC16_SIZE;
    }
    else
    {
        requestedData += LXMODEM_CHKSUM_SIZE;
    }

    bReceived = lmodem_getchar(pThis, pThis->blk_buffer.buffer, requestedData);
    if (bReceived)
    {
        bBlockCorrectlyRetrieved  = lxmodem_check_block_no_and_crc(pThis, expectedBlkNumber, requestedBlksize);
    }

    return bBlockCorrectlyRetrieved;
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

static bool  lxmodem_check_block_no_and_crc(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t requestedBlksize)
{
    bool bBlockCorrectlyRetrieved;
    uint8_t complement;

    bBlockCorrectlyRetrieved = false;
    complement =  ~expectedBlkNumber;

    if ((pThis->blk_buffer.buffer[0] == expectedBlkNumber) &&
        (pThis->blk_buffer.buffer[1] == complement))
    {
        bBlockCorrectlyRetrieved = true;
    }
    else
    {
        DBG("wrong blknumber %d received, expected %d\n", pThis->blk_buffer.buffer[0], expectedBlkNumber);
    }

    if (bBlockCorrectlyRetrieved)
    {
        bBlockCorrectlyRetrieved = lxmodem_check_crc(pThis, requestedBlksize);
    }

    return bBlockCorrectlyRetrieved;
}


static bool lxmodem_check_crc(modem_context_t* pThis, uint32_t requestedBlksize)
{
    bool bCrcOrChecksumOk;
    bCrcOrChecksumOk = false;

    if ((pThis->withCrc == true) || (requestedBlksize > 128))
    {
        uint16_t crc;
        crc = crc16_doCalcul(&pThis->crc16, pThis->blk_buffer.buffer + LXMODEM_HEADER_SIZE, requestedBlksize, 0, 0);
        if ((((crc & 0xFF00) >> 8) == pThis->blk_buffer.buffer[LXMODEM_HEADER_SIZE + requestedBlksize]) &&
            ((crc & 0xFF) == pThis->blk_buffer.buffer[LXMODEM_HEADER_SIZE + requestedBlksize + 1]))
        {
            DBG("crc ok for block %d\n", pThis->blk_buffer.buffer[0]);
            bCrcOrChecksumOk = true;
        }
    }
    else
    {
        uint8_t chksum;
        chksum = lxmodem_calcul_chksum(pThis->blk_buffer.buffer + LXMODEM_HEADER_SIZE, requestedBlksize);
        if (chksum == pThis->blk_buffer.buffer[requestedBlksize + LXMODEM_HEADER_SIZE])
        {
            DBG("checksum ok for block %d\n", pThis->blk_buffer.buffer[0]);
            bCrcOrChecksumOk = true;
        }
    }
    return bCrcOrChecksumOk;
}
