#include <IRremoteESP8266.h>
#include <ir_Goodweather.h>
#include "painlessMesh.h"
#include <Arduino.h>
#include <IRsend.h>
#include "FS.h"

const uint16_t kIrLed = D5;  // The ESP8266 GPIO pin the IR LED is connected to.
IRsend irsend(kIrLed);       // Create an instance of the IRsend class.
IRGoodweatherAc ac(kIrLed);  // Create an instance of the IRGoodweatherAc class.

#define MESH_PREFIX         "whateverYouLike"
#define MESH_PASSWORD       "somethingSneaky"
#define MESH_PORT           5555
#define LED_PIN             2 //                    | this is the Built in LED on NodeMCU //// MAYBE ILL REMOVE THIS ONE... ////
#define RELAY_PIN           4 // D2 on NodeMCU      | this is connected to the relay trigger
#define OUTPUT_TOGGLE_PIN   5 // D1 on NodeMCU  <═╗ | this is connected to D4 as output
                              // Light switch  <══/ | the light switch breaks the connection between the two pins
#define INPUT_TOGGLE_PIN    2 // D4 on NodeMCU  <═╝ | this is connected to D1 as input

#define ROOT_NODE_ID        3042073076 //           | this is the root nodes ID (chip id)

enum State {
    A, B, C, D
};

painlessMesh  mesh;

State currentState = A;
bool lightStateFromApp = false;
bool previousLedState = false;
int previousInputState = LOW;
bool lightFlag = false;
String nodeName = "unset";

void receivedCallback(uint32_t from, String &msg);
void setNodeName(const String& msg);

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN,            OUTPUT);
    pinMode(RELAY_PIN,          OUTPUT);
    pinMode(OUTPUT_TOGGLE_PIN,  OUTPUT);
    pinMode(INPUT_TOGGLE_PIN,   INPUT );

    mesh.setDebugMsgTypes( STARTUP | MESH_STATUS | ERROR );
    // SET NODE CREDENTIALS, OPERATING MODE AND CHANNEL  v------v SUPER IMPORTANT!!
    mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_STA, 1 );
    mesh.onReceive(&receivedCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    initFS();
    irsend.begin();
    ac.begin();
}

void printState(State state, bool ledState, int inputState) {
    switch (state) {
        case A: Serial.println("State A"); break;
        case B: Serial.println("State B"); break;
        case C: Serial.println("State C"); break;
        case D: Serial.println("State D"); break;
    }
    Serial.print("ledState: ");
    Serial.println(ledState ? "HIGH" : "LOW");
    Serial.print("INPUT_TOGGLE_PIN: ");
    Serial.println(inputState == HIGH ? "HIGH" : "LOW");
}

void toggleLightFlag(bool* flag) {
    *flag = !(*flag);
}

void loop() {
    int inputState = digitalRead(INPUT_TOGGLE_PIN);

    switch (currentState) {
        case A:
            if (lightStateFromApp != previousLedState && lightStateFromApp == true) {
                currentState = B;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            } else if (inputState != previousInputState && inputState == HIGH) {
                currentState = C;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            }
            break;
        case B:
            if (lightStateFromApp != previousLedState && lightStateFromApp == false) {
                currentState = A;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            } else if (inputState != previousInputState && inputState == HIGH) {
                currentState = D;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            }
            break;
        case C:
            if (inputState != previousInputState && inputState == LOW) {
                currentState = A;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            } else if (lightStateFromApp != previousLedState && lightStateFromApp == true) {
                currentState = D;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            }
            break;
        case D:
            if (lightStateFromApp != previousLedState && lightStateFromApp == false) {
                currentState = C;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            } else if (inputState != previousInputState && inputState == LOW) {
                currentState = B;
                toggleLightFlag(&lightFlag);
                printState(currentState, lightStateFromApp, inputState);
            }
            break;
    }
    previousLedState = lightStateFromApp;
    previousInputState = inputState;

    digitalWrite(LED_PIN, lightFlag ? LOW : HIGH); // <-- disabled the built in led toggle
    digitalWrite(RELAY_PIN, lightFlag ? HIGH : LOW);

    mesh.update();
}

void initFS(){
    if (!SPIFFS.begin()) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    // Check if the file exists, if not, create it
    if (!SPIFFS.exists("/devName.txt")) {
        File file = SPIFFS.open("/devName.txt", "w");
        if (file) {
            file.println("unset"); // Default name
            file.close();
        } else
            Serial.println("Failed to create file");
    }

    // Read the existing node name from file
    File file = SPIFFS.open("/devName.txt", "r");
    if (file) {
        nodeName = file.readStringUntil('\n');
        file.close();
    } else
        Serial.println("Failed to open file for reading");
}

void setNodeName(const String& msg) {
    String newName = msg.substring(String("set_name:").length());
    nodeName = newName;
    Serial.println("Node name set to: " + nodeName);

    // Write the new name to file
    File file = SPIFFS.open("/devName.txt", "w");
    if (file) {
        file.println(nodeName);
        file.close();
    } else {
        Serial.println("Failed to open file for writing");
    }
}

void toggleAC() {
  if (ac.getPower())
    ac.off();
  else
    ac.on();
  ac.send();
}

// Function to increase the temperature
void tempUp() {
  uint8_t temp = ac.getTemp();
  if (temp < 31) {
    ac.setTemp(temp + 1);
  }
  ac.send();
}

// Function to decrease the temperature
void tempDown() {
  uint8_t temp = ac.getTemp();
  if (temp > 16) {
    ac.setTemp(temp - 1);
  }
  ac.send();
}

void receivedCallback(uint32_t from, String &msg) {
    String response = "fail";
    Serial.println("Received a message");

    if (msg.equals("turn_on")) {
        Serial.println("Received 'turn on' command.");
        lightStateFromApp = true;
        response = "success";

    } else if (msg.equals("turn_off")) {
        Serial.println("Received 'turn off' command.");
        lightStateFromApp = false;
        response = "success";

    } else if (msg.equals("toggle_light")) {
        Serial.println("Received 'toggle_light' command.");
        toggleLightFlag(&lightStateFromApp);
        response = "success";

    } else if (msg.equals("toggle_ac")) {
        Serial.println("Received 'toggle_ac' command.");
        toggleAC();
        response = "success";

    } else if (msg.equals("AC_temp_up")) {
        Serial.println("Received 'toggle_ac' command.");
        tempUp();
        response = "success";

    } else if (msg.equals("AC_temp_down")) {
        Serial.println("Received 'toggle_ac' command.");
        tempDown();
        response = "success";

    } else if (msg.equals("get_name")) {
        Serial.println("Received 'get_name' command.");
        response = "nodeName:" + nodeName;

    } else if (msg.startsWith("set_name:")) {
        setNodeName(msg);
        response = "Node name is set to: " + nodeName;

    } else{
        Serial.printf("Unknown message: %s\n", msg.c_str());
        response = "Unknown message...";
    }

    if (mesh.sendSingle(from, response))
        Serial.println("Response sent: " + response);
}

