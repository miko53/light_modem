
#ifndef LMODEM_BUFFER_H
#define LMODEM_BUFFER_H

#include "lxmodem.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void lmodem_buffer_init(lmodem_buffer* pThis, uint8_t* buffer,  uint32_t max_size);
extern int32_t lmodem_buffer_get_size(lmodem_buffer* pThis);

#ifdef __cplusplus
}
#endif

#endif /* LMODEM_BUFFER_H */
