#include "sntp_utils.h"
#include "commands.h"
#include <libmaple/iwdg.h>
#include <RTClock.h>

uint32_t micros_offset = 0;
uint32_t uptime_sec_count = 0;

/* ModbusIP object */
ModbusIP mb;

int mb_begin_offset = 0;
extern RTClock rtclock;

/*Default ip*/
uint8_t myip[] = { 192,168,0,125 };

uint8_t ntp_protocol = true, mbtcp_protocol = true,
        melsec_protocol = false, gps_enabled = true;

void rtc_sec_intr()
{
   if (rtc_is_second()) uptime_sec_count++;
}

void setup()
{
   Serial.begin(9600);
   rtc_attach_interrupt(RTC_SECONDS_INTERRUPT, rtc_sec_intr);
   rtclock.setTime(1508417425);
   iwdg_init(IWDG_PRE_256, 2000); // init watchdog timer
   static byte mymac[] = { 0x24,0x69,0x69,0x2D,0x30,0x1F };

   uint16_t chksum = (uint16_t)EEPROM.read(4) + (uint16_t)EEPROM.read(5)*256;

   /* if chksum is OK use ip from "eeprom" */

   if((chksum == (uint8_t)EEPROM.read(0)+(uint8_t)EEPROM.read(1)+
                 (uint8_t)EEPROM.read(2)+(uint8_t)EEPROM.read(3))
                 &&chksum>4&&chksum<1000)
   {
      uint8_t ip []= { (uint8_t)EEPROM.read(0), (uint8_t)EEPROM.read(1),
                       (uint8_t)EEPROM.read(2), (uint8_t)EEPROM.read(3) };
      memcpy(myip, ip, 4);
   }

   /* add empty modbus registers into modbus object */
   for(int i = 0; i < 15; i++)
   {
      mb.addHreg(i,0);
   }

   mb.config(mymac, myip);
   ether.enableBroadcast();
   micros_offset = micros()%1000000;

   Serial.begin(9600);
   Serial1.begin(9600);  //starts serial for NMEA receiving

   ether.udpServerListenOnPort(&ntp_recv_and_respond, 123);
   ether.udpServerListenOnPort(&echo_recv_and_respond, 4800);
   ether.udpServerListenOnPort(&udp_listen_commands,7800);
}

void loop()
{
   mb.task();
   processGNRMC(&Serial1);
   check_for_serial_commands((HardwareSerial*)&Serial);
   fill_modbus_time();
   iwdg_feed(); //feed watchdog
}
