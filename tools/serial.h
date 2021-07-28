#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#define SERIAL_PARITY_OFF       (0)
#define SERIAL_PARITY_ON        (1)

#define SERIAL_RTSCTS_OFF       (0)
#define SERIAL_RTSCTS_ON        (1)

#define SERIAL_STOPNB_1         (1)
#define SERIAL_STOPNB_2         (2)

extern int32_t  serial_setup(char* device, int32_t speed, int32_t parity, int32_t rtscts, int32_t nbstop);
extern bool serial_read(int32_t fd, uint8_t* buffer, uint32_t size);
extern bool serial_write(int32_t fd, uint8_t* buffer, uint32_t size);
extern int32_t serial_set_baudrate(int32_t fd, int32_t baudrate);
extern void serial_close(int32_t fd);

extern bool serial_send_char(int32_t fd, char c);

#endif /* SERIAL_H */
