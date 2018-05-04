#include "sntp_utils.h"
#include "Arduino.h"
#include <RTClock.h>
#include <EEPROM.h>

/* Shifts usecs in unixToNtpTime */
#ifndef USECSHIFT
#define USECSHIFT (1LL << 32) * 1.0e-6
#endif


RTClock rtclock (RTCSEL_LSE); // initialise
int timezone = 8;      // change to your timezone
//rtclock.setTime(1508417425);

static char lat_s = '0',lon_s = '0',qual = '0',state = '0',command = '0';
String lat,lon,date_GPS,sats,time_GPS;
boolean renew = false;
uint32_t unixTime_last_sync = 0 ,Time_last_sync = 0;

/* SNTP Packet array */
char serverPacket[PACKETSIZE] = {0};
char echoPacket[10] = {0xF};

/* port to listen to sntp messages */
static char srcPort = 123;

uint32_t micros_recv = 0;
uint32_t timeStamp;
extern uint32_t micros_offset, uptime_sec_count;
extern time_t t;
extern ModbusIP mb;
extern int mb_begin_offset;
uint32_t micros_transmit = 0;

void ntp_recv_and_respond(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len)
{
   if(48==len)
   {
   ether.ntpProcessAnswer(&timeStamp, srcPort);
   Serial.println("\n=====NTP request received=====");
   Serial.println("Packet length: "+ String(len));
   micros_recv = (((micros() - micros_offset)%1000000) + 1) * USECSHIFT;
   uint32_t recvTime = htonl(rtclock.getTime() + STARTOFTIME);
   uint32_t transmitTime = 0;

   Serial.print("Timestamp: ");
   Serial.println(timeStamp);
   Serial.print("UNIX time: ");
   Serial.println(timeStamp-STARTOFTIME);
   Serial.print("src_ip: ");
   Serial.print(src_ip[0]);
   Serial.print('.');
   Serial.print(src_ip[1]);
   Serial.print('.');
   Serial.print(src_ip[2]);
   Serial.print('.');
   Serial.println(src_ip[3]);
   Serial.print("dest_port: ");
   Serial.println(dest_port);
   Serial.print("src_port: ");
   Serial.println(src_port);

   serverPacket[0] = 0x24;   // LI, Version, Mode // Set version number and mode
   serverPacket[1] = 1; // Stratum, or type of clock
   serverPacket[2] = 0;     // Polling Interval
   serverPacket[3] = -12;  // Peer Clock Precision
   serverPacket[12] = 'G';
   serverPacket[13] = 'P';
   serverPacket[14] = 'S';

   Serial.print("NOW IS: ");
   Serial.print(rtclock.getTime());
   Serial.println();

   memcpy(&serverPacket[16], &unixTime_last_sync, 4);// HERE I need to insert the last gps time sync event time
   micros_transmit = (((micros() - micros_offset)%1000000) + 1) * USECSHIFT;
   micros_transmit = htonl(micros_transmit);
   micros_recv = htonl(micros_recv);

   transmitTime = htonl( rtclock.getTime() + STARTOFTIME);

    memcpy(&serverPacket[40], &transmitTime, 4);
    memcpy(&serverPacket[24], &data[40], 4);
    memcpy(&serverPacket[28], &data[44], 4);
    memcpy(&serverPacket[32], &recvTime, 4);
    memcpy(&serverPacket[36], &micros_recv, 4);
    memcpy(&serverPacket[44], &micros_transmit, 4);
    ether.makeUdpReply(serverPacket,sizeof serverPacket, srcPort);
  }
}

String readValue(HardwareSerial *serial_pointer)
{
   String buffer;
   uint32_t micr = micros();
   while(1)
   {
      if(micros() - micr > 100000) return buffer;
      int b = serial_pointer->read();
      if((char)b == 'V') return "";

      if ( (('0' <= (char)b) &&
      ((char)b <= '9')) || ((char)b == '.')
      || (('A' <= (char)b) &&
      ((char)b <= 'Z')) )
      {
         /* read all values containing numbers letters and dots */
         buffer += (char)b;
      }
      /*stop reading when encounter a comma */
      if ( (char)b == ',' ) return buffer;
   }
}


