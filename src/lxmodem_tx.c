
#include "lxmodem.h"
#include "lxmodem_priv.h"



uint32_t lxmodem_emit(modem_context_t* pThis)
{
    uint8_t buffer[6];
    buffer[0] = 0;
    buffer[1] = 2;

    lmodem_putchar(pThis, buffer, 2);

    return 0;
}


