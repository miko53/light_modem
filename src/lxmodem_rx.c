#include <string.h>
#include "lxmodem.h"
#include "lxmodem_priv.h"
#include "lmodem_buffer.h"
#include <assert.h>
#include <stdlib.h>

typedef enum
{
    LXMODEM_RECV_OK,
    LXMODEM_RECV_PREVIOUS_BLOCK,
    LXMODEM_RECV_ERROR
} lxmodem_reception_status;

static int32_t lxmodem_receive(modem_context_t* pThis);
static int32_t lymodem_receive(modem_context_t* pThis);
static void lxmodem_build_and_send_preambule(modem_context_t* pThis);
static lxmodem_reception_status lxmodem_receive_block(modem_context_t* pThis, uint8_t expectedBlkNumber, uint32_t expectedBlksize);
static lxmodem_reception_status lxmodem_check_block_no_and_crc(modem_context_t* pThis, uint8_t expectedBlkNumber,
        uint32_t requestedBlksize);
static lxmodem_reception_status lxmodem_check_crc(modem_context_t* pThis, uint32_t requestedBlksize);
static void lxmodem_build_and_send_reply(modem_context_t* pThis, lxmodem_reception_status rcvStatus);
static bool lymodem_get_meta_data(modem_context_t* pThis);
static uint32_t lymodem_getValue(bool* isValid, char* pString, int32_t mode);
static lxmodem_reception_status lymodem_receive_block0(modem_context_t* pThis, uint32_t blksize);
static void lymodem_block_next_file(modem_context_t* pThis);
static bool lymodem_decode_block0(modem_context_t* pThis);
char* lymodem_get_next_meta_data_string(char** pString, char* pEndString);

int32_t lmodem_receive(modem_context_t* pThis, lmodem_protocol protocol)
{
    int32_t receivedBytes;
    receivedBytes = -1;

    pThis->protocol = protocol;
    switch (protocol)
    {
        case XMODEM:
            receivedBytes = lxmodem_receive(pThis);
            break;

        case YMODEM:
            receivedBytes = lymodem_receive(pThis);
            break;

        default:
            break;
    }

    return receivedBytes;
}


static int32_t lxmodem_receive(modem_context_t* pThis)
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
                    if ((pThis->opts == lxmodem_1k) || (pThis->protocol == YMODEM))
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
                int32_t nbPutInRamFile;
                nbPutInRamFile = lmodem_buffer_write(&pThis->ramfile, pThis->blk_buffer.buffer + 2, blksize);
                if (nbPutInRamFile != (int32_t) blksize)
                {
                    DBG("enable to put into ramfile -> abort\n");
                    bFinished = true;
                    receivedBytes = -1;
                    uint8_t buffer[2];
                    buffer[0] = CAN;
                    buffer[1] = CAN;
                    lmodem_putchar(pThis, buffer, 2);
                }
                else
                {
                    expectedBlkNumber++;
                    receivedBytes += blksize;
                    nbRetry = 0;
                }
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

    return receivedBytes;
}

