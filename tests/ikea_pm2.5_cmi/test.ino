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
    delay(5000);
    Serial.begin(115200);   // debug link
    Serial.println(F("\n\n\n[PM_sensor] serial begin ..."));
    Serial.flush();
    delay(1000);
    Serial2.begin(9600); // debug link
    Serial.println(F("\n\n\n[PM_sensor] serial2 begin ..."));
    Serial.flush();

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
