#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "serial.h"
#include "asciitable.h"

static inline bool isCtrlCode(char c)
{
    if ((c >= 0) && (c < FIRST_PRINTABLE))
    {
        return true;
    }
    else
    {
        return false;
    }
}
static char* controlCode[] =
{
    "NUL",
    "SOH",
    "STX",
    "ETX",
    "EOT",
    "ENQ",
    "ACK",
    "BEL",
    "BS",
    "TAB",
    "LF",
    "VT",
    "FF",
    "CR",
    "SO",
    "SI",
    "DLE",
    "DC1",
    "DC2",
    "DC3",
    "DC4",
    "NAK",
    "SYN",
    "ETB",
    "CAN",
    "EM",
    "SUB",
    "ESC",
    "FS",
    "GS",
    "RS",
    "US"
};

static char* getCtrlCode(char c);
static uint8_t calcul_chksum(uint8_t* buffer, uint32_t size);

int main(int argc, char* argv[])
{
    int32_t fd;
    bool isReceived;
    bool isDone;
    char data;
    char buffer[131 + 1];

    fd = serial_setup("/dev/ttyUSB0", 115200, 0, 0, 1);
    if (fd < 0)
    {
        fprintf(stderr, "unable to open /dev/tty/USB0\n");
        return EXIT_FAILURE;
    }
    isDone = false;

    //serial_send_char(fd, NAK);
    serial_send_char(fd, 'C');

    while (isDone == false)
    {
        isReceived = serial_read(fd, (uint8_t*) &data, 1);
        if (isReceived == true)
        {
            if (data == SOH)
            {
                fprintf(stdout, "SOH: ");
                isReceived = serial_read(fd, (uint8_t*) &buffer, 131 + 1);
                if (isReceived == true)
                {
                    for (uint32_t i = 0; i < 131 + 1; i++)
                    {
                        fprintf(stdout, "0x%.2x-", buffer[i] & 0xFF);
                    }
                    fprintf(stdout, "\n");

                    uint8_t chksum = calcul_chksum((uint8_t*) (buffer + 2), 128 + 1);
                    fprintf(stdout, "chksum = 0x%.2x\n", chksum & 0xFF);

                    serial_send_char(fd, ACK);
                }
                else
                {
                    fprintf(stderr, "unable to receive frame\n");
                }
            }
            else if (data == EOT)
            {
                fprintf(stdout, "EOT: ");
                serial_send_char(fd, ACK);
                fprintf(stdout, "\n");
                isDone = true;
            }
            else
            {
                fprintf(stdout, ">%c (0x%x)", data, data);
                if (isCtrlCode(data) == true)
                {
                    fprintf(stdout, "%s", getCtrlCode(data));
                }
                fprintf(stdout, "\n");
            }
        }
    }

    serial_close(fd);
    return EXIT_SUCCESS;
}

uint8_t calcul_chksum(uint8_t* buffer, uint32_t size)
{
    uint8_t t;
    t = 0;
    for (uint32_t i = 0; i < size; i++)
    {
        t += buffer[i];
    }

    return t;
}

static char* getCtrlCode(char c)
{
    if (isCtrlCode(c) == true)
    {
        return controlCode[(int) c];
    }

    return "";
}
