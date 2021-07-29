#include <string.h>
#include "lxmodem.h"
#include <assert.h>

#define LXMODEM_CRC16_CCITT_POLYNOME   (0x1021)
#define LXMODEM_HEADER_SIZE            (2)
#define LXMODEM_CHKSUM_SIZE            (1)
#define LXMODEM_CRC16_SIZE             (2)
#define LXMODEM_BLOCK_SIZE_128         (128)
#define LXMODEM_BLOCK_SIZE_1024        (1024)

#define MODEM_TRACE
#ifdef MODEM_TRACE
#include <stdio.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)

#else
#define DBG(...)
#endif /* MODEM_DBG */

typedef enum
{
    LXMODEM_RECV_OK,
    LXMODEM_RECV_PREVIOUS_BLOCK,
    LXMODEM_RECV_ERROR
} lxmodem_reception_status;

static uint8_t lxmodem_calcul_chksum(uint8_t* buffer, uint32_t size);
static void lxmodem_build_and_send_preambule(modem_context_t* pThis);
static lxmodem_reception_status lxmodem_receive_block(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t expectedBlksize);
static lxmodem_reception_status lxmodem_check_block_no_and_crc(modem_context_t* pThis, uint8_t expectedBlkNumber,
        uint32_t requestedBlksize);
static lxmodem_reception_status lxmodem_check_crc(modem_context_t* pThis, uint32_t requestedBlksize);
static void lxmodem_build_and_send_reply(modem_context_t* pThis, lxmodem_reception_status rcvStatus);
static void lxmodem_build_and_send_cancel(modem_context_t* pThis);

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
    pThis->ramfile.buffer = buffer;
    pThis->ramfile.max_size = size;
    pThis->ramfile.current_size = 0;
}

