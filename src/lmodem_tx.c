#include "lmodem.h"
#include "lmodem_priv.h"
#include "lmodem_buffer.h"
#include <string.h>

static  int32_t lxmodem_emit(modem_context_t* pThis);
static int32_t lymodem_emit(modem_context_t* pThis);
static bool lxmodem_decode_preambule(modem_context_t* pThis, uint8_t preambule);
static int32_t lxmode_send_data_blocks(modem_context_t* pThis);
static bool lxmode_build_and_send_one_data_block(modem_context_t* pThis, uint8_t blkNo, uint32_t defaultBlksize, bool withCrc,
        int32_t* nbEmitted);
static void lxmode_reemit_previous_block(modem_context_t* pThis);
static bool lymodem_build_and_send_block0(modem_context_t* pThis);
static void lymodem_send_end_of_bach(modem_context_t* pThis);
static bool lmodem_wait_reception_of(modem_context_t* pThis, uint8_t cntrlChar);
static bool lmodem_wait_reception(modem_context_t* pThis, uint8_t* pReceived);

int32_t lmodem_emit(modem_context_t* pThis, lmodem_protocol protocol)
{
    int32_t emittedBytes;
    emittedBytes = -1;

    pThis->protocol = protocol;
    switch (protocol)
    {
        case XMODEM:
            emittedBytes = lxmodem_emit(pThis);
            break;

        case YMODEM:
            emittedBytes = lymodem_emit(pThis);
            break;

        default:
            break;
    }

    return emittedBytes;
}


static int32_t lxmodem_emit(modem_context_t* pThis)
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
            if ((pThis->protocol == YMODEM) || (pThis->opts == lxmodem_128_with_crc) || (pThis->opts == lxmodem_1k))
            {
                bCanContinue = true;
            }
            break;
    }
    return bCanContinue;
}

static int32_t lxmode_send_data_blocks(modem_context_t* pThis)
{
    bool bFinished;
    bool withCrc;
    uint32_t defaultBlksize;
    uint8_t blkNo;
    uint32_t emittedBytes;
    int32_t nbEmitted;

    defaultBlksize = 128;
    emittedBytes = 0;
    withCrc = false;

    if (pThis->protocol == XMODEM)
    {
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
    }
    else if (pThis->protocol == YMODEM)
    {
        defaultBlksize = 1024;
        withCrc = true;
    }

    blkNo = 1;
    bFinished = false;
    uint8_t ackBytes;
    uint32_t retry;
    uint32_t timeout;
    bool bAckReceived;
    bool isLastBlock;

    timeout = 0;
    retry = 0;

    while (!bFinished)
    {
        if (retry == 0)
        {
            isLastBlock = lxmode_build_and_send_one_data_block(pThis, blkNo, defaultBlksize, withCrc, &nbEmitted);
        }
        else
        {
            lxmode_reemit_previous_block(pThis);
        }

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
                    if (isLastBlock == false)
                    {
                        emittedBytes += nbEmitted;
                    }
                    else
                    {
                        bFinished = true;//we have send the last block
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
        int32_t* nbEmitted)
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
        pThis->blk_buffer.buffer[0] = EOT;
        lmodem_putchar(pThis,  pThis->blk_buffer.buffer, 1);
        pThis->blk_buffer.current_size = 1;
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
        pThis->blk_buffer.current_size = datablockSize;
        *nbEmitted += effectiveBlksize;
    }

    return (bytesToRead == 0) ? true : false;
}

void lxmode_reemit_previous_block(modem_context_t* pThis)
{
    lmodem_putchar(pThis,  pThis->blk_buffer.buffer, pThis->blk_buffer.current_size);
}

