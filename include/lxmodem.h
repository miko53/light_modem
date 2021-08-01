
#ifndef LX_MODEM_H
#define LX_MODEM_H

#include <stdint.h>
#include <stdbool.h>
#include "crc16.h"

#ifdef	__cplusplus
extern "C" {
#endif

#include "asciitable.h"

#define LXMODEM_128_CHKSUM_BUFFER_MIN_SIZE    (1 + 2 + 128 + 1)
#define LXMODEM_128_CRC_BUFFER_MIN_SIZE       (1 + 2 + 128 + 2)
#define LXMODEM_1K_BUFFER_MIN_SIZE            (1 + 2 + 1024 + 2)

typedef enum
{
    XMODEM,
    YMODEM
} lmodem_protocol;

typedef enum
{
    lxmodem_128_with_chksum,
    lxmodem_128_with_crc,
    lxmodem_1k,
} lxmodem_opts;

typedef struct modem_context modem_context_t;

typedef struct
{
    uint8_t* buffer;
    uint32_t read_offset;
    uint32_t write_offset;
    uint32_t max_size;
} lmodem_buffer;

typedef struct
{
    uint8_t* buffer;
    uint32_t current_size;
    uint32_t max_size;
} lmodem_linebuffer;

struct modem_context
{
    lmodem_protocol protocol;
    crc16_context_t crc16;
    lxmodem_opts opts;
    lmodem_linebuffer blk_buffer;
    lmodem_buffer ramfile;
    bool withCrc;
    bool (*getchar)(modem_context_t* pThis, uint8_t* data, uint32_t size);
    void (*putchar)(modem_context_t* pThis, uint8_t* data, uint32_t size);
};

extern void lmodem_init(modem_context_t* pThis, lxmodem_opts opts);
extern void lmodem_set_putchar_cb(modem_context_t* pThis, void (*putchar)(modem_context_t* pThis, uint8_t* data, uint32_t size));
extern void lmodem_set_getchar_cb(modem_context_t* pThis, bool (*getchar)(modem_context_t* pThis, uint8_t* data, uint32_t size));
extern bool lmodem_set_line_buffer(modem_context_t* pThis, uint8_t* buffer, uint32_t size);
extern void lmodem_set_file_buffer(modem_context_t* pThis, uint8_t* buffer, uint32_t size);

extern int32_t lmodem_receive(modem_context_t* pThis, lmodem_protocol protocol);
extern int32_t lmodem_emit(modem_context_t* pThis, lmodem_protocol protocol);

extern bool lmodem_buffer_set_write_offset(lmodem_buffer* pThis, uint32_t newWriteOffset);
extern int32_t lmodem_buffer_read(lmodem_buffer* pThis, uint8_t* buffer, uint32_t size);
extern int32_t lmodem_buffer_write(lmodem_buffer* pThis, uint8_t* buffer, uint32_t size);

#ifdef	__cplusplus
}
#endif



#endif /* LX_MODEM_H */
