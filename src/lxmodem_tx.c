#include "lxmodem.h"
#include "lxmodem_priv.h"
#include "lmodem_buffer.h"
#include <string.h>

static bool lxmodem_decode_preambule(modem_context_t* pThis, uint8_t preambule);
static uint32_t lxmode_send_data_blocks(modem_context_t* pThis);
static bool lxmode_build_and_send_one_data_block(modem_context_t* pThis, uint8_t blkNo, uint32_t defaultBlksize,  uint32_t datablockSize,
        bool withCrc);

uint32_t lxmodem_emit(modem_context_t* pThis)
{
    uint32_t wait;
    uint32_t emittedBytes;
    uint8_t preambule;
    bool bFinished;
    bool bReceived;

    bFinished = false;
    wait = 0;
    emittedBytes = 0;

    while (!bFinished)
    {
        //wait preambule
        bReceived = lmodem_getchar(pThis, &preambule, 1);
        if (bReceived)
        {
            bool bCanContinue;
            bCanContinue = lxmodem_decode_preambule(pThis, preambule);
            if (!bCanContinue)
            {
                DBG("options are not compatible stop...\n");
                lxmodem_build_and_send_cancel(pThis);
                bFinished = true;
                emittedBytes = -1;
            }
            else
            {
                emittedBytes = lxmode_send_data_blocks(pThis);
                bFinished = true;
            }
        }
        else
        {
            wait++;
            if (wait > 10)
            {
                bFinished = true;
                emittedBytes = -1;
            }
        }
    }

    return emittedBytes;
}


static bool lxmodem_decode_preambule(modem_context_t* pThis, uint8_t preambule)
{
    bool bCanContinue;
    bCanContinue = false;
    switch (preambule)
    {
        case NAK:
            if (pThis->opts == lxmodem_128_with_chksum)
            {
                bCanContinue = true;
            }
            break;

        case 'C':
            if ((pThis->opts == lxmodem_128_with_crc) || (pThis->opts == lxmodem_1k))
            {
                bCanContinue = true;
            }
            break;
    }
    return bCanContinue;
}

static uint32_t lxmode_send_data_blocks(modem_context_t* pThis)
{
    bool bFinished;
    bool withCrc;
    uint32_t blksize;
    uint32_t datablockSize;
    uint8_t blkNo;
    uint32_t emittedBytes;

    emittedBytes = 0;
    withCrc = false;
    switch (pThis->opts)
    {
        case lxmodem_128_with_chksum:
            blksize = 128;
            withCrc = false;
            datablockSize = 1 + 2 + 128 + 1;
            break;

        case lxmodem_128_with_crc:
            blksize = 128;
            withCrc = true;
            datablockSize = 1 + 2 + 128 + 2;
            break;

        case lxmodem_1k:
            blksize = 1024;
            withCrc = true;
            datablockSize = 1 + 2 + 1024 + 2;
            break;

        default:
            break;
    }


    blkNo = 1;
    bFinished = false;
    uint8_t ackBytes;
    uint32_t retry;
    uint32_t timeout;
    bool bAckReceived;

    timeout = 0;
    retry = 0;

    while (!bFinished)
    {
        bFinished = lxmode_build_and_send_one_data_block(pThis, blkNo, blksize, datablockSize, withCrc);

        bAckReceived = false;
        timeout = 0;
        while ((bAckReceived == false) && (timeout < 10))
        {
            bAckReceived = lmodem_getchar(pThis, &ackBytes, 1);
            timeout++;
        }

        if (bAckReceived)
        {
            switch (ackBytes)
            {
                case ACK:
                    blkNo++;
                    if (!bFinished)
                    {
                        emittedBytes += blksize;
                    }
                    retry = 0;
                    break;

                case NAK:
                    retry++;
                    break;

                case CAN:
                default:
                    retry++;
                    if (retry >= 2)
                    {
                        emittedBytes = -1;
                        bFinished = true;
                    }
                    break;
            }
        }
        else
        {
            retry++;
        }
        if (retry >= 10)
        {
            emittedBytes = -1;
            bFinished = true;
        }
    }

    return emittedBytes;
}

bool lxmode_build_and_send_one_data_block(modem_context_t* pThis, uint8_t blkNo, uint32_t defaultBlksize,  uint32_t datablockSize,
        bool withCrc)
{
    uint32_t remainingBytes;
    bool isDone;
    uint32_t bytesToRead;

    isDone = false;
    bytesToRead = 0;

    remainingBytes = lmodem_buffer_get_size(&pThis->ramfile);
    if (remainingBytes == 0)
    {
        isDone = true;
    }
    else if (defaultBlksize < remainingBytes)
    {
        bytesToRead = defaultBlksize;
    }
    else
    {
        bytesToRead = remainingBytes;
    }


    if (isDone)
    {
        uint8_t endTransfert;
        endTransfert = EOT;
        lmodem_putchar(pThis,  &endTransfert, 1);
    }
    else
    {
        if (bytesToRead <= 128)
        {
            pThis->blk_buffer.buffer[0] = SOH;
        }
        else
        {
            pThis->blk_buffer.buffer[0] = STX;
        }

        pThis->blk_buffer.buffer[1] = blkNo;
        pThis->blk_buffer.buffer[2] = ~blkNo;

        lmodem_buffer_read(&pThis->ramfile, pThis->blk_buffer.buffer + 3, bytesToRead);

        if (remainingBytes < defaultBlksize)
        {
            memset(pThis->blk_buffer.buffer + 3 + bytesToRead, SUB, defaultBlksize - remainingBytes);
        }

        if (withCrc)
        {
            uint16_t crc;
            crc = crc16_doCalcul(&pThis->crc16, pThis->blk_buffer.buffer + 3, defaultBlksize, LXMODEM_CRC16_INIT_VALUE, LXMODEM_CRC16_XOR_FINAL);
            pThis->blk_buffer.buffer[3 + defaultBlksize] = (crc & 0xFF00) >> 8;
            pThis->blk_buffer.buffer[3 + defaultBlksize + 1] = (crc & 0x00FF);
        }
        else
        {
            uint8_t chksum;
            chksum = lxmodem_calcul_chksum(pThis->blk_buffer.buffer + 3, defaultBlksize);
            pThis->blk_buffer.buffer[3 + defaultBlksize] = (chksum & 0x00FF);
        }

        lmodem_putchar(pThis,  pThis->blk_buffer.buffer, datablockSize);
    }

    return isDone;
}
