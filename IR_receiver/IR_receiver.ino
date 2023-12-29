#include <IRremote.h>

IRrecv IR(7);


void setup() {
    IR.enableIRIn();
    Serial.begin(9600);
    delay(2000);
    Serial.println(F("Ready to receive IR signals"));
}

void loop() {
    if(IR.decode()){
        Serial.println(IR.decodedIRData.decodedRawData, HEX);
        delay(1500);
        IR.resume();
    }
}
