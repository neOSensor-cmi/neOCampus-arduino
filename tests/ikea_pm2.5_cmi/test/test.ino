#include <Arduino.h>
#include <U8x8lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include <string>


U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
uint8_t seg_index = 0;
uint16_t frameLen;
uint16_t checksum;
uint16_t calculatedChecksum;
struct DATA {
    // Atmospheric environment
    uint16_t PM_1_0;
    uint16_t PM_2_5;
    uint16_t PM_10_0;
};

uint8_t payload[12];
DATA data;


void setup() {
    // put your setup code here, to run once:
    u8x8.begin();
    u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);
    u8x8.clear();
    u8x8.print("u8x8 begin\n");
    delay(5000);
    Serial.begin(115200);   // debug link
    Serial.println(F("\n\n\n[PM_sensor] serial begin ..."));
    Serial.flush();
    u8x8.print("serial begin\n");
    delay(1000);
    Serial2.begin(9600); // debug link
    Serial.println(F("\n\n\n[PM_sensor] serial2 begin ..."));
    Serial.flush();
    u8x8.print("serial2 begin\n");

    Serial.println(F("\n\n\n[PM_sensor] u8x8 begin ..."));
    Serial.flush();
    u8x8.clear();

    //uint8_t command[] = { 0x11, 0x02, 0x0B, 0x01, 0xE1 };
    //Serial2.write(command, sizeof(command));


}

void loop() {
    // put your main code here, to run repeatedly:
    if (Serial2.available()) {
        uint8_t ch = Serial2.read();

        switch (seg_index) {
            case 0:
                if (ch != 0x16) {
                    return;
                }
                calculatedChecksum = ch;
                break;

            case 1:
                calculatedChecksum += ch;
                frameLen = ch;
                break;

            case 2:
                if (ch != 0x0B) {
                    seg_index = 0;
                    return;
                }
                calculatedChecksum += ch;
                break;

            default:
                if (seg_index == frameLen + 2) {
                    checksum = ch;

                    if (uint8_t(calculatedChecksum + checksum) == (uint8_t) 0) {
                        //_status = STATUS_OK;

                        // Atmospheric Environment.
                        data.PM_1_0 = makeWord(payload[6], payload[7]);
                        data.PM_2_5 = makeWord(payload[2], payload[3]);
                        data.PM_10_0 = makeWord(payload[10], payload[11]);

                        Serial.println(data.PM_2_5);
                        Serial.flush();
                        String s = String(data.PM_2_5);
                        u8x8.clear();
                        u8x8.print(s);
                    }

                    seg_index = 0;
                    return;
                } else {
                    calculatedChecksum += ch;
                    uint8_t payloadIndex = seg_index - 3;

                    // Payload is common to all sensors
                    if (payloadIndex < sizeof(payload)) {
                        payload[payloadIndex] = ch;
                    }
                }

                break;
        }

        seg_index++;
    }
}
