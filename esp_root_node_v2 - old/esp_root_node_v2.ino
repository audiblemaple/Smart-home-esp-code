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

#define HOSTNAME "HTTP_BRIDGE"
#define MINUTE 60 * 1000  // 60 times (one second in milliseconds)

bool isAdminLoggedIn = false;
unsigned long lastAdminActivityTime = 0;
const unsigned long adminTimeoutInterval = 2 * MINUTE;

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

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String htmlResponse =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "    <style>"
      "        * { user-select: none; }"
      "        body { background: radial-gradient(circle at center, lightgrey, transparent); }"
      "        .footer, .centered-div, h1 {"
      "            position: absolute;"
      "            left: 50%;"
      "            transform: translate(-50%, -50%);"
      "        }"
      "        h1 { top: 3%; }"
      "        .footer { top: 95%; font-size: 20px; }"
      "        .centered-div {"
      "            top: 40%;"
      "            border: 2px solid black;"
      "            padding: 10px 20px 35px;"
      "            text-align: center;"
      "            border-radius: 15px;"
      "        }"
      "        input { width: auto; height: 20px; }"
      "        input:focus { outline: none; }"
      "        button { cursor: pointer; width: 60px; height: 35px; }"
      "    </style>"
      "    <title>Mesh Control panel login</title>"
      "</head>"
      "<body>"
      "    <h1>Welcome to the Mesh control panel</h1>"
      "    <form class=\"centered-div\">"
      "        <h3>Mesh login</h3>"
      "        <br>"
      "        username  <input type=\"text\" placeholder=\"username\" name=\"username\">"
      "        <br><br>"
      "        password  <input type=\"password\" placeholder=\"password\" name=\"password\">"
      "        <br><br><br><br>"
      "        <button type=\"submit\">"
      "            Login"
      "        </button>"
      "    </form>"
      "    <div class=\"footer\">"
      "        Developed by Lior Jigalo <a href='https://github.com/audiblemaple/Smart_home.git'>github.com/audiblemaple/Smart_home</a>"
      "    </div>"
      "</body>"
      "</html>";

    if (request->hasArg("username") && request->hasArg("password")) {
      String username = request->arg("username");
      String password = request->arg("password");
      if (username == "admin" && password == "admin") {
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

    request->send(200, "text/html", htmlResponse);
  });

  server.on("/dev", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAdminLoggedIn) {
      request->redirect("/");
    }
    lastAdminActivityTime = millis();
    String htmlResponse =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "    <style>"
      "        body { background: radial-gradient(circle at center, lightgrey, transparent); }"
      "        .footer, .centered-div, h1, #logArea {"
      "            position: absolute;"
      "            left: 50%;"
      "            transform: translate(-50%, -50%);"
      "        }"
      "        h1 { top: 3%; }"
      "        .footer { top: 95%; font-size: 20px; }"
      "        .centered-div {"
      "            top: 30%;"
      "            border: 2px solid black;"
      "            padding: 10px 20px 35px;"
      "            text-align: center;"
      "            border-radius: 15px;"
      "        }"
      "        #logArea {"
      "            top: 64%;"
      "            width: 60%;"
      "            height: 35%;"
      "            resize: none;"
      "            outline: none;"
      "        }"
      "        input, select { width: 150px; height: 25px; }"
      "        input:focus, select:focus { outline: none; }"
      "        button { cursor: pointer; width: 60px; height: 35px; }"
      "    </style>"
      "    <title>Mesh Control Panel</title>"
      "</head>"
      "<body>"
      "    <h1>Mesh Control Panel</h1>"
      "    <form class='centered-div'>"
      "        Command UI"
      "        <br><br>"
      "        NodeId:<select name='id'>";

    // Obtain the subConnectionJson string
    String subConnectionJson = mesh.subConnectionJson();
    Serial.println(subConnectionJson);

    // Parse the JSON string
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, subConnectionJson);

    // Extracting connected nodes information and creating dropdown options
    JsonArray subs = doc["subs"];
    if (!subs.isNull() && subs.size() > 0) {
      for (JsonObject sub : subs) {
        uint32_t nodeId = sub["nodeId"];  // Extracting nodeId
        htmlResponse += "<option value='" + String(nodeId) + "'>" + String(nodeId) + "</option>";
      }
    }
    htmlResponse +=
      "        </select><br><br>"
      "        Action:<select name='act'>"
      "            <option value='turn_on'>Turn On</option>"
      "            <option value='turn_off'>Turn Off</option>"
      "            <option value='get_name'>Get Name</option>"
      "            <option value='set_name'>Set Name</option>"
      "            <option value='toggle_light'>Toggle light</option>"
      "        </select><br><br>"
      "        Command:<input type='text' placeholder='Commands' name='arg'></input><br><br><br>"
      "        <input type='submit' value='Send'>"
      "    </form>"
      "    <textarea id='logArea' readonly></textarea>"
      "    <script>"
      "        function fetchLog() {"
      "            var xhr = new XMLHttpRequest();"
      "            xhr.onreadystatechange = function() {"
      "                if (this.readyState == 4 && this.status == 200) {"
      "                    document.getElementById('logArea').textContent = this.responseText;"
      "                }"
      "            };"
      "            xhr.open('GET', '/getLog', true);"
      "            xhr.send();"
      "        }"
      "        setInterval(fetchLog, 1000);"
      "    </script>"
      "    <div class='footer'>"
      "        Developed by Lior Jigalo <a href='https://github.com/audiblemaple/Smart_home.git'>github.com/audiblemaple/Smart_home</a>"
      "    </div>"
      "</body>"
      "</html>";


    uint32_t rootNodeId = doc["nodeId"];
    htmlResponse += "<h3>Root Node:</h3><ul>";
    htmlResponse += "<li>" + String(rootNodeId) + "</li>";
    htmlResponse += "</ul>";

    htmlResponse += "<h3>Connected Nodes:</h3><ul>";
    for (JsonObject sub : subs) {
      uint32_t nodeId = sub["nodeId"];
      htmlResponse += "<li>" + String(nodeId) + "</li>";
    }
    htmlResponse += "</ul>";

    if (request->hasArg("id") && request->hasArg("act")) {
      String id = request->arg("id");
      String act = request->arg("act");

      Serial.println("Received id: " + id);
      addToLog("Received id: " + id);

      Serial.println("Received act: " + act);
      addToLog("Received act: " + act);

      if (request->hasArg("arg") && !request->arg("arg").isEmpty()) {
        String arg = request->arg("arg");
        act += ":" + arg;
        Serial.println("added arg");
      }

      String combinedMsg = act;

      if (mesh.sendSingle(id.toInt(), combinedMsg)) {
        Serial.println("Send success");
        addToLog("Send success");
      }
    } else
      Serial.println("ID and/or Action argument not received");
    request->send(200, "text/html", htmlResponse);
  });


  server.on("/getLog", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAdminLoggedIn) {
      request->redirect("/");
    }
    request->send(200, "text/plain", logString);
  });

  server.on("/comm", HTTP_GET, [](AsyncWebServerRequest *request) {
    String id, act;
    if (request->hasArg("id") && request->hasArg("act")) {
      id = request->arg("id");
      act = request->arg("act");

      // Serial.println("Received id: " + id);
      addToLog("Received id: " + id);

      // Serial.println("Received act: " + act);
      addToLog("Received act: " + act);

      String combinedMsg = id + ":" + act;

      if (mesh.sendSingle(id.toInt(), act)) {
        // Serial.println("Send success");
        addToLog("Send success");
      }
    } else
      // Serial.println("ID and/or Action argument not received");
      addToLog("ID and/or Action argument not received");
  });
  server.begin();
}

void loop() {
  mesh.update();
  if (myIP != getlocalIP()) {
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
    addToLog("My IP is: " + myIP.toString());
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