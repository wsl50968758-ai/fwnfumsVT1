#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

/*
  Target environment:
  - Arduino IDE 1.8.19
  - ESP32 Core 1.0.6

  Required libraries:
  - ArduinoJson (v6.x)

  Serial command format (newline terminated):
  ADD <scene> <r> <w> <b> <fr> <ser> <sew> <seb> <sefr>
  UPDATE <scene> <r> <w> <b> <fr> <ser> <sew> <seb> <sefr>
  DELETE <scene>
  UPDATE_ALL
  DELETE_ALL
  READ
  HELP
  SHOWEEPROM
*/

const char* WIFI_SSID = "TYNRICH";
const char* WIFI_PASSWORD = "123456";

// Replace with your deployed GAS Web App URL.
const char* GAS_WEB_APP_URL = "https://script.google.com/macros/s/YOUR_DEPLOY_ID/exec";

const uint8_t EEPROM_SCENE_CAPACITY = 10;
const uint16_t EEPROM_SIZE_BYTES = 1024;

struct SceneData {
  uint8_t scene;
  uint16_t r;
  uint16_t w;
  uint16_t b;
  uint16_t fr;
  uint8_t ser;
  uint8_t sew;
  uint8_t seb;
  uint8_t sefr;
  bool valid;
};

SceneData scenes[EEPROM_SCENE_CAPACITY];
WiFiClientSecure secureClient;

String readLineFromSerial() {
  static String input;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (input.length() > 0) {
        String line = input;
        input = "";
        return line;
      }
    } else {
      input += c;
    }
  }
  return "";
}

int sceneIndexById(uint8_t sceneId) {
  if (sceneId < 1 || sceneId > EEPROM_SCENE_CAPACITY) {
    return -1;
  }
  return sceneId - 1;
}

void saveScenesToEEPROM() {
  EEPROM.put(0, scenes);
  EEPROM.commit();
}

void loadScenesFromEEPROM() {
  EEPROM.get(0, scenes);

  // Sanitize data in case EEPROM has random bytes.
  for (int i = 0; i < EEPROM_SCENE_CAPACITY; i++) {
    scenes[i].valid = (scenes[i].valid == true);
    if (scenes[i].scene < 1 || scenes[i].scene > EEPROM_SCENE_CAPACITY) {
      scenes[i].valid = false;
      scenes[i].scene = i + 1;
      scenes[i].r = scenes[i].w = scenes[i].b = scenes[i].fr = 0;
      scenes[i].ser = scenes[i].sew = scenes[i].seb = scenes[i].sefr = 0;
    }
  }
}

void printScenesToSerial() {
  Serial.println("場景編號,r,g,b,fr,ser,seg,seb,sefr");
  for (int i = 0; i < EEPROM_SCENE_CAPACITY; i++) {
    if (!scenes[i].valid) continue;
    Serial.printf("%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                  scenes[i].scene,
                  scenes[i].r,
                  scenes[i].w,
                  scenes[i].b,
                  scenes[i].fr,
                  scenes[i].ser,
                  scenes[i].sew,
                  scenes[i].seb,
                  scenes[i].sefr);
  }
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");
  uint8_t retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi connect failed.");
  return false;
}

