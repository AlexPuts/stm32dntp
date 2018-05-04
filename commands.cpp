#include "commands.h"
#include <RTClock.h>
#define SERIALNUMBER 42

extern RTClock rtclock;
extern uint32_t uptime_sec_count;
extern uint8_t myip[];
extern uint8_t ntp_protocol, mbtcp_protocol, melsec_protocol, gps_enabled;
extern uint32_t unixTime_last_sync,Time_last_sync;

/* Sample commands to change IP via serial: #chip192,168,0,121,481, #chip192,168,0,125,485, */

void check_for_serial_commands(HardwareSerial *serial_pointer)
{
   if (serial_pointer->available() > 0 )
   {
      uint16_t address1, address2, address3, address4, chksum;
      if (Serial.read() == '#' && Serial.read() == 'c'
         && Serial.read() == 'h' && Serial.read() == 'i'
         && Serial.read() == 'p')
      {
         address1 = atoi(readValue((HardwareSerial*)&Serial).c_str());
         address2 = atoi(readValue((HardwareSerial*)&Serial).c_str());
         address3 = atoi(readValue((HardwareSerial*)&Serial).c_str());
         address4 = atoi(readValue((HardwareSerial*)&Serial).c_str());
         chksum = atoi(readValue((HardwareSerial*)&Serial).c_str());
         uint16_t ipAddress[5] = {address1, address2, address3, address4, chksum};
         commandChangeIp(ipAddress);
      }
   }
}

void commandChangeIp(uint16_t ipAddress[5])
{
   if (ipAddress[0] > 255 || ipAddress[1] > 255 || ipAddress[2] > 255 || ipAddress[3] > 255 ||
       ipAddress[0] < 0   || ipAddress[1] < 0   || ipAddress[2] < 0   || ipAddress[3] < 0 )
   {
      Serial.println("Wrong IP in change ip command \n");
      return;
   }
   if ((ipAddress[0] + ipAddress[1] + ipAddress[2] + ipAddress[3] == ipAddress[4]) && ipAddress[4] != 0)
   {

      EEPROM.write(0, (uint8_t)ipAddress[0]);
      EEPROM.write(1, (uint8_t)ipAddress[1]);
      EEPROM.write(2, (uint8_t)ipAddress[2]);
      EEPROM.write(3, (uint8_t)ipAddress[3]);

      EEPROM.write(4, (uint8_t)(ipAddress[4] & 0xFF));
      EEPROM.write(5, (uint8_t)(ipAddress[4] >> 8));

      Serial.print("\nNew IP is: " +  String(ipAddress[0]) + '.' + String(ipAddress[1]) + '.' +  String(ipAddress[2]) + '.' + String(ipAddress[3]) + '\n');
      Serial.print("Rebooting \n");
      for (;;);
   }
   else
   {
      Serial.println("Invalid IP Checksum \n");
      Serial.println("Correct format example: 192,168,0,121,481, \n");
      Serial.println("192+168+0+121 = 481");
      return;
   }
return;
}

void commandSetTime(const char *data)
{
   Serial.println("\n=====SetTime request received=====");
   Serial.print("NOW IS: ");
   Serial.print(rtclock.getTime());
   Serial.println();
   uint32_t timeStamp = *((uint32_t*)data)-STARTOFTIME;
   Serial.println(timeStamp);
   Serial.print("UNIX time: ");
   rtclock.setTime(timeStamp);
}

void udp_listen_commands(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len)
{
   Serial.println(len);
   char reply[64] {0};
   char rcv_data[len];
   memcpy(rcv_data,data,len);
   for(int i = 0; i < len; i++) rcv_data[i]^=60;

   /*check if length is like settime packet */
   if(10==len)
   {
      if(rcv_data[0]=='s'&&rcv_data[1]=='t'&&rcv_data[2]=='t'
       &&rcv_data[3]=='i'&&rcv_data[4]=='m'&&rcv_data[5]=='e')
      {
         Serial.println("\n=====SetTime request received=====");
         uint32_t received_time = *((uint32_t*)(rcv_data+6));
         Serial.println(received_time);
         rtclock.setTime(received_time);
         unixTime_last_sync = htonl(rtclock.getTime()+ STARTOFTIME);
      }
   }

   /*check if length is like settings packet */
   if(14==len)
   {
      if(rcv_data[0]=='s'&&rcv_data[1]=='t'&&rcv_data[2]=='s'&&rcv_data[3]=='e'&&rcv_data[4]=='t'&&rcv_data[5]=='s')
      {
         Serial.println("\n=====Set Settings request received=====");

         ntp_protocol = *(uint8_t*)(rcv_data+6);
         mbtcp_protocol = *(uint8_t*)(rcv_data+7);
         melsec_protocol = *(uint8_t*)(rcv_data+8);
         gps_enabled = *(uint8_t*)(rcv_data+9);
         uint16_t ipAddress[5]{0};
         uint16_t sum_ip = 0;

         for(int i = 0; i < 4; i++)
         {
            ipAddress[i] = *(uint8_t*)(rcv_data+10+i);
            sum_ip+=*(uint8_t*)(rcv_data+10+i);
         }

         ipAddress[4] = sum_ip;
         commandChangeIp(ipAddress);
      }
   }
}

void echo_recv_and_respond(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len)
{
   uint8_t bcast_ip[4] = {255,255,255,255};
   char reply[64] {0};
   char module_type[15] = "stm32f103c8t6;";
   int32_t sn = SERIALNUMBER;
   uint8_t ip []= { (uint8_t)EEPROM.read(0), (uint8_t)EEPROM.read(1),
                    (uint8_t)EEPROM.read(2), (uint8_t)EEPROM.read(3) };

   memcpy(reply, module_type, 14);
   memcpy(reply + 14, ip,4);
   memcpy(reply + 19, myip, 4);
   memcpy(reply + 23, ((void*)&uptime_sec_count), 4);
   memcpy(reply + 27, &ntp_protocol,1);
   memcpy(reply + 28, &mbtcp_protocol,1);
   memcpy(reply + 29, &melsec_protocol,1);
   memcpy(reply + 30, &gps_enabled,1);
   memcpy(reply + 31, &sn,4);


   Serial.println("\n=====Echo request received=====");
   Serial.print("src_ip: ");
   Serial.print(src_ip[0]);
   Serial.print('.');
   Serial.print(src_ip[1]);
   Serial.print('.');
   Serial.print(src_ip[2]);
   Serial.print('.');
   Serial.println(src_ip[3]);
   Serial.print("dest_port:" + String(dest_port) + '\n' + "src_port: " + String(src_port));
   Serial.println("\nlength:" + String(len));
   Serial.println(data);
   Serial.println();
   ether.printIp("My IP Address:\t", ether.myip);
   ether.printIp("My Netmask:\t", ether.netmask);
   ether.printIp("My Gateway:\t", ether.gwip);
   Serial.println();
   for (int i = 0; i < 64; i++) reply[i] ^= 60;
   ether.sendUdp (reply, sizeof(reply), 7800, src_ip, 7800);
   ether.sendUdp (reply, sizeof(reply), 7800,bcast_ip, 7800);
   ether.sendUdp (reply, sizeof(reply), 7800,src_ip,src_port);
}