static int32_t lymodem_emit(modem_context_t* pThis)
{
    uint32_t nbEmitted;
    uint8_t receivedChar;
    uint32_t retry;
    bool bOk;
    bool bDone;

    receivedChar = 0;
    nbEmitted = -1;
    bOk = false;

    bOk = lmodem_wait_reception_of(pThis, 'C');
    if (bOk)
    {
        lymodem_build_and_send_block0(pThis);

        bDone = false;
        retry = 0;
        while ((!bDone) && (retry < 10))
        {
            lmodem_wait_reception(pThis, &receivedChar);
            switch (receivedChar)
            {
                case ACK:
                    bOk = true;
                    bDone = true;
                    break;

                case CAN:
                    bDone = true;
                    bOk = false;
                    break;

                case NAK:
                    lxmode_reemit_previous_block(pThis);
                    retry++;
                    break;
            }
        }
    }

    if (bOk)
    {
        nbEmitted = lxmodem_emit(pThis);
    }

    if (bOk && (nbEmitted > 0))
    {
        bOk = lmodem_wait_reception_of(pThis, 'C');
    }

    if (bOk)
    {
        lymodem_send_end_of_bach(pThis);
        bDone = false;
        retry = 0;
        while ((!bDone) && (retry < 10))
        {
            lmodem_wait_reception(pThis, &receivedChar);
            switch (receivedChar)
            {
                case ACK:
                    bOk = true;
                    bDone = true;
                    break;

                case CAN:
                    bDone = true;
                    bOk = false;
                    break;

                case NAK:
                    lxmode_reemit_previous_block(pThis);
                    retry++;
                    break;
            }
        }
        if (!bOk)
        {
            nbEmitted = -1;
        }

    }

    return nbEmitted;
}

static bool lmodem_wait_reception_of(modem_context_t* pThis, uint8_t cntrlChar)
{
    uint32_t timeout;
    bool bReceived;
    bool isReceivedOk;
    uint8_t receivedChar;
    isReceivedOk = false;

    bReceived = false;
    timeout = 0;
    while ((isReceivedOk == false) && (timeout < 10))
    {
        //wait preambule
        bReceived = lmodem_getchar(pThis, &receivedChar, 1);
        if ((bReceived) && (receivedChar == cntrlChar))
        {
            isReceivedOk = true;
            break;
        }
        timeout++;
    }

    return isReceivedOk;
}

static bool lmodem_wait_reception(modem_context_t* pThis, uint8_t* pReceived)
{
    uint32_t timeout;
    bool bReceived;
    bool isReceivedOk;
    char receivedChar;
    isReceivedOk = false;

    bReceived = false;
    timeout = 0;
    while ((isReceivedOk == false) && (timeout < 10))
    {
        //wait preambule
        bReceived = lmodem_getchar(pThis, (uint8_t*) &receivedChar, 1);
        if (bReceived)
        {
            if (pReceived != NULL)
            {
                *pReceived = receivedChar;
            }
            isReceivedOk = true;
            break;
        }
        timeout++;
    }

    return isReceivedOk;
}

static const char* lymodem_format[] =
{
    "%d",
    "%d %o",
    "%d %o %o",
    "%d %o %o %o"
};


