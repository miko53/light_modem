
#include "lxmodem.h"
#include "lxmodem_priv.h"
#include "lmodem_buffer.h"
#include <string.h>

#define LXMODEM_CRC16_CCITT_POLYNOME   (0x1021)

void lmodem_init(modem_context_t* pThis, lxmodem_opts opts)
{
    memset(pThis, 0, sizeof(modem_context_t));
    pThis->opts = opts;
    //if ((opts == lxmodem_128_with_crc) || (opts == lxmodem_1k))
    //{
    crc16_init(&pThis->crc16, LXMODEM_CRC16_CCITT_POLYNOME);
    //}
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

void lmodem_set_filename_buffer(modem_context_t* pThis, char* buffer, uint32_t size)
{
    pThis->file_data.filename = buffer;
    pThis->file_data.filename_size = size;
}


void lmodem_metadata_set_filename(modem_context_t* pThis, char* filename)
{
    if (pThis->file_data.filename != NULL)
    {
        strncpy(pThis->file_data.filename, filename, pThis->file_data.filename_size);
        pThis->file_data.filename[pThis->file_data.filename_size - 1] = '\0';
        pThis->file_data.valid |= LMODEM_METADATA_FILENAME_VALID;
    }
}

void lmodem_metadata_set_filesize(modem_context_t* pThis, uint32_t size)
{
    pThis->file_data.size = size;
    pThis->file_data.valid |= LMODEM_METADATA_FILESIZE_VALID;
}

void lmodem_metadata_set_modif_time(modem_context_t* pThis, uint32_t modif_date)
{
    pThis->file_data.modif_date = modif_date;
    pThis->file_data.valid |= LMODEM_METADATA_MODIFDATE_VALID;
}

void lmodem_metadata_set_permission(modem_context_t* pThis, uint32_t perm)
{
    pThis->file_data.permission = perm;
    pThis->file_data.valid |= LMODEM_METADATA_PERMISSION_VALID;
}

void lmodem_metadata_set_serial(modem_context_t* pThis, uint32_t serial)
{
    pThis->file_data.serial_number = serial;
    pThis->file_data.valid |= LMODEM_METADATA_SERIAL_VALID;
}


bool lmodem_metadata_get_filename(modem_context_t* pThis, char* filename, uint32_t size)
{
    bool bOk;
    bOk = false;
    if ((pThis->file_data.valid & LMODEM_METADATA_FILENAME_VALID) == LMODEM_METADATA_FILENAME_VALID)
    {
        if (filename != NULL)
        {
            memcpy(filename, pThis->file_data.filename, size);
            filename[size - 1] = '\0';
            bOk = true;
        }
    }

    return bOk;
}


bool lmodem_metadata_get_filesize(modem_context_t* pThis, uint32_t* filesize)
{
    bool bOk;
    bOk = false;
    if ((pThis->file_data.valid & LMODEM_METADATA_FILENAME_VALID) == LMODEM_METADATA_FILENAME_VALID)
    {
        if (filesize != NULL)
        {
            *filesize =  pThis->file_data.size;
            bOk = true;
        }
    }

    return bOk;
}

bool lmodem_metadata_get_modif_time(modem_context_t* pThis, uint32_t* modiftime)
{
    bool bOk;
    bOk = false;
    if ((pThis->file_data.valid & LMODEM_METADATA_FILENAME_VALID) == LMODEM_METADATA_FILENAME_VALID)
    {
        if (modiftime != NULL)
        {
            *modiftime =  pThis->file_data.modif_date;
            bOk = true;
        }
    }

    return bOk;
}

bool lmodem_metadata_get_permission(modem_context_t* pThis, uint32_t* mode)
{
    bool bOk;
    bOk = false;
    if ((pThis->file_data.valid & LMODEM_METADATA_FILENAME_VALID) == LMODEM_METADATA_FILENAME_VALID)
    {
        if (mode != NULL)
        {
            *mode =  pThis->file_data.permission;
            bOk = true;
        }
    }

    return bOk;
}

bool lmodem_metadata_get_serial(modem_context_t* pThis, uint32_t* serial)
{
    bool bOk;
    bOk = false;
    if ((pThis->file_data.valid & LMODEM_METADATA_FILENAME_VALID) == LMODEM_METADATA_FILENAME_VALID)
    {
        if (serial != NULL)
        {
            *serial =  pThis->file_data.serial_number;
            bOk = true;
        }
    }

    return bOk;
}
