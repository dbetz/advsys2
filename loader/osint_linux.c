/**
 * @file ComPort.c - a C99 compatible com port driver
 * Copyright (c) 2009 John Steven Denson (jazzed)
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>
#include <sys/select.h>

#include "osint.h"

typedef int HANDLE;
static HANDLE hSerial;
static struct termios old_sparm;

void chk(char *fun, int sts)
{
    if (sts != 0)
        printf("%s failed\n", fun);
}

/**
 * open serial port
 * @param port - COMn port name
 * @returns hSerial - file handle to serial port
 */
int serial_init(const char* port, unsigned long baud)
{
    struct termios sparm;
    int tbaud = 0;

    hSerial = open(port, O_RDWR | O_NOCTTY);
    if(hSerial == -1) {
        printf("Invalid file handle for serial port '%s'\n",port);
        printf("Error '%s'\n",strerror(errno));
        return 0;
    }

    switch(baud) {
        case 0:
            tbaud = B115200;
            break;
        case 115200:
            tbaud = B115200;
            break;
        case 57600:
            tbaud = B57600;
            break;
        case 38400:
            tbaud = B38400;
            break;
        default:
            printf("Unsupported baudrate. Use /dev/tty*:115200, /dev/tty*:57600, or /dev/tty*:38400\n");
            serial_done();
            exit(2);
            break;
    }

    /* open the port */
    fcntl(hSerial, F_SETFL, 0);

    /* get the current options */
    chk("tcgetattr", tcgetattr(hSerial, &old_sparm));
    sparm = old_sparm;
    
    /* set raw input */
    sparm.c_cflag     |= (CS8 | CLOCAL | CREAD);
    sparm.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
    sparm.c_oflag     &= ~OPOST;
    sparm.c_iflag     = IGNPAR | IGNBRK;
    chk("cfsetispeed", cfsetispeed(&sparm, tbaud));
    chk("cfsetospeed", cfsetospeed(&sparm, tbaud));

    /* set the options */
    chk("tcsetattr", tcsetattr(hSerial, TCSANOW, &sparm));

    return hSerial;
}

/**
 * close serial port
 */
void serial_done(void)
{
#ifdef TEST
    tcsetattr(hSerial, TCSANOW, &old_sparm);
#endif
    close(hSerial);
}

/**
 * receive a buffer
 * @param buff - char pointer to buffer
 * @param n - number of bytes in buffer to read
 * @returns number of bytes read
 */
int rx(uint8_t* buff, int n)
{
    ssize_t bytes = read(hSerial, buff, n);
    if(bytes < 1) {
        printf("Error reading port: %d\n", (int)bytes);
        return 0;
    }
    return bytes;
}

/**
 * transmit a buffer
 * @param buff - char pointer to buffer
 * @param n - number of bytes in buffer to send
 * @returns zero on failure
 */
int tx(uint8_t* buff, int n)
{
    ssize_t bytes;
#if 0
    int j = 0;
    while(j < n) {
        printf("%02x ",buff[j++]);
    }
    printf("tx %d byte(s)\n",n);
#endif
    bytes = write(hSerial, buff, n);
    if(bytes != n) {
        printf("Error writing port\n");
        return 0;
    }
    return bytes;
}

/**
 * transmit a buffer and wait waitu microseconds
 * @param buff - char pointer to buffer
 * @param n - number of bytes in buffer to send
 * @returns zero on failure
 */
int txwu(uint8_t* buff, int n, int waitu)
{
    struct timeval tv;
    ssize_t bytes;
    bytes = write(hSerial, buff, n);
    if(bytes != n) {
        printf("Error writing port\n");
        return 0;
    }
    tv.tv_sec  = 0;
    tv.tv_usec = waitu;
    select(0,0,0,0,&tv);
    return bytes;
}

/**
 * receive a buffer with a timeout
 * @param buff - char pointer to buffer
 * @param n - number of bytes in buffer to read
 * @param timeout - timeout in milliseconds
 * @returns number of bytes read or SERIAL_TIMEOUT
 */
int rx_timeout(uint8_t* buff, int n, int timeout)
{
    ssize_t bytes = 0;
    struct timeval toval;
    fd_set set;

    FD_ZERO(&set);
    FD_SET(hSerial, &set);

    toval.tv_sec = timeout / 1000;
    toval.tv_usec = (timeout % 1000) * 1000;

    if (select(hSerial + 1, &set, NULL, NULL, &toval) > 0) {
        if (FD_ISSET(hSerial, &set))
            bytes = read(hSerial, buff, n);
    }

    return (int)(bytes > 0 ? bytes : SERIAL_TIMEOUT);
}

/**
 * hwreset ... resets Propeller hardware using DTR
 * @param sparm - pointer to DCB serial control struct
 * @returns void
 */
void hwreset(void)
{
    int cmd = TIOCM_DTR;
    ioctl(hSerial, TIOCMBIS, &cmd); // assert DTR pin
    msleep(25);
    ioctl(hSerial, TIOCMBIC, &cmd); // deassert DTR pin
    msleep(90);
    tcflush(hSerial, TCIFLUSH);
}

/**
 * sleep for ms milliseconds
 * @param ms - time to wait in milliseconds
 */
void msleep(int ms)
{
    volatile struct timeb t0, t1;
    do {
        ftime((struct timeb*)&t0);
        do {
            ftime((struct timeb*)&t1);
        } while (t1.millitm == t0.millitm);
    } while(ms-- > 0);
}

/* console i/o functions for Unix/Linux courtesy of 'jazzed' */

int console_kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

int console_getch(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  return ch;
}

void console_putch(int ch)
{
    putchar(ch);
}
