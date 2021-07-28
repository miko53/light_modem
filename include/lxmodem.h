
#ifndef LX_MODEM_H
#define LX_MODEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include "asciitable.h"

typedef enum
{
    lxmodem_128_with_chksum,
    lxmodem_128_with_crc,
    lxmodem_1k
} lxmodem_opts;

typedef struct modem_context modem_context_t;

typedef struct
{
    uint8_t* buffer;
    uint32_t current_size;
    uint32_t max_size;
} lmodem_buffer;

struct modem_context
{
    lxmodem_opts opts;
    lmodem_buffer blk_buffer;
    lmodem_buffer ramfile;
    uint32_t blksize;
    bool withCrc;
    bool (*getchar)(modem_context_t* pThis, uint8_t* data, uint32_t size);
    void (*putchar)(modem_context_t* pThis, uint8_t* data, uint32_t size);
};

extern void lmodem_init(modem_context_t* pThis, lxmodem_opts opts);
extern void lmodem_set_putchar_cb(modem_context_t* pThis, void (*putchar)(modem_context_t* pThis, uint8_t* data, uint32_t size));
extern void lmodem_set_getchar_cb(modem_context_t* pThis, bool (*getchar)(modem_context_t* pThis, uint8_t* data, uint32_t size));
extern bool lmodem_set_buffer(modem_context_t* pThis, uint8_t* buffer, uint32_t size);
extern void lmodem_set_file_buffer(modem_context_t* pThis, uint8_t* buffer, uint32_t size);

extern uint32_t lxmodem_receive(modem_context_t* pThis);





#ifdef	__cplusplus
}
#endif



#endif /* LX_MODEM_H */
