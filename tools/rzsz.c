
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include "lxmodem.h"
#include "serial.h"
#include <sys/stat.h>


#define XMODEM_BUFFER_BLK_SIZE (133)
#define BUFFER_FILE_SIZE       (1024*1024)

#define BUFFER_FILENAME_SIZE    (256)

typedef enum
{
    OPTS_PROTOCOL,
    OPTS_DEVICE,
    OPTS_SPEED,
    OPTS_PARITY,
    OPTS_CTRL_FLOW,
    OPTS_NB_STOP,
    OPTS_CRC,
    OPTS_1K,
    OPTS_TX,
    OPTS_RX,
    OPTS_FILE,
    OPTS_UNKNOWN = '?'
} OPTS;

typedef struct
{
    lmodem_protocol protocol;
    char* device;
    uint32_t speed;
    uint32_t parity;
    uint32_t ctrl_flow;
    uint32_t nb_stop;
    uint32_t crc;
    uint32_t xmodem_blksize;
    uint32_t tx;
    uint32_t rx;
    char* filename;
} options_t;

static options_t options;

static struct option long_options[] =
{
    /* These options set a flag. */
    {"protocol", required_argument, 0, OPTS_PROTOCOL },
    {"device", required_argument, 0, OPTS_DEVICE },
    {"speed", required_argument, 0, OPTS_SPEED },
    {"parity", required_argument, 0, OPTS_PARITY},
    {"ctrl-flow", required_argument, 0, OPTS_CTRL_FLOW},
    {"nb-stop", required_argument, 0, OPTS_NB_STOP},
    {"crc", no_argument, 0, OPTS_CRC},
    {"1k", no_argument, 0, OPTS_1K},
    {"tx", no_argument, 0, OPTS_TX},
    {"rx", no_argument, 0, OPTS_RX},
    {"file", required_argument, 0, OPTS_FILE},
    {0, 0, 0, 0}
};


static int32_t serial_fd;
static modem_context_t xmodem_ctx;
static uint8_t* xmodem_buffer;
static uint32_t xmodem_buffer_size;
static char xmodem_filename_buffer[BUFFER_FILENAME_SIZE];

static uint8_t xmodem_recvFile[BUFFER_FILE_SIZE];

static bool parse_options(int argc, char* argv[]);
static bool serial_getchar(modem_context_t* pThis, uint8_t* data, uint32_t size);
static void serial_putchar(modem_context_t* pThis, uint8_t* data, uint32_t size);
int do_file_transmission(void);
int do_file_reception(void);

int main(int argc, char* argv[])
{
    bool bOk;
    int exit_code;
    exit_code = EXIT_FAILURE;

    bOk = parse_options(argc, argv);
    if (!bOk)
    {
        exit(EXIT_FAILURE);
    }

    serial_fd = serial_setup(options.device, options.speed, options.parity, options.ctrl_flow, options.nb_stop);
    if (serial_fd < 0)
    {
        fprintf(stderr, "unable to open '%s'\n", options.device);
        exit(EXIT_FAILURE);
    }

    lxmodem_opts xmodem_opts = 0;
    if (options.protocol == XMODEM)
    {
        if (options.xmodem_blksize > 0)
        {
            xmodem_opts = lxmodem_1k;
            xmodem_buffer_size = LXMODEM_1K_BUFFER_MIN_SIZE;
        }
        else if (options.crc > 0)
        {
            xmodem_opts = lxmodem_128_with_crc;
            xmodem_buffer_size = LXMODEM_128_CRC_BUFFER_MIN_SIZE;
        }
        else
        {
            xmodem_opts = lxmodem_128_with_chksum;
            xmodem_buffer_size = LXMODEM_128_CHKSUM_BUFFER_MIN_SIZE;
        }
    }
    else if (options.protocol == YMODEM)
    {
        xmodem_buffer_size = LXMODEM_1K_BUFFER_MIN_SIZE;
    }

    lmodem_init(&xmodem_ctx, xmodem_opts);
    xmodem_buffer = malloc(xmodem_buffer_size);
    assert(xmodem_buffer != NULL);

    bOk = lmodem_set_line_buffer(&xmodem_ctx, xmodem_buffer, xmodem_buffer_size);
    if (!bOk)
    {
        fprintf(stdout, "internal error\n");
        free(xmodem_buffer);
        serial_close(serial_fd);
        exit(EXIT_FAILURE);
    }

    lmodem_set_filename_buffer(&xmodem_ctx, xmodem_filename_buffer, BUFFER_FILENAME_SIZE);
    lmodem_set_getchar_cb(&xmodem_ctx, serial_getchar);
    lmodem_set_putchar_cb(&xmodem_ctx, serial_putchar);

    if (options.rx)
    {
        exit_code = do_file_reception();
    }

    if (options.tx)
    {
        exit_code = do_file_transmission();
    }

    serial_close(serial_fd);
    free(xmodem_buffer);
    return exit_code;
}

