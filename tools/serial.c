#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "serial.h"
#include <assert.h>
#include <syslog.h>

static uint32_t serial_convertBaudRateToFlag(uint32_t baudrate);

int32_t serial_set_interface (int32_t fd, int32_t speedFlags, int32_t parity, int32_t rtscts, int32_t nbstop)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        perror("error from tcgetattr");
        return -1;
    }

    cfsetospeed (&tty, speedFlags);
    cfsetispeed (&tty, speedFlags);

    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    //tty.c_iflag &= ~IGNBRK;         // disable break processing
    //tty.c_iflag &= ~(IXON | IXOFF | IXANY ); // shut off xon/xoff ctrl
    //tty.c_lflag = 0;                // no signaling chars, no echo, no canonical processing
    //tty.c_oflag = 0;                // no remapping, no delays

    cfmakeraw(&tty);
    //tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    //tty.c_oflag &= ~OPOST;
    //tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);


    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,

    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;

    if (nbstop <= 1)
    {
        tty.c_cflag &= ~CSTOPB;
    }
    else
    {
        tty.c_cflag |= CSTOPB;
    }

    if (rtscts == 0)
    {
        tty.c_cflag &= ~CRTSCTS;
    }
    else
    {
        tty.c_cflag |= CRTSCTS;
    }

    tcflush(fd, TCIFLUSH);
    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        perror ("error from tcsetattr");
        return -1;
    }
    return 0;
}

void serial_set_blocking (int32_t fd, int32_t should_block)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        perror ("error %d from tggetattr");
        return;
    }

    tty.c_cc[VMIN]  = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        perror ("errorsetting term attributes");
    }
}


int32_t serial_set_baudrate(int32_t fd, int32_t baudrate)
{
    struct termios tty;
    int32_t speedFlags;

    speedFlags = serial_convertBaudRateToFlag(baudrate);
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        perror("error from tcgetattr");
        return -1;
    }

    cfsetospeed (&tty, speedFlags);
    cfsetispeed (&tty, speedFlags);

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        perror ("errorsetting term attributes");
        return -1;
    }
    return 0;
}


int32_t serial_setup(char* device, int32_t speed, int32_t parity, int32_t rtscts, int32_t nbstop)
{
    int32_t fd;
    int32_t speedFlags;

    fd = open (device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd >= 0)
    {
        speedFlags = serial_convertBaudRateToFlag(speed);
        serial_set_interface (fd, speedFlags, parity, rtscts, nbstop);
        // set no blocking already set
        //serial_set_blocking (fd, 0);
    }
    return fd;
}

static uint32_t serial_convertBaudRateToFlag(uint32_t baudrate)
{
    uint32_t baudrateValue;

    switch (baudrate)
    {
        case 134:
            baudrateValue = B134;
            break;

        case 150:
            baudrateValue = B150;
            break;

        case 200:
            baudrateValue = B200;
            break;

        case 300:
            baudrateValue = B300;
            break;

        case 600:
            baudrateValue = B600;
            break;

        case 1200:
            baudrateValue = B1200;
            break;

        case 1800:
            baudrateValue = B1800;
            break;

        case 2400:
            baudrateValue = B2400;
            break;

        case 4800:
            baudrateValue = B4800;
            break;

        case 9600:
            baudrateValue = B9600;
            break;

        case 19200:
            baudrateValue = B19200;
            break;

        case 38400:
            baudrateValue = B38400;
            break;

        case 57600:
            baudrateValue = B57600;
            break;

        case 115200:
            baudrateValue = B115200;
            break;

        default:
            baudrateValue = B9600;
            syslog(LOG_WARNING, "unknwon speed value, set to 9600bps");
            break;
    }

    return baudrateValue;
}


bool serial_read(int32_t fd, uint8_t* buffer, uint32_t size)
{
    uint32_t nbRead;
    ssize_t nbCurrentyRead;
    int32_t attempt;
    bool bResult;

    bResult = true;
    attempt = 0;
    nbRead = 0;
    while (nbRead < size)
    {
        nbCurrentyRead = read(fd, &(buffer[nbRead]), size - nbRead);
        if (nbCurrentyRead == -1)
        {
            //syslog(LOG_ERR, "Error on read data errno = %d", errno);
            bResult = false;
            break;
        }
        else if (nbCurrentyRead == 0)
        {
            // #if 0
            attempt++;
            if (attempt > 2)
            {
                //assert(false);
                //if (nbRead > 0)
                //  return true;
                //else
                return false;
            }
            //  #endif
        }
        else
        {
            nbRead += nbCurrentyRead;
            //fprintf(stdout, "Data read\n");
        }
    }

    //fprintf(stdout, "bResult = %d, nbRead = %d\n", bResult, nbRead);
    return bResult;
}

bool serial_write(int32_t fd, uint8_t* buffer, uint32_t size)
{
    ssize_t nbWritten;
    nbWritten = write(fd, buffer, size);
    if (nbWritten == -1)
    {
        //syslog(LOG_ERR, "unable to send data on serial line erro = %d", errno);
    }
    else if (size == (uint32_t) nbWritten)
    {
        return true;
    }

    return false;
}

bool serial_send_char(int32_t fd, char c)
{
    uint8_t d = c;
    return serial_write(fd, &d, 1);
}

void serial_close(int32_t fd)
{
    close(fd);
}

