#include "stdint.h"
#include "Arduino.h"
#include <EtherCard_STM.h>
#include <EEPROM.h>
#include <ModbusIP_ENC28J60_STM.h>
#define ENC28J60_CS PA8

#ifndef PACKETSIZE
#define PACKETSIZE 48
#endif

#define STARTOFTIME 2208988800UL

#ifndef UTIL_H
#define UTIL_H

#define htons(x) ( ((x)<< 8 & 0xFF00) | \
                   ((x)>> 8 & 0x00FF) )
#define ntohs(x) htons(x)

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#define ntohl(x) htonl(x)
#endif

void ntp_recv_and_respond(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len);
void set_Internal_time_GPS(uint8_t hr,uint8_t min,uint8_t sec,uint8_t day,uint8_t mnth, int yr);
String readValue(HardwareSerial *serial);
void commandChangeIp(uint16_t ipAddress[5]);
void processGNRMC(HardwareSerial *serial_pointer);
void echo_recv_and_respond(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len);
void fill_modbus_time();
