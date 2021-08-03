
#ifndef LXMODEM_PRIV_H
#define LXMODEM_PRIV_H

#include "lmodem.h"

#define LXMODEM_CRC16_INIT_VALUE       (0)
#define LXMODEM_CRC16_XOR_FINAL        (0)

#define LXMODEM_HEADER_SIZE            (2)
#define LXMODEM_CHKSUM_SIZE            (1)
#define LXMODEM_CRC16_SIZE             (2)
#define LXMODEM_BLOCK_SIZE_128         (128)
#define LXMODEM_BLOCK_SIZE_1024        (1024)

#define LMODEM_METADATA_NB                   (5)
#define LMODEM_METADATA_FILENAME_VALID    (0x01)
#define LMODEM_METADATA_FILESIZE_VALID    (0x02)
#define LMODEM_METADATA_MODIFDATE_VALID   (0x04)
#define LMODEM_METADATA_PERMISSION_VALID  (0x08)
#define LMODEM_METADATA_SERIAL_VALID      (0x10)

#define MODEM_TRACE
#ifdef MODEM_TRACE
#include <stdio.h>
#include <time.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif /* MODEM_DBG */

#define min(a,b)      (((a)<(b))?(a):(b))

static inline void lmodem_putchar(modem_context_t* pThis, uint8_t* data, uint32_t size)
{
#ifdef MODEM_TRACE
    int32_t i;
    time_t t = time(NULL);
    DBG("(%ld) send %d byte(s): ", t, size);
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
        time_t t = time(NULL);
        DBG("(%ld) get %d byte(s): ", t, size);
        for (i = 0; i < (int32_t) (size - 1); i++)
        {
            DBG("0x%.2x-", data[i] & 0xFF);
        }
        DBG("0x%.2x\n", data[i] & 0xFF);
    }
    else
    {
        time_t t = time(NULL);
        DBG("(%ld) get 0 byte\n", t);
    }
#endif /* MODEM_TRACE */
    return b;
}

extern void lxmodem_build_and_send_cancel(modem_context_t* pThis);
extern uint8_t lxmodem_calcul_chksum(uint8_t* buffer, uint32_t size);

#endif /* LXMODEM_PRIV_H */