static bool parse_options(int argc, char* argv[])
{
    bool bOk;
    int opt_index;
    OPTS c;
    bOk = false;

    memset(&options, 0, sizeof(options_t));

    while (1)
    {
        c = getopt_long(argc, argv, "", long_options, &opt_index);
        if ((int32_t) c == -1)
        {
            break;
        }

        switch (c)
        {
            case OPTS_PROTOCOL:
                options.protocol = strtoul(optarg, NULL, 0);
                break;

            case OPTS_DEVICE:
                options.device = optarg;
                break;

            case OPTS_SPEED:
                options.speed = strtoul(optarg, NULL, 0);
                break;

            case OPTS_PARITY:
                options.parity = strtoul(optarg, NULL, 0);
                break;

            case OPTS_CTRL_FLOW:
                options.ctrl_flow = strtoul(optarg, NULL, 0);
                break;

            case OPTS_NB_STOP:
                options.nb_stop = strtoul(optarg, NULL, 0);
                break;

            case OPTS_CRC:
                options.crc = 1;
                break;

            case OPTS_1K:
                options.xmodem_blksize = 1;
                break;

            case OPTS_TX:
                options.tx = 1;
                break;

            case OPTS_RX:
                options.rx = 1;
                break;

            case OPTS_FILE:
                options.filename = optarg;
                break;

            case OPTS_UNKNOWN:
                fprintf(stdout, "unknow options\n");
                exit(EXIT_FAILURE);
                break;

            default:
                exit(EXIT_FAILURE);
                break;
        }
    }

    fprintf(stdout, "%s executed with following options:\n", argv[0]);
    if (options.protocol == XMODEM)
    {
        fprintf(stdout, "protocol: xmodem\n");
        bOk = true;
    }
    else if (options.protocol == YMODEM)
    {
        fprintf(stdout, "protocol: ymodem\n");
        bOk = true;
    }
    else
    {
        fprintf(stdout, "protocol: unknown\n");
        bOk = false;
    }

    if (bOk)
    {
        if (options.device == NULL)
        {
            fprintf(stdout, "device: unknown\n");
            bOk = false;
        }
        else
        {
            fprintf(stdout, "device: %s\n", options.device);
            bOk = true;
        }
    }

    if (bOk)
    {
        fprintf(stdout, "  speed: %d, parity: %d, ctr_flow: %d, nb_stop: %d\n",
                options.speed, options.parity, options.ctrl_flow, options.nb_stop);
    }

    if (bOk)
    {
        fprintf(stdout, "xmodem_1k: %d\n", options.xmodem_blksize);
    }

    if (bOk)
    {
        fprintf(stdout, "xmodem_crc: %d\n", options.crc);
    }

    if (bOk && (options.tx ^ options.rx) == 0)
    {
        fprintf(stdout, "only one of tx or rx options must be set\n");
        bOk = false;
    }
    else
    {
        fprintf(stdout, "rx: %d\n", options.rx);
        fprintf(stdout, "tx: %d\n", options.tx);
    }

    if (bOk)
    {
        if (options.filename == NULL)
        {
            fprintf(stdout, "a filename must be specified\n");
            bOk = false;
        }
        else
        {
            fprintf(stdout, "file: %s\n", options.filename);
        }
    }

    return bOk;
}

bool serial_getchar(modem_context_t* pThis, uint8_t* data, uint32_t size)
{
    (void) pThis;
    return serial_read(serial_fd, data, size);;
}

void serial_putchar(modem_context_t* pThis, uint8_t* data, uint32_t size)
{
    (void) pThis;
    serial_write(serial_fd, data, size);
}

int do_file_transmission(void)
{
    int exit_code;
    exit_code = EXIT_FAILURE;
    struct stat fileStat;
    int r;
    r = stat(options.filename, &fileStat);
    if (r == 0)
    {
        fprintf(stdout, "file size = %ld\n", fileStat.st_size);
        FILE* f = fopen(options.filename, "r");
        if (f != NULL)
        {
            uint8_t* datafile = malloc(fileStat.st_size);
            uint32_t nbRead;
            int32_t nbBytesEmitted;
            bool b;
            nbRead = fread(datafile, fileStat.st_size, 1, f);
            fprintf(stdout, "nbBlockRead = %d\n", nbRead);

            lmodem_set_file_buffer(&xmodem_ctx, datafile, fileStat.st_size);
            b = lmodem_buffer_set_write_offset(&xmodem_ctx.ramfile, fileStat.st_size);
            assert(b == true);

            nbBytesEmitted = lmodem_emit(&xmodem_ctx, options.protocol);
            fprintf(stdout, "nbBytesEmitted = %d\n", nbBytesEmitted);
            free(datafile);
            if (nbBytesEmitted >= 0)
            {
                exit_code = EXIT_SUCCESS;
            }
        }
    }
    else
    {
        fprintf(stdout, "unable to open file '%s'\n", options.filename);
    }
    return exit_code;
}


int do_file_reception()
{
    int exit_code;
    int32_t nbBytesReceived;
    exit_code = EXIT_FAILURE;
    lmodem_set_file_buffer(&xmodem_ctx, xmodem_recvFile, BUFFER_FILE_SIZE);

    FILE* f = fopen(options.filename, "w");
    if (f != NULL)
    {
        nbBytesReceived = lmodem_receive(&xmodem_ctx, options.protocol);
        fprintf(stdout, "> %d bytes received\n", nbBytesReceived);
        if (nbBytesReceived >= 0)
        {
            fwrite(xmodem_ctx.ramfile.buffer, 1, xmodem_ctx.ramfile.write_offset, f);
            exit_code = EXIT_SUCCESS;
        }
        fclose(f);
    }
    else
    {
        fprintf(stdout, "unable to open file '%s'\n", options.filename);
    }
    return exit_code;
}
