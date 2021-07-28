
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "lxmodem.h"
#include "serial.h"

#define XMODEM_BUFFER_BLK_SIZE (132)
#define BUFFER_FILE_SIZE       (65*1024)

static int32_t serial_fd;
static modem_context_t xmodem_ctx;
static uint8_t xmodem_buffer[XMODEM_BUFFER_BLK_SIZE];

static uint8_t xmodem_recvFile[BUFFER_FILE_SIZE];

static bool serial_getchar(modem_context_t* pThis, uint8_t* data, uint32_t size);
static void serial_putchar(modem_context_t* pThis, uint8_t* data, uint32_t size);

int main(int argc, char* argv[])
{
    bool bOk;

    serial_fd = serial_setup("/dev/ttyUSB0", 115200, 0, 0, 1);
    if (serial_fd < 0)
    {
        fprintf(stderr, "unable to open /dev/tty/USB0\n");
        return EXIT_FAILURE;
    }

    lmodem_init(&xmodem_ctx, lxmodem_128_with_chksum);
    bOk = lmodem_set_buffer(&xmodem_ctx, xmodem_buffer, XMODEM_BUFFER_BLK_SIZE);
    assert(bOk == true);

    lmodem_set_file_buffer(&xmodem_ctx, xmodem_recvFile, BUFFER_FILE_SIZE);

    lmodem_set_getchar_cb(&xmodem_ctx, serial_getchar);
    lmodem_set_putchar_cb(&xmodem_ctx, serial_putchar);


    uint32_t nbBytesReceived;
    nbBytesReceived = lxmodem_receive(&xmodem_ctx);


    fprintf(stdout, "Test (%d bytes received)\n", nbBytesReceived);

    serial_close(serial_fd);

    return EXIT_SUCCESS;
}

bool serial_getchar(modem_context_t* pThis, uint8_t* data, uint32_t size)
{
    (void) pThis;
    return serial_read(serial_fd, data, size);
}

void serial_putchar(modem_context_t* pThis, uint8_t* data, uint32_t size)
{
    (void) pThis;
    serial_write(serial_fd, data, size);
}
