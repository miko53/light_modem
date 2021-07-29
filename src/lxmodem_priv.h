
#ifndef LXMODEM_PRIV_H
#define LXMODEM_PRIV_H

#include "lxmodem.h"

#define LXMODEM_CRC16_INIT_VALUE       (0)
#define LXMODEM_CRC16_XOR_FINAL        (0)


#define MODEM_TRACE
#ifdef MODEM_TRACE
#include <stdio.h>
#include <time.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif /* MODEM_DBG */

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