void lxmodem_build_and_send_preambule(modem_context_t* pThis)
{
    char p;

    if (pThis->protocol == XMODEM)
    {
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
    }
    else if (pThis->protocol == YMODEM)
    {
        p = 'C';
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

    if ((pThis->withCrc == true) || (pThis->protocol == YMODEM) || (requestedBlksize > LXMODEM_BLOCK_SIZE_128))
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

uint8_t lxmodem_calcul_chksum(uint8_t* buffer, uint32_t size)
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

    if ((pThis->withCrc == true) || (pThis->protocol == YMODEM) || (requestedBlksize > LXMODEM_BLOCK_SIZE_128))
    {
        uint16_t crc;
        uint8_t hiCrc;
        uint8_t loCrc;

        crc = crc16_doCalcul(&pThis->crc16, pThis->blk_buffer.buffer + LXMODEM_HEADER_SIZE, requestedBlksize,
                             LXMODEM_CRC16_INIT_VALUE, LXMODEM_CRC16_XOR_FINAL);
        hiCrc = ((crc & 0xFF00) >> 8);
        loCrc = (crc & 0xFF);

        if ((hiCrc == pThis->blk_buffer.buffer[LXMODEM_HEADER_SIZE + requestedBlksize]) &&
            (loCrc == pThis->blk_buffer.buffer[LXMODEM_HEADER_SIZE + requestedBlksize + 1]))
        {
            DBG("crc ok for block %d\n", pThis->blk_buffer.buffer[0]);
            crcOrChecksumOk = LXMODEM_RECV_OK;
        }
        else
        {
            DBG("wrong crc: calculated: 0x%.2x, received: 0x%.2x\n", crc,  ((hiCrc << 8) | loCrc));
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

static int32_t lymodem_receive(modem_context_t* pThis)
{
    uint32_t timeout;
    uint8_t startBlock0;
    bool bReceived;
    uint32_t receivedBytes;
    lxmodem_reception_status rxStatus;

    rxStatus = LXMODEM_RECV_ERROR;
    receivedBytes = -1;

    lxmodem_build_and_send_preambule(pThis);

    bReceived = false;
    timeout = 0;
    while ((bReceived == false) && (timeout < 10))
    {
        //wait preambule
        bReceived = lmodem_getchar(pThis, &startBlock0, 1);
        if (bReceived)
        {
            switch (startBlock0)
            {
                case SOH:
                    rxStatus = lymodem_receive_block0(pThis, 128);
                    break;

                case STX:
                    rxStatus = lymodem_receive_block0(pThis, 1024);
                    break;

                default:
                    break;
            }
        }
        timeout++;
    }

    if (rxStatus == LXMODEM_RECV_OK)
    {
        bool bBlock0Ok;
        bBlock0Ok = lymodem_decode_block0(pThis);
        if (bBlock0Ok == true)
        {
            receivedBytes = lxmodem_receive(pThis);
            //dont accept another file...
            lymodem_block_next_file(pThis);
        }
    }

    if (receivedBytes > 0)
    {
        if ((pThis->file_data.valid & LMODEM_METADATA_FILESIZE_VALID) == LMODEM_METADATA_FILESIZE_VALID)
        {
            lmodem_buffer_set_write_offset(&pThis->ramfile, pThis->file_data.size);
        }
    }

    return receivedBytes;
}

static lxmodem_reception_status lymodem_receive_block0(modem_context_t* pThis, uint32_t blksize)
{
    bool bReceived;
    bool bFinished;
    uint32_t retry;
    bReceived = false;
    bFinished = false;
    lxmodem_reception_status rxStatus;

    retry = 0;
    while (bFinished == false)
    {
        bReceived = lmodem_getchar(pThis, pThis->blk_buffer.buffer, 2 + blksize + 2);
        if (bReceived == true)
        {
            rxStatus = lxmodem_check_block_no_and_crc(pThis, 0, blksize);
            lxmodem_build_and_send_reply(pThis, rxStatus);
            if (rxStatus == LXMODEM_RECV_OK)
            {
                bFinished = true;
                break;
            }
            else
            {
                retry++;
                if (retry >= 10)
                {
                    lxmodem_build_and_send_cancel(pThis);
                    bFinished = true;
                }
            }
        }
    }

    return rxStatus;
}

static void lymodem_block_next_file(modem_context_t* pThis)
{
    uint8_t startBlock0;
    uint32_t timeout;
    bool bReceived;

    lxmodem_build_and_send_preambule(pThis);
    bReceived = false;
    timeout = 0;
    while ((bReceived == false) && (timeout < 10))
    {
        bReceived = lmodem_getchar(pThis, &startBlock0, 1);
        if (bReceived == true)
        {
            switch (startBlock0)
            {
                case SOH:
                    bReceived = lmodem_getchar(pThis, pThis->blk_buffer.buffer, 128 + 2 + 2);
                    break;

                case STX:
                    bReceived = lmodem_getchar(pThis, pThis->blk_buffer.buffer, 1024 + 2 + 2);
                    break;
                default:
                    break;
            }

            if (bReceived == true)
            {
                if (pThis->blk_buffer.buffer[2] == '\0')
                {
                    lxmodem_build_and_send_reply(pThis, LXMODEM_RECV_OK);
                }
                else
                {
                    lxmodem_build_and_send_cancel(pThis);
                }
            }
        }
        timeout++;
    }
}

void lmodem_metadata_clean(lmodem_file_characteristics* pMetaData)
{
    pMetaData->filename[0] = '\0';
    pMetaData->modif_date = 0;
    pMetaData->permission = 0;
    pMetaData->serial_number = 0;
    pMetaData->size = 0;
    pMetaData->valid = 0;
}

bool lymodem_decode_block0(modem_context_t* pThis)
{
    char* pString;
    bool bResult = false;
    pString = (char*) (pThis->blk_buffer.buffer + 2);

    lmodem_metadata_clean(&pThis->file_data);

    if (*pString == '\0')
    {
        //no data
        DBG("no filename '%s', end of bach\n", pThis->file_data.filename);
        bResult = false;
    }
    else
    {
        strncpy(pThis->file_data.filename, pString, pThis->file_data.filename_size - 1);
        pThis->file_data.filename[pThis->file_data.filename_size - 1] = '\0';
        pThis->file_data.valid |= LMODEM_METADATA_FILENAME_VALID;

        DBG("reception of file '%s'\n", pThis->file_data.filename);
        bResult = lymodem_get_meta_data(pThis);
    }
    return bResult;
}

bool lymodem_get_meta_data(modem_context_t* pThis)
{
    char* pString;
    char* pEndString;
    bool bResult;

    bResult = true;
    //set pString at the end of filename
    pString = (char*) (pThis->blk_buffer.buffer + 2 + strlen(pThis->file_data.filename));
    pEndString = (pString + pThis->blk_buffer.max_size);

    while ((*pString == '\0') && (pString < pEndString))
    {
        pString++;
    }

    if (pString < pEndString)
    {
        //decode another fields
        uint32_t nbFields;
        nbFields = 0;
        while (nbFields < (LMODEM_METADATA_NB - 1))
        {
            char* pData;
            pData = lymodem_get_next_meta_data_string(&pString, pEndString);
            if (pData == NULL)
            {
                break;
            }
            else
            {
                switch (nbFields)
                {
                    case 0:
                        //file size
                        pThis->file_data.size = lymodem_getValue(&bResult, pData, 10);
                        pThis->file_data.valid |= LMODEM_METADATA_FILESIZE_VALID;
                        DBG("file size = '%d'\n", pThis->file_data.size );
                        break;

                    case 1:
                        //modif date
                        pThis->file_data.modif_date = lymodem_getValue(&bResult, pData, 8);
                        pThis->file_data.valid |= LMODEM_METADATA_MODIFDATE_VALID;
                        DBG("modif date = '%d'\n", pThis->file_data.modif_date );
                        break;

                    case 2:
                        //file mode
                        pThis->file_data.permission = lymodem_getValue(&bResult, pData, 8);
                        //remove hiBit indication for unix permission type
                        pThis->file_data.permission &= ~0x8000;
                        pThis->file_data.valid |= LMODEM_METADATA_PERMISSION_VALID;
                        DBG("filemode = 0'%o'\n", pThis->file_data.permission );
                        break;

                    case 3:
                        //serial number
                        pThis->file_data.serial_number = lymodem_getValue(&bResult, pData, 8);
                        pThis->file_data.valid |= LMODEM_METADATA_SERIAL_VALID;
                        DBG("serial = '%d'\n", pThis->file_data.serial_number );
                        break;

                    default://not possible
                        break;

                }
            }
            nbFields++;
            if (bResult == false)
            {
                break;
            }
        }
    }

    return bResult;
}

uint32_t lymodem_getValue(bool* isValid, char* pString, int32_t mode)
{
    char* pEnd;
    uint32_t r;

    r = strtoul(pString, &pEnd, mode);
    if (*pEnd != '\0')
    {
        *isValid = false;
        DBG("fields '%s' invalid\n", pString);
    }
    else
    {
        *isValid = true;
    }
    return r;
}

char* lymodem_get_next_meta_data_string(char** pString, char* pEndString)
{
    char* pCursor;
    char* pResult;
    pCursor = *pString;
    pResult = *pString;

    if ((*pCursor == '\0') || (pCursor >= pEndString))
    {
        return NULL;
    }

    while ((*pCursor != ' ') && (*pCursor != '\0') && (pCursor < pEndString))
    {
        pCursor++;
    }

    *pCursor = '\0';
    pCursor++;
    *pString = pCursor;

    return pResult;
}