void processGNRMC(HardwareSerial *serial_pointer)
{
   if(serial_pointer->available() > 0)
   {
      int b = serial_pointer->read();
      /* swtich states for state machine */
      switch ( state )
      {
         case '0': if('$' == b) state = '1'; else state = '0'; break;

         case '1': if('G' == b) state = '2'; else state = '0'; break;

         case '2': if(('P' == b)||('N' == b)) state = '3'; else state = '0'; break;

         case '3': if('R' == b) state = '4'; else state = '0'; break;

         case '4': if('M' == b) state = '5'; else state = '0'; break;

         case '5': if('C' == b) state = '6'; else state = '0'; break;

         /* found $GPRMC/GNRMC statement */
         case '6':

         if(',' == b)
         {
            time_GPS = readValue(serial_pointer);
            lat = readValue(serial_pointer);
            lat.replace('.',',');
            lat_s = (char)readValue(serial_pointer)[0];
            lon = readValue(serial_pointer);
            lon.replace('.',',');
            lon_s = (char)readValue(serial_pointer)[0];
            qual = (char)readValue(serial_pointer)[0];
            sats = readValue(serial_pointer);
            readValue(serial_pointer); // skip empty
            date_GPS = readValue(serial_pointer);
            //Serial.print("time_GPS = " + time_GPS + " lat = " + lat + " lat_s =" + lat_s + " lon = " + lon + " lon_s = " + lon_s  + " qual = " +qual + " sats = " + sats + " date_GPS = " + date_GPS + "\r\n");
            renew = true;
            state = '0';
         }
         else state = '0'; break;

         default: state = '0'; break;
      }

/* if found flag A, we sync every 300 seconds todo : setup a stack with last 5 values from gps */
      if (renew&&lat[0]=='A'&&(rtclock.getTime()-Time_last_sync )> 300)
      {
         int hours = 0,minutes = 0,seconds = 0,days = 0,months = 0,years = 0;
         hours += (time_GPS[0]-'0')*10;
         hours+= time_GPS[1]-'0';
         minutes+=(time_GPS[2]-'0')*10;
         minutes+=time_GPS[3]-'0';
         seconds+=(time_GPS[4]-'0')*10;
         seconds+=time_GPS[5]-'0';

         days+=(date_GPS[0]-'0')*10;
         days+= date_GPS[1]-'0';
         months+=(date_GPS[2]-'0')*10;
         months+=date_GPS[3]-'0';
         years+=(date_GPS[4]-'0')*10;
         years+=date_GPS[5]-'0';

         /* set internal time with time received from gps */
         set_Internal_time_GPS(hours,minutes,seconds,days,months, years);

         Serial.print("time_GPS = " + time_GPS + " lat = " + lat + " lat_s =" +
                      lat_s + " lon = " + lon + " lon_s = " + lon_s  + " qual = "+
                       qual + " sats = " + sats + " date_GPS = " + date_GPS + "\r\n");

         renew = false;
         unixTime_last_sync = htonl(rtclock.getTime()+ STARTOFTIME);
         Time_last_sync = rtclock.getTime();
         micros_offset = (micros())%1000000;

         /* set marker A in 0 register of modbus, means that time is ready for reading */
         mb.Hreg(mb_begin_offset + 0,'A');
      }
      /* set marker V in 0 register of modbus, - time is NOT valid */
      else if((rtclock.getTime()-Time_last_sync )> 300) mb.Hreg(mb_begin_offset + 0,'V');
   }
}

void set_Internal_time_GPS(uint8_t hr,uint8_t min,uint8_t sec,uint8_t day,uint8_t mnth, int yr)
{
   tm_t gps_time = { (uint8_t)(yr-1970), mnth, day, 0, 0, hr, min, sec };
   rtclock.setTime(gps_time);
}

void fill_modbus_time()
{
   uint32_t t = rtclock.getTime();
   mb.Hreg(mb_begin_offset  + 1,*((int16_t*)&t));  // Fills the unix seconds in modbus
   mb.Hreg(mb_begin_offset  + 2,*((int16_t*)&t+1));  // Fills the unix seconds in modbus
   mb.Hreg(mb_begin_offset  + 3,rtclock.hour());  // Returns the hour for the given
   mb.Hreg(mb_begin_offset  + 4,rtclock.minute()); // Returns the minute for the given
   mb.Hreg(mb_begin_offset  + 5,rtclock.second()); // Returns the second for the given
   mb.Hreg(mb_begin_offset  + 6,rtclock.day()); // The day for the given time_GPS t
   /*weekday(t); // Day of the week for the given NO week day is needed at the moment */
   mb.Hreg(mb_begin_offset  + 7,rtclock.month()); // The month for the given time_GPS t
   mb.Hreg(mb_begin_offset  + 8,rtclock.year());// The year for the given time_GPS t

   mb.Hreg(mb_begin_offset  + 10,*((int16_t*)&uptime_sec_count));  // Fills the UPTIME unix seconds in modbus
   mb.Hreg(mb_begin_offset  + 11,*((int16_t*)&uptime_sec_count+1));
}