bool postJsonToGAS(const String& payload, String& response) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    if (!connectWiFi()) {
      return false;
    }
  }

  HTTPClient http;
  secureClient.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(secureClient, GAS_WEB_APP_URL)) {
    Serial.println("HTTP begin failed.");
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);
  if (httpCode <= 0) {
    Serial.printf("HTTP error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  response = http.getString();
  Serial.printf("HTTP %d\n", httpCode);
  http.end();
  return (httpCode == 200);
}

void clearAllScenes() {
  for (int i = 0; i < EEPROM_SCENE_CAPACITY; i++) {
    scenes[i].scene = i + 1;
    scenes[i].r = scenes[i].w = scenes[i].b = scenes[i].fr = 0;
    scenes[i].ser = scenes[i].sew = scenes[i].seb = scenes[i].sefr = 0;
    scenes[i].valid = false;
  }
}

void applyRemoteScenes(JsonArray arr) {
  clearAllScenes();

  for (JsonObject row : arr) {
    uint8_t sceneId = row["Scene"] | 0;
    int idx = sceneIndexById(sceneId);
    if (idx < 0) continue;

    scenes[idx].scene = sceneId;
    scenes[idx].r = row["r"] | 0;
    scenes[idx].w = row["w"] | row["g"] | 0;
    scenes[idx].b = row["b"] | 0;
    scenes[idx].fr = row["fr"] | 0;
    scenes[idx].ser = row["ser"] | 0;
    scenes[idx].sew = row["sew"] | row["seg"] | 0;
    scenes[idx].seb = row["seb"] | 0;
    scenes[idx].sefr = row["sefr"] | 0;
    scenes[idx].valid = true;
  }

  saveScenesToEEPROM();
}

bool fetchAllFromGAS() {
  String payload = "{\"function\":\"READ\"}";
  String response;
  if (!postJsonToGAS(payload, response)) {
    Serial.println("Fetch from GAS failed.");
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return false;
  }

  if (!(doc["ok"] | false)) {
    Serial.println("GAS returned error.");
    Serial.println(response);
    return false;
  }

  JsonArray arr = doc["data"].as<JsonArray>();
  applyRemoteScenes(arr);
  Serial.println("Loaded scenes from Google Sheet.");
  printScenesToSerial();
  return true;
}

String buildCommandJson(const String& fn,
                        const SceneData* s,
                        bool includeScene) {
  DynamicJsonDocument doc(512);
  doc["function"] = fn;
  if (includeScene && s != nullptr) {
    doc["Scene"] = s->scene;
    doc["r"] = s->r;
    doc["w"] = s->w;
    doc["b"] = s->b;
    doc["fr"] = s->fr;
    doc["ser"] = s->ser;
    doc["sew"] = s->sew;
    doc["seb"] = s->seb;
    doc["sefr"] = s->sefr;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

void syncCommandToGAS(const String& jsonPayload) {
  String response;
  if (postJsonToGAS(jsonPayload, response)) {
    Serial.println("GAS response:");
    Serial.println(response);
  } else {
    Serial.println("Sync to GAS failed.");
  }
}


void printHelp() {
  Serial.println("=== ESP32 Google Sheet Command Help ===");
  Serial.println("READ");
  Serial.println("  - Read all scenes from Google Sheet (Sheet1) and store to EEPROM.");
  Serial.println("ADD <scene> <r> <w> <b> <fr> <ser> <sew> <seb> <sefr>");
  Serial.println("  - Add single scene (Function: ADD SCENE). scene range: 1~10");
  Serial.println("UPDATE <scene> <r> <w> <b> <fr> <ser> <sew> <seb> <sefr>");
  Serial.println("  - Update single scene (Function: UPDATE). scene range: 1~10");
  Serial.println("DELETE <scene>");
  Serial.println("  - Delete single scene (Function: DELETE). scene range: 1~10");
  Serial.println("UPDATE_ALL");
  Serial.println("  - Upload all valid local EEPROM scenes to Google Sheet (Function: UPDATE ALL).");
  Serial.println("DELETE_ALL");
  Serial.println("  - Delete all local EEPROM scenes and clear Google Sheet data (Function: DELETE ALL).");
  Serial.println("HELP");
  Serial.println("  - Show this command help.");
  Serial.println("SHOWEEPROM");
  Serial.println("  - Show raw SceneData values currently stored in EEPROM.");
}


void showEEPROMRaw() {
  SceneData eepromSnapshot[EEPROM_SCENE_CAPACITY];
  EEPROM.get(0, eepromSnapshot);

  Serial.println("=== EEPROM Raw SceneData ===");
  Serial.println("idx,scene,r,w,b,fr,ser,sew,seb,sefr,valid");
  for (int i = 0; i < EEPROM_SCENE_CAPACITY; i++) {
    Serial.printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                  i,
                  eepromSnapshot[i].scene,
                  eepromSnapshot[i].r,
                  eepromSnapshot[i].w,
                  eepromSnapshot[i].b,
                  eepromSnapshot[i].fr,
                  eepromSnapshot[i].ser,
                  eepromSnapshot[i].sew,
                  eepromSnapshot[i].seb,
                  eepromSnapshot[i].sefr,
                  eepromSnapshot[i].valid ? 1 : 0);
  }
}

bool parseSceneDataFromTokens(char* token, SceneData& data) {
  long values[9];
  for (int i = 0; i < 9; i++) {
    if (token == nullptr) return false;
    values[i] = atol(token);
    token = strtok(nullptr, " ");
  }

  if (values[0] < 1 || values[0] > 10) return false;

  data.scene = (uint8_t)values[0];
  data.r = (uint16_t)values[1];
  data.w = (uint16_t)values[2];
  data.b = (uint16_t)values[3];
  data.fr = (uint16_t)values[4];
  data.ser = (uint8_t)values[5];
  data.sew = (uint8_t)values[6];
  data.seb = (uint8_t)values[7];
  data.sefr = (uint8_t)values[8];
  data.valid = true;
  return true;
}

void handleSerialCommand(const String& line) {
  char buffer[180];
  line.toCharArray(buffer, sizeof(buffer));

  char* cmd = strtok(buffer, " ");
  if (!cmd) return;

  if (strcmp(cmd, "READ") == 0) {
    fetchAllFromGAS();
    return;
  }

  if (strcmp(cmd, "HELP") == 0) {
    printHelp();
    return;
  }

  if (strcmp(cmd, "SHOWEEPROM") == 0) {
    showEEPROMRaw();
    return;
  }

  if (strcmp(cmd, "DELETE_ALL") == 0) {
    clearAllScenes();
    saveScenesToEEPROM();
    syncCommandToGAS("{\"function\":\"DELETE ALL\"}");
    printScenesToSerial();
    return;
  }

  if (strcmp(cmd, "UPDATE_ALL") == 0) {
    DynamicJsonDocument doc(4096);
    doc["function"] = "UPDATE ALL";
    JsonArray arr = doc.createNestedArray("data");

    for (int i = 0; i < EEPROM_SCENE_CAPACITY; i++) {
      if (!scenes[i].valid) continue;
      JsonObject row = arr.createNestedObject();
      row["Scene"] = scenes[i].scene;
      row["r"] = scenes[i].r;
      row["w"] = scenes[i].w;
      row["g"] = scenes[i].w;
      row["b"] = scenes[i].b;
      row["fr"] = scenes[i].fr;
      row["ser"] = scenes[i].ser;
      row["sew"] = scenes[i].sew;
      row["seg"] = scenes[i].sew;
      row["seb"] = scenes[i].seb;
      row["sefr"] = scenes[i].sefr;
    }

    String payload;
    serializeJson(doc, payload);
    syncCommandToGAS(payload);
    return;
  }

  if (strcmp(cmd, "ADD") == 0 || strcmp(cmd, "UPDATE") == 0) {
    SceneData data;
    if (!parseSceneDataFromTokens(strtok(nullptr, " "), data)) {
      Serial.println("Invalid ADD/UPDATE command args.");
      return;
    }

    int idx = sceneIndexById(data.scene);
    scenes[idx] = data;
    saveScenesToEEPROM();

    const String functionName = (strcmp(cmd, "ADD") == 0) ? "ADD SCENE" : "UPDATE";
    syncCommandToGAS(buildCommandJson(functionName, &data, true));
    printScenesToSerial();
    return;
  }

  if (strcmp(cmd, "DELETE") == 0) {
    char* sceneToken = strtok(nullptr, " ");
    if (!sceneToken) {
      Serial.println("DELETE needs scene id.");
      return;
    }

    int sceneId = atoi(sceneToken);
    int idx = sceneIndexById(sceneId);
    if (idx < 0) {
      Serial.println("Scene out of range.");
      return;
    }

    scenes[idx].scene = sceneId;
    scenes[idx].valid = false;
    scenes[idx].r = scenes[idx].w = scenes[idx].b = scenes[idx].fr = 0;
    scenes[idx].ser = scenes[idx].sew = scenes[idx].seb = scenes[idx].sefr = 0;
    saveScenesToEEPROM();

    DynamicJsonDocument doc(256);
    doc["function"] = "DELETE";
    doc["Scene"] = sceneId;
    String payload;
    serializeJson(doc, payload);
    syncCommandToGAS(payload);
    printScenesToSerial();
    return;
  }

  Serial.println("Unknown command.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  EEPROM.begin(EEPROM_SIZE_BYTES);
  loadScenesFromEEPROM();

  connectWiFi();
  fetchAllFromGAS();

  Serial.println("Ready for serial commands. Type HELP for command usage.");
}

void loop() {
  String cmd = readLineFromSerial();
  if (cmd.length() > 0) {
    handleSerialCommand(cmd);
  }
}
