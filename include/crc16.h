#ifndef _CRC_16_H
#define _CRC_16_H

#include <stdint.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t crctab[256];
    uint16_t polynome;
} crc16_context_t;

extern void crc16_init(crc16_context_t* pThis, uint16_t polynome);
extern uint16_t crc16_doCalcul(crc16_context_t* pThis, uint8_t* data, uint32_t len, uint16_t initValue, uint16_t xorFinal);

#ifdef	__cplusplus
}
#endif



#endif /* _CRC_16_H */
