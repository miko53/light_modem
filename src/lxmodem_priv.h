
#ifndef LXMODEM_PRIV_H
#define LXMODEM_PRIV_H

#include "lxmodem.h"

#define MODEM_TRACE
#ifdef MODEM_TRACE
#include <stdio.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif /* MODEM_DBG */

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

#endif /* LXMODEM_PRIV_H */
