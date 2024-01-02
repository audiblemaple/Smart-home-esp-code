// TODO:
//       1. Add a response queue
//          when i send a command i add to the queue and when i receive a success status from the node id in the queue,
//          remove it and update the react GUI.
//          After X time with no response try again, if still error, send error code to react GUI.
//          * Add parsing for the message from the node to get the nodeId and the nodeName.
//
//       2. finish implementing the getName functionality
//
//       3. fix the log functionality using websocket

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

// Mesh object
painlessMesh mesh;

// Declare server and websocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Declare IP addresses
IPAddress myIP  (0, 0, 0, 0);
IPAddress myAPIP(0, 0, 0, 0);

// String for the log functionality
String logString = "";

// Function Prototypes
void receivedCallback(const uint32_t &from, const String &msg);
void newConnectionCallback(uint32_t nodeId);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
// void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void notifyClients(String sensorReadings);
void initWebSocket();
IPAddress getlocalIP();



void notifyClients(String sensorReadings) {
    ws.textAll(sensorReadings);
}

// void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
//     AwsFrameInfo *info = (AwsFrameInfo*)arg;
//     if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
//         String message = "Nothing to do here yet....";
//         Serial.print(message);
//         notifyClients(message);
//     }
// }



void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        // case WS_EVT_DATA:
        //     handleWebSocketMessage(arg, data, len);
        //     break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}


void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


// Setup:
//      Serial output
//      Mesh debug output levels
//      Mesh callback functions
//      Mesh wifi connection and AP connection
//      Mesh set this device as the meshe's root device and declare it in the mesh
//      Set up Server routes
//      Initialize file system
//      Initialize websocket
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
    initWebSocket();

    myAPIP = IPAddress(mesh.getAPIP());
    Serial.println("My AP IP is " + myAPIP.toString());

    SPIFFS.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasArg("username") || !request->hasArg("password")) {
            addToLog("No login credentials provided");
            request->send(SPIFFS, "/mesh_login.html", "text/html");
            return;
        }

        String username = request->arg("username");
        String password = request->arg("password");

        if (username == admin_username && password == admin_password) {
            isAdminLoggedIn = true;
            lastAdminActivityTime = millis();
            addToLog("Admin logged in");
            request->redirect("/dev");
        } else {
            isAdminLoggedIn = false;
            addToLog("Incorrect login attempt");
            request->send(SPIFFS, "/mesh_login.html", "text/html");
        }
    });

    server.on("/dev", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAdminLoggedIn){
            request->redirect("/");
            addToLog("Admin logged in");
            return;
        }
        lastAdminActivityTime = millis();

        if (!request->hasArg("id") || !request->hasArg("act")) {
            Serial.println("ID and/or Action argument not received");
            request->send(SPIFFS, "/control_panel.html", "text/html");
            return;
        }

        String id = request->arg("id");
        String act = request->arg("act");

        addToLog("Received id: " + id);
        addToLog("Received act: " + act);

        if (request->hasArg("arg") && !request->arg("arg").isEmpty()) {
            String arg = request->arg("arg");
            act += ":" + arg;
            Serial.println("added arg");
        }

        if( !mesh.isConnected(id.toInt())){
            addToLog( "Node " + id + "is not connected");
            request->send(SPIFFS, "/control_panel.html", "text/html");
            return;
        }

        Serial.println("Sending: " + act + " to: " + id);

        if (mesh.sendSingle(id.toInt(), act))
            addToLog("Sent: " + act + " to: " + id);
        
        request->send(SPIFFS, "/control_panel.html", "text/html");
    });

    server.on("/comm", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasArg("id") || !request->hasArg("act")) {
            addToLog("ID and/or Action argument not received");
            request->send(404, "text/plain", "ID and/or Action argument not received");
            return;
        }

        String id = request->arg("id");
        String act = request->arg("act");
        String token = request->arg("token");

        if (token != "opriwqytopwthlsvmbnxcvmbaosigahsflkashndajzGdiulwqf") {
            request->send(401, "Node not connected");
            return;
        }

        addToLog("Received id: " + id);
        addToLog("Received act: " + act);

        if (!mesh.isConnected(id.toInt())) {
            addToLog("Check if: " + id + " is not connected");
            request->send(406, "Node not connected");
            return;
        }

        addToLog(id + " is connected");

        if (mesh.sendSingle(id.toInt(), act)) {
            addToLog("Sent: " + act + " to: " + id);
            request->send(200, "OK");
        } else {
            addToLog("Error sending command to: " + id);
            request->send(404, "Error sending command to: " + id);
        }
    });

    server.on("/getLog", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAdminLoggedIn)
            request->send(401, "text/plain", "Unauthorized");
        request->send(200, "text/plain", logString);
    });

    server.on("/getNodes", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", mesh.subConnectionJson());
    });

    server.serveStatic("/", SPIFFS, "/");

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
        
        addToLog("Admin session timed out");
    }
    ws.cleanupClients();
}

void receivedCallback(const uint32_t &from, const String &msg) {
    addToLog("Received from: " + String(from) + ", " + String(msg.c_str()));
}

IPAddress getlocalIP() {
    return IPAddress(mesh.getStationIP());
}

void newConnectionCallback(uint32_t nodeId) {
    addToLog("New node Connection, nodeId: " + nodeId);
}

void addToLog(String strToLog) {
    // logString += formatTimestamp(millis()) + strToLog + "\n";
    Serial.println(strToLog);
    if (isAdminLoggedIn) {
        // notifyClients(strToLog + "\n");
        notifyClients(strToLog);
    }
}

// void writeToLog(const String& message) {
//     File logFile = SPIFFS.open("/log.txt", "a");
//     if (logFile) {
//         logFile.println(message);
//         logFile.close();
//         Serial.println("Log updated");
//     } else {
//         Serial.println("Failed to open log file for appending");
//     }
// }


// FILE sendFileOverWebSocket(const String& path) {
//     if (SPIFFS.exists(path)) {
//         File file = SPIFFS.open(path, "r");
//         if (!file) {
//             Serial.println("Failed to open file for reading");
//             return;
//         }

//         // Read the file and send it in chunks
//         // const size_t bufferSize = 1024;  // Adjust the buffer size based on available memory
//         // char buffer[bufferSize];
//         // while (file.available()) {
//         //     size_t len = file.readBytes(buffer, bufferSize);
//         //     ws.textAll(buffer, len);  // Send a chunk of the file
//         // }

//         file.close();
//         return file;
//     } else {
//         Serial.println("File does not exist");
//     }
// }






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
