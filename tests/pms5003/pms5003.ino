/* Simple PMS5003 test
 * https://github.com/fu-hsi/PMS
 */

#include "Arduino.h"
#include "Stream.h"

//---------- Defines -----------------------------------------------------------



class PMS
{
public:
  static const uint16_t SINGLE_RESPONSE_TIME = 1000;
  static const uint16_t TOTAL_RESPONSE_TIME = 1000 * 10;
  static const uint16_t STEADY_RESPONSE_TIME = 1000 * 30;

  static const uint16_t BAUD_RATE = 9600;

  struct DATA {
    // Standard Particles, CF=1
    uint16_t PM_SP_UG_1_0;
    uint16_t PM_SP_UG_2_5;
    uint16_t PM_SP_UG_10_0;

    // Atmospheric environment
    uint16_t PM_AE_UG_1_0;
    uint16_t PM_AE_UG_2_5;
    uint16_t PM_AE_UG_10_0;
  };

  PMS(Stream&);
  void sleep();
  void wakeUp();
  void activeMode();
  void passiveMode();

  void requestRead();
  bool read(DATA& data);
  bool readUntil(DATA& data, uint16_t timeout = SINGLE_RESPONSE_TIME);

private:
  enum STATUS { STATUS_WAITING, STATUS_OK };
  enum MODE { MODE_ACTIVE, MODE_PASSIVE };

  uint8_t _payload[12];
  Stream* _stream;
  DATA* _data;
  STATUS _status;
  MODE _mode = MODE_ACTIVE;

  uint8_t _index = 0;
  uint16_t _frameLen;
  uint16_t _checksum;
  uint16_t _calculatedChecksum;

  void loop();
};


PMS::PMS(Stream& stream)
{
  this->_stream = &stream;
}

// Standby mode. For low power consumption and prolong the life of the sensor.
void PMS::sleep()
{
  uint8_t command[] = { 0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73 };
  _stream->write(command, sizeof(command));
}

// Operating mode. Stable data should be got at least 30 seconds after the sensor wakeup from the sleep mode because of the fan's performance.
void PMS::wakeUp()
{
  uint8_t command[] = { 0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74 };
  _stream->write(command, sizeof(command));
}

// Active mode. Default mode after power up. In this mode sensor would send serial data to the host automatically.
void PMS::activeMode()
{
  uint8_t command[] = { 0x42, 0x4D, 0xE1, 0x00, 0x01, 0x01, 0x71 };
  _stream->write(command, sizeof(command));
  _mode = MODE_ACTIVE;
}

// Passive mode. In this mode sensor would send serial data to the host only for request.
void PMS::passiveMode()
{
  uint8_t command[] = { 0x42, 0x4D, 0xE1, 0x00, 0x00, 0x01, 0x70 };
  _stream->write(command, sizeof(command));
  _mode = MODE_PASSIVE;
}

// Request read in Passive Mode.
void PMS::requestRead()
{
  if (_mode == MODE_PASSIVE)
  {
    uint8_t command[] = { 0x42, 0x4D, 0xE2, 0x00, 0x00, 0x01, 0x71 };
    _stream->write(command, sizeof(command));
  }
}

// Non-blocking function for parse response.
bool PMS::read(DATA& data)
{
  _data = &data;
  loop();
  
  return _status == STATUS_OK;
}

// Blocking function for parse response. Default timeout is 1s.
bool PMS::readUntil(DATA& data, uint16_t timeout)
{
  _data = &data;
  uint32_t start = millis();
  do
  {
    loop();
    if (_status == STATUS_OK) break;
  } while (millis() - start < timeout);

  return _status == STATUS_OK;
}

void PMS::loop()
{
  _status = STATUS_WAITING;
  if (_stream->available())
  {
    uint8_t ch = _stream->read();

    switch (_index)
    {
    case 0:
      if (ch != 0x42)
      {
        return;
      }
      _calculatedChecksum = ch;
      break;

    case 1:
      if (ch != 0x4D)
      {
        _index = 0;
        return;
      }
      _calculatedChecksum += ch;
      break;

    case 2:
      _calculatedChecksum += ch;
      _frameLen = ch << 8;
      break;

    case 3:
      _frameLen |= ch;
      // Unsupported sensor, different frame length, transmission error e.t.c.
      if (_frameLen != 2 * 9 + 2 && _frameLen != 2 * 13 + 2)
      {
        _index = 0;
        return;
      }
      _calculatedChecksum += ch;
      break;

    default:
      if (_index == _frameLen + 2)
      {
        _checksum = ch << 8;
      }
      else if (_index == _frameLen + 2 + 1)
      {
        _checksum |= ch;

        if (_calculatedChecksum == _checksum)
        {
          _status = STATUS_OK;

          // Standard Particles, CF=1.
          _data->PM_SP_UG_1_0 = makeWord(_payload[0], _payload[1]);
          _data->PM_SP_UG_2_5 = makeWord(_payload[2], _payload[3]);
          _data->PM_SP_UG_10_0 = makeWord(_payload[4], _payload[5]);

          // Atmospheric Environment.
          _data->PM_AE_UG_1_0 = makeWord(_payload[6], _payload[7]);
          _data->PM_AE_UG_2_5 = makeWord(_payload[8], _payload[9]);
          _data->PM_AE_UG_10_0 = makeWord(_payload[10], _payload[11]);
        }

        _index = 0;
        return;
      }
      else
      {
        _calculatedChecksum += ch;
        uint8_t payloadIndex = _index - 4;

        // Payload is common to all sensors (first 2x6 bytes).
        if (payloadIndex < sizeof(_payload))
        {
          _payload[payloadIndex] = ch;
        }
      }

      break;
    }

    //Serial.print("+");
    _index++;
  }
}