bool lymodem_build_and_send_block0(modem_context_t* pThis)
{
    bool bOk;

    bOk = false;

    memset(pThis->blk_buffer.buffer, 0, pThis->blk_buffer.max_size);
    //block 0
    pThis->blk_buffer.buffer[1] = 0;
    pThis->blk_buffer.buffer[2] = ~0;

    if ((pThis->file_data.valid & LMODEM_METADATA_FILENAME_VALID) == LMODEM_METADATA_FILENAME_VALID)
    {
        uint32_t maxTocopy;
        uint32_t sizeFilename;

        maxTocopy = pThis->blk_buffer.max_size - 3;

        strncpy((char*) &pThis->blk_buffer.buffer[3], pThis->file_data.filename, maxTocopy);
        pThis->blk_buffer.buffer[3 + maxTocopy] = '\0';

        sizeFilename = strlen((char*) &pThis->blk_buffer.buffer[3]);

        char* pStartMetaData;
        pStartMetaData = (char*) &pThis->blk_buffer.buffer[3 + sizeFilename + 1];

        char* pEndBuffer = (char*) pThis->blk_buffer.buffer + pThis->blk_buffer.max_size;
        pEndBuffer -= 3; // remove crc and final \0

        uint32_t nbMetaDataTocpy;
        int32_t nbWritten;
        nbWritten = 0;
        nbMetaDataTocpy = 0;
        //copy the metadata
        if ((pThis->file_data.valid & LMODEM_METADATA_FILESIZE_VALID) == LMODEM_METADATA_FILESIZE_VALID)
        {
            nbMetaDataTocpy++;
        }
        if ((pThis->file_data.valid & LMODEM_METADATA_MODIFDATE_VALID) == LMODEM_METADATA_MODIFDATE_VALID)
        {
            nbMetaDataTocpy++;
        }
        if ((pThis->file_data.valid & LMODEM_METADATA_PERMISSION_VALID) == LMODEM_METADATA_PERMISSION_VALID)
        {
            nbMetaDataTocpy++;
        }

        if ((pThis->file_data.valid & LMODEM_METADATA_SERIAL_VALID) == LMODEM_METADATA_SERIAL_VALID)
        {
            nbMetaDataTocpy++;
        }

        switch (nbMetaDataTocpy)
        {
            case 0:
            default:
                break;
            case 1:
                nbWritten = snprintf(pStartMetaData, pEndBuffer - pStartMetaData, lymodem_format[nbMetaDataTocpy - 1], pThis->file_data.size);
                break;
            case 2:
                nbWritten = snprintf(pStartMetaData, pEndBuffer - pStartMetaData, lymodem_format[nbMetaDataTocpy - 1], pThis->file_data.size,
                                     pThis->file_data.modif_date);
                break;
            case 3:
                nbWritten = snprintf(pStartMetaData, pEndBuffer - pStartMetaData, lymodem_format[nbMetaDataTocpy - 1], pThis->file_data.size,
                                     pThis->file_data.modif_date, pThis->file_data.permission | 0x8000);
                break;
            case 4:
                nbWritten = snprintf(pStartMetaData, pEndBuffer - pStartMetaData, lymodem_format[nbMetaDataTocpy - 1], pThis->file_data.size,
                                     pThis->file_data.modif_date, pThis->file_data.permission | 0x8000, pThis->file_data.serial_number);
                break;
        }

        bOk = true;
        uint32_t max_size = 3 + sizeFilename + 1 + nbWritten + 1;
        uint32_t nbDataToSend;
        uint32_t effectiveBlksize;
        uint16_t crc;
        if (max_size < 128)
        {
            pThis->blk_buffer.buffer[0] = SOH;
            effectiveBlksize = 128;
            nbDataToSend = 1 + 2 + 128 + 2;
        }
        else
        {
            pThis->blk_buffer.buffer[0] = STX;
            effectiveBlksize = 1024;
            nbDataToSend = 1 + 2 + 1024 + 2;
        }

        crc = crc16_doCalcul(&pThis->crc16, pThis->blk_buffer.buffer + 3, effectiveBlksize, LXMODEM_CRC16_INIT_VALUE, LXMODEM_CRC16_XOR_FINAL);
        pThis->blk_buffer.buffer[3 + effectiveBlksize] = (crc & 0xFF00) >> 8;
        pThis->blk_buffer.buffer[3 + effectiveBlksize + 1] = (crc & 0xFF);
        lmodem_putchar(pThis, pThis->blk_buffer.buffer, nbDataToSend);
        pThis->blk_buffer.current_size = nbDataToSend;
    }

    return bOk;
}

void lymodem_send_end_of_bach(modem_context_t* pThis)
{
    uint16_t crc;
    memset(pThis->blk_buffer.buffer, 0, pThis->blk_buffer.max_size);
    pThis->blk_buffer.buffer[0] = SOH;
    pThis->blk_buffer.buffer[1] = 0;
    pThis->blk_buffer.buffer[2] = ~0;
    crc = crc16_doCalcul(&pThis->crc16, pThis->blk_buffer.buffer + 3, 128, LXMODEM_CRC16_INIT_VALUE, LXMODEM_CRC16_XOR_FINAL);
    pThis->blk_buffer.buffer[3 + 128] = (crc & 0xFF00) >> 8;
    pThis->blk_buffer.buffer[3 + 128 + 1] = (crc & 0xFF);
    lmodem_putchar(pThis, pThis->blk_buffer.buffer, 3 + 128 + 2);
    pThis->blk_buffer.current_size = 3 + 128 + 2;
}
