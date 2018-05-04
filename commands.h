#include "Arduino.h"
#include "sntp_utils.h"

void check_for_serial_commands(HardwareSerial *serial_pointer);
void echo_recv_and_respond(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len);
void udp_listen_commands(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len);
void commandSetTime(const char *data);
void commandChangeIp(uint16_t ipAddress[5]);

