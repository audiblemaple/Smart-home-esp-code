#include "painlessMesh.h"

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

void t4Callback();


painlessMesh  mesh;

void sendMessage(String msg) {
    mesh.sendBroadcast(msg);
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
    Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void setup() {
    // Initialize GPIO 10 as an input
    pinMode(5, INPUT);
    pinMode(2, OUTPUT);


    Serial.begin(115200);

    Serial.printf("my nodeID: %u\n", mesh.getNodeId());

    // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
    // mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
    mesh.setDebugMsgTypes(MESH_STATUS);  
    mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

void loop() {
        int state = digitalRead(5);
    if (state == HIGH) {
        Serial.println("GPIO 5 is HIGH");
        sendMessage("turn_on");
        digitalWrite(2, HIGH);
    } else {
        Serial.println("GPIO 5 is LOW");
        sendMessage("turn_off");
        digitalWrite(2, LOW);
    }
    delay(1000);
    mesh.update();
}