uint32_t lxmodem_receive(modem_context_t* pThis)
{
    uint32_t receivedBytes;
    bool bFinished;
    bool bReceived;
    lxmodem_reception_status rcvStatus;
    uint8_t header;
    uint8_t expectedBlkNumber;
    uint32_t blksize;
    uint32_t canCharReceived;
    uint32_t nbRetry;

    nbRetry = 0;
    canCharReceived = 0;
    receivedBytes = 0;
    expectedBlkNumber = 1;

    //send that we are ready
    lxmodem_build_and_send_preambule(pThis);

    bFinished = false;
    while (!bFinished)
    {
        rcvStatus = LXMODEM_RECV_ERROR;
        //receive block by block
        bReceived = lmodem_getchar(pThis, &header, 1);
        if (bReceived)
        {
            switch (header)
            {
                case SOH:
                    //read 128 blzsize
                    canCharReceived = 0;
                    blksize = LXMODEM_BLOCK_SIZE_128;
                    rcvStatus = lxmodem_receive_block(pThis, expectedBlkNumber, blksize);
                    break;

                case STX:
                    //read 1k blzsize
                    canCharReceived = 0;
                    DBG("request 1k\n");
                    if (pThis->opts == lxmodem_1k)
                    {
                        blksize = LXMODEM_BLOCK_SIZE_1024;
                        rcvStatus = lxmodem_receive_block(pThis, expectedBlkNumber, blksize);
                    }
                    break;

                case EOT:
                    //end of transfert
                    canCharReceived = 0;
                    rcvStatus = LXMODEM_RECV_OK;
                    blksize = 0;
                    bFinished = true;
                    break;

                case CAN:
                    canCharReceived++;
                    if (canCharReceived >= 2)
                    {
                        bFinished = true;
                        receivedBytes = -1;
                        DBG("two CAN received -> abort\n");
                    }
                    break;

                default:
                    break;
            }

            lxmodem_build_and_send_reply(pThis, rcvStatus);
            if (rcvStatus == LXMODEM_RECV_OK)
            {
                expectedBlkNumber++;
                receivedBytes += blksize;
                nbRetry = 0;
            }
            else
            {
                nbRetry++;
                if (nbRetry >= 10)
                {
                    receivedBytes = -1;
                    bFinished = true;
                    lxmodem_build_and_send_cancel(pThis);
                    DBG("max retry reached -> abort\n");
                }
            }
        }
    }

    return receivedBytes;
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

static lxmodem_reception_status lxmodem_receive_block(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t requestedBlksize)
{
    bool bReceived;
    lxmodem_reception_status blockCorrectlyRetrieved;
    uint32_t requestedData;

    blockCorrectlyRetrieved = LXMODEM_RECV_ERROR;
    requestedData = requestedBlksize + LXMODEM_HEADER_SIZE;

    if ((pThis->withCrc == true) || (requestedBlksize > LXMODEM_BLOCK_SIZE_128))
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
        blockCorrectlyRetrieved  = lxmodem_check_block_no_and_crc(pThis, expectedBlkNumber, requestedBlksize);
    }

    return blockCorrectlyRetrieved;
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

static lxmodem_reception_status lxmodem_check_block_no_and_crc(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t requestedBlksize)
{
    lxmodem_reception_status rcvStatus;
    uint8_t complement;

    complement =  ~expectedBlkNumber;
    rcvStatus = LXMODEM_RECV_ERROR;

    if ((pThis->blk_buffer.buffer[0] == expectedBlkNumber) &&
        (pThis->blk_buffer.buffer[1] == complement))
    {
        rcvStatus = LXMODEM_RECV_OK;
    }
    else
    {
        uint8_t previousBlkNumber = expectedBlkNumber - 1;
        complement =  ~previousBlkNumber;
        if ((pThis->blk_buffer.buffer[0] == previousBlkNumber) &&
            (pThis->blk_buffer.buffer[1] == complement))
        {
            rcvStatus = LXMODEM_RECV_PREVIOUS_BLOCK;
            DBG("reception of retry block %d\n", previousBlkNumber);
        }
        else
        {
            DBG("wrong blknumber %d received, expected %d\n", pThis->blk_buffer.buffer[0], expectedBlkNumber);
        }
    }

    if (rcvStatus == LXMODEM_RECV_OK)
    {
        rcvStatus = lxmodem_check_crc(pThis, requestedBlksize);
    }

    return rcvStatus;
}


static lxmodem_reception_status lxmodem_check_crc(modem_context_t* pThis, uint32_t requestedBlksize)
{
    lxmodem_reception_status crcOrChecksumOk;
    crcOrChecksumOk = LXMODEM_RECV_ERROR;

    if ((pThis->withCrc == true) || (requestedBlksize > LXMODEM_BLOCK_SIZE_128))
    {
        uint16_t crc;
        uint8_t hiCrc;
        uint8_t loCrc;

        crc = crc16_doCalcul(&pThis->crc16, pThis->blk_buffer.buffer + LXMODEM_HEADER_SIZE, requestedBlksize, 0, 0);
        hiCrc = ((crc & 0xFF00) >> 8);
        loCrc = (crc & 0xFF);

        if ((hiCrc == pThis->blk_buffer.buffer[LXMODEM_HEADER_SIZE + requestedBlksize]) &&
            (loCrc == pThis->blk_buffer.buffer[LXMODEM_HEADER_SIZE + requestedBlksize + 1]))
        {
            DBG("crc ok for block %d\n", pThis->blk_buffer.buffer[0]);
            crcOrChecksumOk = LXMODEM_RECV_OK;
        }
    }
    else
    {
        uint8_t chksum;
        chksum = lxmodem_calcul_chksum(pThis->blk_buffer.buffer + LXMODEM_HEADER_SIZE, requestedBlksize);
        if (chksum == pThis->blk_buffer.buffer[requestedBlksize + LXMODEM_HEADER_SIZE])
        {
            DBG("checksum ok for block %d\n", pThis->blk_buffer.buffer[0]);
            crcOrChecksumOk = LXMODEM_RECV_OK;
        }
    }
    return crcOrChecksumOk;
}

static void lxmodem_build_and_send_reply(modem_context_t* pThis, lxmodem_reception_status rcvStatus)
{
    uint8_t ack;
    if ((rcvStatus == LXMODEM_RECV_OK) || (rcvStatus == LXMODEM_RECV_PREVIOUS_BLOCK))
    {
        ack = ACK;
    }
    else
    {
        ack = NAK;
    }
    lmodem_putchar(pThis, &ack, 1);
}

void lxmodem_build_and_send_cancel(modem_context_t* pThis)
{
    uint8_t buffer[2];
    buffer[0] = CAN;
    buffer[1] = CAN;
    lmodem_putchar(pThis, buffer, 2);
}
