// TODO:
//       1. Add a response queue
//          when i send a command i add to the queue and when i receive a success status from the node id in the queue,
//          remove it and update the react GUI.
//          After X time with no response try again, if still error, send error code to react GUI.
//          * Add parsing for the message from the node to get the nodeId and the nodeName.
//
//       2. improve styling for root node /dev GUI.
//
//       3. finish implementing the getName functionality

#include "IPAddress.h"
#include "painlessMesh.h"
#include "FS.h"

#ifdef ESP8266
#include "Hash.h"
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

// MESH CREDENTIALS
#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555

// YOUR WI-FI credentials
#define STATION_SSID "Sharon_2.4"
#define STATION_PASSWORD "207503509"

// Mesh control panel credentials
String admin_username = "admin";
String admin_password = "admin";

#define HOSTNAME "HTTP_BRIDGE"
#define MINUTE 60 * 1000  // 60 seconds in milliseconds

bool isAdminLoggedIn = false;
unsigned long lastAdminActivityTime = 0;
const unsigned long adminTimeoutInterval = 5 * MINUTE;

painlessMesh mesh;
AsyncWebServer server(80);
IPAddress myIP(0, 0, 0, 0);
IPAddress myAPIP(0, 0, 0, 0);
String logString = "";

// Function Prototypes
void receivedCallback(const uint32_t &from, const String &msg);
void newConnectionCallback(uint32_t nodeId);
IPAddress getlocalIP();

void setup() {
    Serial.begin(115200);
    mesh.setDebugMsgTypes(ERROR | MESH_STATUS);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 1);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.stationManual(STATION_SSID, STATION_PASSWORD);
    mesh.setHostname(HOSTNAME);
    mesh.setRoot(true);
    mesh.setContainsRoot(true);

    myAPIP = IPAddress(mesh.getAPIP());
    Serial.println("My AP IP is " + myAPIP.toString());

    SPIFFS.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasArg("username") && request->hasArg("password")) {
            String username = request->arg("username");
            String password = request->arg("password");
            if (username == admin_username && password == admin_username) {
                isAdminLoggedIn = true;
                lastAdminActivityTime = millis();
                Serial.println("Admin logged in");
                addToLog("Admin logged in");
                request->redirect("/dev");
                return;
            } else {
                isAdminLoggedIn = false;
                Serial.println("Incorrect login attempt");
                addToLog("Incorrect login attempt");
            }
        } else {
            Serial.println("No login credentials provided");
            addToLog("No login credentials provided");
        }
        request->send(SPIFFS, "/mesh_login.html", "text/html");
    });

    server.on("/dev", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAdminLoggedIn)
            request->redirect("/");
            
        lastAdminActivityTime = millis();
        if (request->hasArg("id") && request->hasArg("act")) {
            String id = request->arg("id");
            String act = request->arg("act");

            // Serial.println("Received id: " + id);
            // addToLog("Received id: " + id);

            // Serial.println("Received act: " + act);
            // addToLog("Received act: " + act);

            if (request->hasArg("arg") && !request->arg("arg").isEmpty()) {
                String arg = request->arg("arg");
                act += ":" + arg;
                Serial.println("added arg");
            }

            String combinedMsg = act;

            if (mesh.sendSingle(id.toInt(), combinedMsg))
                addToLog("Sent: " + act + " to: " + id.toInt());
        } else
            Serial.println("ID and/or Action argument not received");
            request->send(SPIFFS, "/control_panel.html", "text/html");
    });

    server.on("/comm", HTTP_GET, [](AsyncWebServerRequest *request) {
        String id, act;
        if (request->hasArg("id") && request->hasArg("act")) {
            id = request->arg("id");
            act = request->arg("act");

            // addToLog("Received id: " + id);
            // addToLog("Received act: " + act);

            String combinedMsg = id + ":" + act;

            if (mesh.sendSingle(id.toInt(), act))
                addToLog("Sent: " + act + "to: " + id);

            // add await for response from node and then send ok or error
            request->send(200, "OK");
        } else{
            addToLog("ID and/or Action argument not received");
            request->send(201, "text/plain", "ID and/or Action argument not received");
        }
    });

    server.on("/getLog", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAdminLoggedIn)
            request->send(401, "text/plain", "Unauthorized");
        request->send(200, "text/plain", logString);
    });

    server.on("/getNodes", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAdminLoggedIn)
            request->send(401, "text/plain", "Unauthorized");
        request->send(200, "application/json", mesh.subConnectionJson());
    });

    server.begin();
}

void loop() {
    mesh.update();

    if (myIP != getlocalIP()) {
        myIP = getlocalIP();
        Serial.println("My IP is " + myIP.toString());
    }
    if (isAdminLoggedIn && millis() - lastAdminActivityTime > adminTimeoutInterval) {
        isAdminLoggedIn = false;
        Serial.println("Admin session timed out");
        addToLog("Admin session timed out");
    }
}

void receivedCallback(const uint32_t &from, const String &msg) {
    Serial.printf("Root node: Received from %u msg=%s\n", from, msg.c_str());
    addToLog("Root node: Received from: " + String(from) + ", " + String(msg.c_str()));
}

IPAddress getlocalIP() {
    return IPAddress(mesh.getStationIP());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("New node Connection, nodeId: %u\n", nodeId);
    addToLog("New node Connection, nodeId: " + nodeId);
}

String formatTimestamp(unsigned long millisecs) {
    unsigned long seconds = millisecs / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    char timestamp[10];
    sprintf(timestamp, "%02lu:%02lu:%02lu ", hours, minutes, seconds);
    return String(timestamp);
}

void addToLog(String strToLog) {
    logString += formatTimestamp(millis()) + strToLog + "\n";
}






// gpt improvements:
// // Declaration of the command queue
// struct Command {
//     uint32_t nodeId;
//     String command;
//     unsigned long timestamp;
// };
// std::queue<Command> commandQueue;

// void sendCommand(uint32_t nodeId, const String& command) {
//     // Add command to the queue
//     commandQueue.push({nodeId, command, millis()});
//     // Send command to the node
//     mesh.sendSingle(nodeId, command);
// }

// void checkCommandQueue() {
//     // Check and process the command queue
//     // Remove successful commands or retry failed ones
// }

// void loop() {
//     mesh.update();
//     checkCommandQueue();
//     // Rest of your loop code
// }
