#ifndef __SERIAL_IO_H__
#define __SERIAL_IO_H__

#include <stdint.h>

/* serial i/o definitions */
#define SERIAL_TIMEOUT  -1

/* serial i/o routines */
int serial_init(const char *port, unsigned long baud);
void serial_done(void);
int tx(uint8_t* buff, int n);
int rx(uint8_t* buff, int n);
int rx_timeout(uint8_t* buff, int n, int timeout);
void hwreset(void);

/* console i/o routines */
int console_kbhit(void);
int console_getch(void);
void console_putch(int ch);

/* miscellaneous functions */
void msleep(int ms);

#endif
