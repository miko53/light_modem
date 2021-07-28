#include "crc16.h"

void crc16_init(crc16_context_t* pThis, uint16_t polynome)
{
    uint32_t i;
    uint32_t j;
    uint32_t crc;
    uint16_t highbit;

    pThis->polynome = polynome;

    for (i = 0; i < 256; i++)
    {
        crc = i << 8;
        for (j = 0; j < 8 ; j++)
        {
            highbit = (crc & 0x8000);
            crc = crc << 1;
            if (highbit)
            {
                crc ^= (uint32_t) polynome;
            }
        }
        pThis->crctab[i] = crc & 0xFFFF;
    }
}

uint16_t crc16_doCalcul(crc16_context_t* pThis, uint8_t* data, uint32_t len, uint16_t initValue, uint16_t xorFinal)
{
    uint16_t crc;
    uint32_t i;
    uint32_t k;

    crc = initValue;

    for (i = 0; i < len; i++)
    {
        k = ((crc >> 8) ^ (uint16_t) data[i] ) & 0xFF;
        crc = (crc << 8) ^ pThis->crctab[k];
    }

    crc ^= xorFinal;

    return crc;
}
