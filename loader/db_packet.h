#ifndef __DB_PACKET_H__
#define __DB_PACKET_H__

#include "db_config.h"

#define PKTMAXLEN   1024

int WaitForInitialAck(void);
int SendPacket(int type, uint8_t *buf, int len);
int ReceivePacket(int *pType, uint8_t *buf, int len);
void TerminalMode(void);

#endif
