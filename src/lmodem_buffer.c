#include "lmodem_buffer.h"
#include <string.h>

void lmodem_buffer_init(lmodem_buffer* pThis, uint8_t* buffer,  uint32_t max_size)
{
    pThis->buffer = buffer;
    pThis->max_size = max_size;
    pThis->read_offset = 0;
    pThis->write_offset = 0;
}

int32_t lmodem_buffer_get_remaining_capacity(lmodem_buffer* pThis)
{
    return pThis->max_size - pThis->write_offset;
}

int32_t lmodem_buffer_write(lmodem_buffer* pThis, uint8_t* buffer, uint32_t size)
{
    int32_t remaining_bytes = lmodem_buffer_get_remaining_capacity(pThis);
    int32_t nbToCopy;

    if (remaining_bytes > (int32_t) size)
    {
        nbToCopy = size;
    }
    else
    {
        nbToCopy = remaining_bytes - size;
        if (nbToCopy < 0)
        {
            nbToCopy = 0;
        }
    }

    memcpy(&pThis->buffer[pThis->write_offset], buffer, nbToCopy);
    pThis->write_offset += nbToCopy;
    return nbToCopy;
}

int32_t lmodem_buffer_get_size(lmodem_buffer* pThis)
{
    return pThis->write_offset - pThis->read_offset;
}

int32_t lmodem_buffer_read(lmodem_buffer* pThis, uint8_t* buffer, uint32_t size)
{
    int32_t currentSize;
    int32_t readSize;

    currentSize = lmodem_buffer_get_size(pThis);

    if (currentSize < (int32_t) size)
    {
        readSize = currentSize;
    }
    else
    {
        readSize = size;
    }

    memcpy(buffer, &pThis->buffer[pThis->read_offset], readSize);
    pThis->read_offset += readSize;
    return readSize;
}


bool lmodem_buffer_set_write_offset(lmodem_buffer* pThis, uint32_t newWriteOffset)
{
    bool b;
    b = true;
    if (newWriteOffset <= pThis->max_size)
    {
        pThis->write_offset = newWriteOffset;
    }
    else
    {
        b = false;
    }

    return b;
}