/* Declarations */
//#define DEBUG_SERIAL
#define PM_PASSIVE_MODE   1   // PASSIVE vs ACTIVE modes
#define PM_POWER_SAVING   1   // enable sleep() wakeUp() cycles to save power

#define PM_COOLDOWN       30  // seconds inactive betwwen two measures
#define PM_ENABLE         5   // PMS5003 has an Enable pin featuring a pullup resistor:
                              // input or output high ==> normal ops
                              // low ==> disable

/* Global variables */
PMS pms(Serial2);
PMS::DATA data;
enum class pmSensorState_t : uint8_t {
  idle    = 0,
  requestData,    // (passive mode) ask for data
  readData        // read data
};
pmSensorState_t cur_status = pmSensorState_t::idle;
unsigned long _lastActive;
const char _pattern[] = { 0x42, 0x4d, '\0' };

/*
 * SETUP
 */
void setup() {
  delay(5000);
  Serial.begin(115200);   // debug link
  Serial.println(F("\n\n\n[PMS5003] demo ..."));Serial.flush();
  delay(1000);
/*
  Stream* _pstream;
  _pstream = &Serial2;
  _pstream->begin( 9600 );
*/
  Serial.print(F("\n[PMS5003] setup Serial2"));Serial.flush();
  Serial2.begin(9600);    // PMS link
  delay(50);
  
#if defined(PM_PASSIVE_MODE)
  Serial.print(F("\n[PMS5003] switch to passive mode & empty receive buffer"));Serial.flush();
  pms.passiveMode();delay(50);
  while( Serial2.available() ) Serial2.read();  // flush input buffer
#else
  Serial.print(F("\n[PMS5003] switch to active mode"));Serial.flush();
  pms.activeMode();
#endif /* PM_PASSIVE_MODE */
  
#if not defined(PM_POWER_SAVING)
  Serial.print(F("\n[PMS5003] power saving mode disabled !! ... 30s warmup procedure ..."));Serial.flush();
  pms.wakeUp(); delay(30*1000);
#endif /* PM_POWER_SAVING */
  // enable pin is input as default
  pinMode( PM_ENABLE, INPUT );
  digitalWrite( PM_ENABLE, LOW ); // useless ... till we set it as an ouput

/* PM_ENABLE gpio
  Serial.println(F("\n[PMS5003] disabling "));Serial.flush();
  pinMode( PM_ENABLE, OUTPUT );
  while( true ) {
    Serial.print(F("."));Serial.flush();
    delay(1000);
  }
*/

/* DEBUG
 * raw print of serial messages
 */
#ifdef DEBUG_SERIAL
  delay(50);
//pms.passiveMode();delay(50);
//pms.wakeUp();delay(50);
  pms.activeMode();delay(50);
  Serial.println(F("\n[PMS5003] PMS is active ..."));Serial.flush();
  delay(5000);

  while( true ) {
    while( Serial2.available() ) {
      char msg[64];
      char _cur = Serial2.read();
      if( _cur==0x42 ) {
        Serial.print(F("\n[new frame] = "));
      }
      snprintf(msg,sizeof(msg)," 0x%02x", _cur);
      Serial.print(msg);
    }
    delay(250);
  }
#endif /* DEBUG_SERIAL */
}

/*
 * LOOP
 */
void loop() {

#ifdef PM_POWER_SAVING
  Serial.print(F("\n[PMS5003] wakeup ... now waiting 30s for stable values..."));Serial.flush();
  pms.wakeUp();delay(30*1000);
#endif /* PM_POWER_SAVING */

  Serial.println(F("\n[PMS5003] start to read data"));Serial.flush();
  // 32 bytes @ 8N1 @ 9600bauds = 33.3ms

  // undertaking 5 measures with 1250ms interleave
  uint8_t _mescount=0;
  while( _mescount < 5 ) {
#ifdef PM_PASSIVE_MODE
    pms.requestRead();
#endif /* PM_PASSIVE_MODE */
    _lastActive = millis();
    while( ! pms.readUntil(data,200) ) {
      Serial.print(F("*"));Serial.flush();
    }
    unsigned long _cur = millis();
    char msg[64];
    snprintf(msg,sizeof(msg),"[PMS5003] %lums read [PM1|PM2.5|PM10](µg/m3) %d %d %d", (_cur-_lastActive), data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0 );
    Serial.println(msg);Serial.flush();
/*
    Serial.print("\nPM 1.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_1_0);

    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5);

    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_10_0);
*/
    delay(1250);
    _mescount++;
  }

#ifdef PM_POWER_SAVING
  // end of measurement campaign
  Serial.print(F("\n[PMS5003] going down for 10s..."));Serial.flush();
  pms.sleep();delay(10*1000);
#else
  Serial.print(F("\n[PMS5003] waiting for 40s ..."));Serial.flush();
  delay(40*1000);
#endif /* PM_POWER_SAVING */
/*
  // activation ?
  if( digitalRead(PM_ENABLE)==LOW and (millis() - _lastActive) >= PM_COOLDOWN*1000 ) {
    Serial.println(F("\n[PMS5003] activation ..."));Serial.flush();
    pinMode( PM_ENABLE, INPUT );
  }
  
  // may we read ?
  if( digitalRead(PM_ENABLE)==HIGH ) {
    Serial.print(F("."));Serial.flush();
  }
  bool _res = (digitalRead(PM_ENABLE)==HIGH) && pms.read(data);
  
  if( _res ) {
    Serial.print("\nPM 1.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_1_0);
  
    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5);
  
    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_10_0);
  
    Serial.println(F("\n[PMS5003] disabling ..."));Serial.flush();
    pinMode( PM_ENABLE, OUTPUT );
    _lastActive = millis();
  }
  
  // Do other stuff...
  delay(250);
*/
}
