#include "lxmodem.h"
#include "lxmodem_priv.h"
#include "lmodem_buffer.h"
#include <string.h>

static bool lxmodem_decode_preambule(modem_context_t* pThis, uint8_t preambule);
static uint32_t lxmode_send_data_blocks(modem_context_t* pThis);
static bool lxmode_build_and_send_one_data_block(modem_context_t* pThis, uint8_t blkNo, uint32_t defaultBlksize, bool withCrc,
        uint32_t* nbEmitted);

uint32_t lxmodem_emit(modem_context_t* pThis)
{
    uint32_t timeout;
    uint32_t emittedBytes;
    uint8_t preambule;
    bool bReceived;

    emittedBytes = 0;
    bReceived = false;
    timeout = 0;
    while ((bReceived == false) && (timeout < 10))
    {
        //wait preambule
        bReceived = lmodem_getchar(pThis, &preambule, 1);
        if (bReceived)
        {
            break;
        }
        timeout++;
    }

    if (bReceived)
    {
        bool bCanContinue;
        bCanContinue = lxmodem_decode_preambule(pThis, preambule);
        if (!bCanContinue)
        {
            DBG("options are not compatible stop...\n");
            lxmodem_build_and_send_cancel(pThis);
            emittedBytes = -1;
        }
        else
        {
            emittedBytes = lxmode_send_data_blocks(pThis);
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
    uint32_t defaultBlksize;
    uint8_t blkNo;
    uint32_t emittedBytes;
    uint32_t nbEmitted;

    emittedBytes = 0;
    withCrc = false;
    switch (pThis->opts)
    {
        case lxmodem_128_with_chksum:
            defaultBlksize = 128;
            withCrc = false;
            break;

        case lxmodem_128_with_crc:
            defaultBlksize = 128;
            withCrc = true;
            break;

        case lxmodem_1k:
            defaultBlksize = 1024;
            withCrc = true;
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
        bFinished = lxmode_build_and_send_one_data_block(pThis, blkNo, defaultBlksize, withCrc, &nbEmitted);

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
                        emittedBytes += nbEmitted;
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

bool lxmode_build_and_send_one_data_block(modem_context_t* pThis, uint8_t blkNo, uint32_t defaultBlksize,  bool withCrc,
        uint32_t* nbEmitted)
{
    uint32_t remainingBytes;
    uint32_t bytesToRead;
    uint32_t effectiveBlksize;
    uint32_t datablockSize;

    effectiveBlksize = defaultBlksize;
    *nbEmitted = 0;
    bytesToRead = 0;
    remainingBytes = lmodem_buffer_get_size(&pThis->ramfile);

    bytesToRead = min(defaultBlksize, remainingBytes);
    if (bytesToRead == 0)
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
            effectiveBlksize = 128;
        }
        else
        {
            pThis->blk_buffer.buffer[0] = STX;
            effectiveBlksize = 1024;
        }

        pThis->blk_buffer.buffer[1] = blkNo;
        pThis->blk_buffer.buffer[2] = ~blkNo;

        lmodem_buffer_read(&pThis->ramfile, pThis->blk_buffer.buffer + 3, bytesToRead);

        if (remainingBytes < effectiveBlksize)
        {
            memset(pThis->blk_buffer.buffer + 3 + bytesToRead, SUB, effectiveBlksize - remainingBytes);
        }

        datablockSize = 3 + effectiveBlksize + 1;
        if (withCrc)
        {
            uint16_t crc;
            crc = crc16_doCalcul(&pThis->crc16, pThis->blk_buffer.buffer + 3, effectiveBlksize, LXMODEM_CRC16_INIT_VALUE, LXMODEM_CRC16_XOR_FINAL);
            pThis->blk_buffer.buffer[3 + effectiveBlksize] = (crc & 0xFF00) >> 8;
            pThis->blk_buffer.buffer[3 + effectiveBlksize + 1] = (crc & 0x00FF);
            datablockSize += 1;
        }
        else
        {
            uint8_t chksum;
            chksum = lxmodem_calcul_chksum(pThis->blk_buffer.buffer + 3, effectiveBlksize);
            pThis->blk_buffer.buffer[3 + effectiveBlksize] = (chksum & 0x00FF);
        }

        lmodem_putchar(pThis,  pThis->blk_buffer.buffer, datablockSize);
        *nbEmitted += effectiveBlksize;
    }

    return (bytesToRead == 0) ? true : false;
}
