#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "melodias.h"

/* ================== WIFI ================== */
const char* ssid     = "Lab";
const char* password = "12345678";
const int   fixedLastOctet = 100;

/* ================== SERVOS ================== */
Servo LeftFoot, RightFoot, LeftLeg, RightLeg;
Servo LeftArm, RightArm, Head;

const int pinLeftFoot  = 14;   // Servo 360° (rotación continua)
const int pinRightFoot = 27;   // Servo 360° (rotación continua)
const int pinLeftLeg   = 13;
const int pinRightLeg  = 12;
const int pinLeftArm   = 26;
const int pinRightArm  = 25;
const int pinHead      = 33;

/* ================== BUZZER ================== */
const int BuzzerPin = 32;
Melodias buzzer(BuzzerPin);

/* ================== SERVER Y MEMORIA ================== */
WebServer server(80);
Preferences preferences;

/* ================== CONFIGURACIÓN DE MOVIMIENTO ================== */
int tiltAngleR   = 25;    // Inclinación al inclinar a la derecha
int tiltAngleL   = 35;    // Inclinación al inclinar a la izquierda
int spinTimeR    = 130;   // Tiempo giro pie derecho (adelante)
int spinTimeL    = 170;   // Tiempo giro pie izquierdo (adelante)
int spinTimeRB   = 170;   // Tiempo giro pie derecho (atrás)
int spinTimeLB   = 190;   // Tiempo giro pie izquierdo (atrás)
int spinTimeTR   = 150;   // Tiempo giro para girar derecha
int spinTimeTL   = 150;   // Tiempo giro para girar izquierda
int tiltTime     = 500;   // Milisegundos para estabilizar la inclinación
int footSpeed    = 30;    // Velocidad del pie (1-90)
int smoothDelay  = 10;    // Delay entre cada grado de movimiento suave

const int leftLegHome  = 90;
const int rightLegHome = 70;

int currentLeftLeg  = leftLegHome;
int currentRightLeg = rightLegHome;

/* ================== OFFSETS DE CALIBRACIÓN ================== */
int offsetLeftLeg  = 0;
int offsetRightLeg = 0;

/* ================== SISTEMA DE TAREAS NO-BLOQUEANTES ================== */
unsigned long taskStartTime = 0;
bool taskRunning = false;
String currentTask = "";
int taskStep = 0;

/* ================== MELODÍAS PERSONALIZADAS ================== */
#define MAX_CUSTOM_MELODIES 5
#define MAX_NOTES_PER_MELODY 100
#define CUSTOM_MELODY_START 16

struct CustomMelody {
  char name[21];
  uint16_t noteCount;
  uint16_t notes[200]; // freq[0], dur[0], freq[1], dur[1], ... (max 100 pares)
  bool loaded;
};

CustomMelody customMelodies[MAX_CUSTOM_MELODIES];

/* ================== FUNCIONES DE MEMORIA ================== */

void loadOffsets() {
  preferences.begin("otto", false);
  offsetLeftLeg  = preferences.getInt("leftLeg", 0);
  offsetRightLeg = preferences.getInt("rightLeg", 0);
  preferences.end();

  Serial.println("=== OFFSETS CARGADOS ===");
  Serial.print("  Left Leg:  "); Serial.println(offsetLeftLeg);
  Serial.print("  Right Leg: "); Serial.println(offsetRightLeg);
}

void saveOffsets() {
  preferences.begin("otto", false);
  preferences.putInt("leftLeg", offsetLeftLeg);
  preferences.putInt("rightLeg", offsetRightLeg);
  preferences.end();

  Serial.println("=== OFFSETS GUARDADOS ===");
  Serial.print("  Left Leg:  "); Serial.println(offsetLeftLeg);
  Serial.print("  Right Leg: "); Serial.println(offsetRightLeg);
}

void resetOffsets() {
  preferences.begin("otto", false);
  preferences.clear();
  preferences.end();

  offsetLeftLeg  = 0;
  offsetRightLeg = 0;
  Serial.println("=== OFFSETS RESETEADOS ===");
}

/* ================== MELODÍAS PERSONALIZADAS - STORAGE ================== */

void loadCustomMelody(int slot) {
  int index = slot - CUSTOM_MELODY_START;
  if (index < 0 || index >= MAX_CUSTOM_MELODIES) return;

  preferences.begin("melodies", true); // solo lectura

  String nameKey = "m" + String(slot) + "_name";
  String countKey = "m" + String(slot) + "_cnt";
  String dataKey = "m" + String(slot) + "_data";

  String name = preferences.getString(nameKey.c_str(), "");
  if (name.length() == 0) {
    customMelodies[index].loaded = false;
    preferences.end();
    return;
  }

  name.toCharArray(customMelodies[index].name, 21);
  customMelodies[index].noteCount = preferences.getUShort(countKey.c_str(), 0);

  size_t dataSize = customMelodies[index].noteCount * 4; // 2 uint16_t por nota
  if (dataSize > 0 && dataSize <= sizeof(customMelodies[index].notes)) {
    preferences.getBytes(dataKey.c_str(), customMelodies[index].notes, dataSize);
  }
  customMelodies[index].loaded = true;

  preferences.end();

  Serial.print("  Melodia slot "); Serial.print(slot);
  Serial.print(": "); Serial.print(customMelodies[index].name);
  Serial.print(" ("); Serial.print(customMelodies[index].noteCount);
  Serial.println(" notas)");
}

void saveCustomMelody(int slot, const char* name, uint16_t* notes, uint16_t count) {
  int index = slot - CUSTOM_MELODY_START;
  if (index < 0 || index >= MAX_CUSTOM_MELODIES || count > MAX_NOTES_PER_MELODY) return;

  preferences.begin("melodies", false);

  String nameKey = "m" + String(slot) + "_name";
  String countKey = "m" + String(slot) + "_cnt";
  String dataKey = "m" + String(slot) + "_data";

  preferences.putString(nameKey.c_str(), name);
  preferences.putUShort(countKey.c_str(), count);
  preferences.putBytes(dataKey.c_str(), notes, count * 4);

  preferences.end();

  // Actualizar en memoria
  strncpy(customMelodies[index].name, name, 20);
  customMelodies[index].name[20] = '\0';
  customMelodies[index].noteCount = count;
  memcpy(customMelodies[index].notes, notes, count * 4);
  customMelodies[index].loaded = true;

  Serial.print("=== MELODIA GUARDADA slot ");
  Serial.print(slot); Serial.print(": ");
  Serial.print(name); Serial.print(" (");
  Serial.print(count); Serial.println(" notas) ===");
}

void deleteCustomMelody(int slot) {
  int index = slot - CUSTOM_MELODY_START;
  if (index < 0 || index >= MAX_CUSTOM_MELODIES) return;

  preferences.begin("melodies", false);
  preferences.remove(("m" + String(slot) + "_name").c_str());
  preferences.remove(("m" + String(slot) + "_cnt").c_str());
  preferences.remove(("m" + String(slot) + "_data").c_str());
  preferences.end();

  customMelodies[index].loaded = false;
  customMelodies[index].noteCount = 0;

  Serial.print("=== MELODIA ELIMINADA slot ");
  Serial.println(slot);
}

void playCustomMelody(int slot) {
  int index = slot - CUSTOM_MELODY_START;
  if (index < 0 || index >= MAX_CUSTOM_MELODIES) return;
  if (!customMelodies[index].loaded || customMelodies[index].noteCount == 0) return;

  Serial.print("Reproduciendo melodia: ");
  Serial.println(customMelodies[index].name);

  for (int i = 0; i < customMelodies[index].noteCount; i++) {
    uint16_t freq = customMelodies[index].notes[i * 2];
    uint16_t dur = customMelodies[index].notes[i * 2 + 1];

    if (freq > 0) {
      buzzer.playTone(freq, dur);
    } else {
      delay(dur);
    }
  }
}

void loadAllCustomMelodies() {
  Serial.println("=== CARGANDO MELODIAS PERSONALIZADAS ===");
  for (int i = 0; i < MAX_CUSTOM_MELODIES; i++) {
    customMelodies[i].loaded = false;
    customMelodies[i].noteCount = 0;
    loadCustomMelody(CUSTOM_MELODY_START + i);
  }
}

/**
 * Parsear string de datos de melodía: "freq,dur;freq,dur;..."
 * Retorna la cantidad de notas parseadas
 */
int parseMelodyData(String data, uint16_t* notes, int maxNotes) {
  int noteCount = 0;
  int startIdx = 0;

  while (startIdx < data.length() && noteCount < maxNotes) {
    // Buscar el separador de nota ';' o fin de string
    int endIdx = data.indexOf(';', startIdx);
    if (endIdx == -1) endIdx = data.length();

    String notePair = data.substring(startIdx, endIdx);

    // Buscar separador freq,dur
    int commaIdx = notePair.indexOf(',');
    if (commaIdx > 0) {
      uint16_t freq = notePair.substring(0, commaIdx).toInt();
      uint16_t dur = notePair.substring(commaIdx + 1).toInt();

      notes[noteCount * 2] = freq;
      notes[noteCount * 2 + 1] = dur;
      noteCount++;
    }

    startIdx = endIdx + 1;
  }

  return noteCount;
}

/* ================== CONEXIÓN WIFI HÍBRIDA ================== */

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi (DHCP)");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" OK");

  IPAddress dhcpIP      = WiFi.localIP();
  IPAddress dhcpGateway = WiFi.gatewayIP();
  IPAddress dhcpSubnet  = WiFi.subnetMask();
  IPAddress dhcpDNS     = WiFi.dnsIP();

  Serial.print("  DHCP asignó: "); Serial.println(dhcpIP);

  IPAddress fixedIP(dhcpIP[0], dhcpIP[1], dhcpIP[2], fixedLastOctet);
  Serial.print("  IP fija deseada: "); Serial.println(fixedIP);

  WiFi.disconnect();
  delay(200);

  if (!WiFi.config(fixedIP, dhcpGateway, dhcpSubnet, dhcpDNS)) {
    Serial.println("  ERROR al configurar IP estática");
  }

  WiFi.begin(ssid, password);
  Serial.print("  Reconectando con IP fija");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println(" OK");
  Serial.print("  IP final: "); Serial.println(WiFi.localIP());
}

/* ================== HELPERS DE PIERNAS ================== */

void setLegs(int leftAngle, int rightAngle) {
  currentLeftLeg  = leftAngle;
  currentRightLeg = rightAngle;
  LeftLeg.write(leftAngle + offsetLeftLeg);
  RightLeg.write(rightAngle + offsetRightLeg);
}

void smoothSetLegs(int targetLeft, int targetRight) {
  while (currentLeftLeg != targetLeft || currentRightLeg != targetRight) {
    if (currentLeftLeg < targetLeft) currentLeftLeg++;
    else if (currentLeftLeg > targetLeft) currentLeftLeg--;
    if (currentRightLeg < targetRight) currentRightLeg++;
    else if (currentRightLeg > targetRight) currentRightLeg--;
    LeftLeg.write(currentLeftLeg + offsetLeftLeg);
    RightLeg.write(currentRightLeg + offsetRightLeg);
    delay(smoothDelay);
  }
}

void tiltRight() {
  smoothSetLegs(leftLegHome - tiltAngleR, rightLegHome - tiltAngleR);
}

void tiltLeft() {
  smoothSetLegs(leftLegHome + tiltAngleL, rightLegHome + tiltAngleL);
}

void legsHome() {
  smoothSetLegs(leftLegHome, rightLegHome);
}

/* ================== HELPERS DE PIES 360° ================== */

void footForward(Servo &foot) {
  foot.write(90 - footSpeed);
}

void footBackward(Servo &foot) {
  foot.write(90 + footSpeed);
}

void footStop(Servo &foot) {
  foot.write(90);
}

/* ================== FUNCIONES BASE ================== */

void stopRoll() {
  footStop(LeftFoot);
  footStop(RightFoot);
}

void Home() {
  if(!LeftFoot.attached())  LeftFoot.attach(pinLeftFoot);
  if(!RightFoot.attached()) RightFoot.attach(pinRightFoot);
  if(!LeftLeg.attached())   LeftLeg.attach(pinLeftLeg);
  if(!RightLeg.attached())  RightLeg.attach(pinRightLeg);
  if(!LeftArm.attached())   LeftArm.attach(pinLeftArm);
  if(!RightArm.attached())  RightArm.attach(pinRightArm);
  if(!Head.attached())      Head.attach(pinHead);

  footStop(LeftFoot);
  footStop(RightFoot);
  legsHome();
  LeftArm.write(90);
  RightArm.write(90);
  Head.write(90);
}

void QuickHome() {
  if(!LeftFoot.attached())  LeftFoot.attach(pinLeftFoot);
  if(!RightFoot.attached()) RightFoot.attach(pinRightFoot);
  if(!LeftLeg.attached())   LeftLeg.attach(pinLeftLeg);
  if(!RightLeg.attached())  RightLeg.attach(pinRightLeg);
  if(!LeftArm.attached())   LeftArm.attach(pinLeftArm);
  if(!RightArm.attached())  RightArm.attach(pinRightArm);
  if(!Head.attached())      Head.attach(pinHead);

  footStop(LeftFoot);
  footStop(RightFoot);
  setLegs(leftLegHome, rightLegHome);
  currentLeftLeg  = leftLegHome;
  currentRightLeg = rightLegHome;
  LeftArm.write(90);
  RightArm.write(90);
  Head.write(90);
}

/* ================== MOVIMIENTOS INSTANTÁNEOS ================== */

void RollMode() {
  if(!LeftLeg.attached())  LeftLeg.attach(pinLeftLeg);
  if(!RightLeg.attached()) RightLeg.attach(pinRightLeg);
  setLegs(0, 180);
}

void WalkMode() {
  if(!LeftLeg.attached())  LeftLeg.attach(pinLeftLeg);
  if(!RightLeg.attached()) RightLeg.attach(pinRightLeg);
  setLegs(leftLegHome, rightLegHome);
  currentLeftLeg  = leftLegHome;
  currentRightLeg = rightLegHome;
}

void RaiseLeftArm() {
  if(!LeftArm.attached()) LeftArm.attach(pinLeftArm);
  LeftArm.write(180);
}

void RaiseRightArm() {
  if(!RightArm.attached()) RightArm.attach(pinRightArm);
  RightArm.write(0);
}

void RaiseBothArms() {
  if(!LeftArm.attached())  LeftArm.attach(pinLeftArm);
  if(!RightArm.attached()) RightArm.attach(pinRightArm);
  LeftArm.write(45);
  RightArm.write(135);
}

void LowerLeftArm() {
  if(!LeftArm.attached()) LeftArm.attach(pinLeftArm);
  LeftArm.write(0);
}

void LowerRightArm() {
  if(!RightArm.attached()) RightArm.attach(pinRightArm);
  RightArm.write(180);
}

void LowerArms() {
  if(!LeftArm.attached())  LeftArm.attach(pinLeftArm);
  if(!RightArm.attached()) RightArm.attach(pinRightArm);
  LeftArm.write(180);
  RightArm.write(0);
}

void LookLeft() {
  if(!Head.attached()) Head.attach(pinHead);
  Head.write(135);
}

void LookRight() {
  if(!Head.attached()) Head.attach(pinHead);
  Head.write(45);
}

void LookCenter() {
  if(!Head.attached()) Head.attach(pinHead);
  Head.write(90);
}

/* ================== SCHEDULER DE TAREAS ================== */

void startTask(String task) {
  if (taskRunning) {
    Serial.println("Tarea en curso, ignorando: " + task);
    return;
  }
  taskRunning   = true;
  currentTask   = task;
  taskStep      = 0;
  taskStartTime = millis();
  Serial.println("Iniciando tarea: " + task);
}

void nextStep() {
  taskStep++;
  taskStartTime = millis();
}

void endTask() {
  taskRunning = false;
  Serial.println("Tarea completada: " + currentTask);
}

void processTask() {
  if (!taskRunning) return;
  unsigned long elapsed = millis() - taskStartTime;

  if (currentTask == "walk_forward") {
    if      (taskStep == 0)                          { tiltRight();                        nextStep(); }
    else if (taskStep == 1 && elapsed >= tiltTime)   { footForward(RightFoot);             nextStep(); }
    else if (taskStep == 2 && elapsed >= spinTimeR)  { footStop(RightFoot); legsHome();    nextStep(); }
    else if (taskStep == 3 && elapsed >= tiltTime)   { tiltLeft();                         nextStep(); }
    else if (taskStep == 4 && elapsed >= tiltTime)   { footBackward(LeftFoot);             nextStep(); }
    else if (taskStep == 5 && elapsed >= spinTimeL)  { footStop(LeftFoot); Home();         endTask();  }
  }
  else if (currentTask == "walk_backward") {
    if      (taskStep == 0)                          { tiltRight();                        nextStep(); }
    else if (taskStep == 1 && elapsed >= tiltTime)   { footBackward(RightFoot);            nextStep(); }
    else if (taskStep == 2 && elapsed >= spinTimeRB) { footStop(RightFoot); legsHome();    nextStep(); }
    else if (taskStep == 3 && elapsed >= tiltTime)   { tiltLeft();                         nextStep(); }
    else if (taskStep == 4 && elapsed >= tiltTime)   { footForward(LeftFoot);              nextStep(); }
    else if (taskStep == 5 && elapsed >= spinTimeLB) { footStop(LeftFoot); Home();         endTask();  }
  }
  else if (currentTask == "walk_left") {
    if      (taskStep == 0)                          { tiltLeft();                         nextStep(); }
    else if (taskStep == 1 && elapsed >= tiltTime)   { footBackward(LeftFoot);             nextStep(); }
    else if (taskStep == 2 && elapsed >= spinTimeTL) { footStop(LeftFoot); Home();         endTask();  }
  }
  else if (currentTask == "walk_right") {
    if      (taskStep == 0)                          { tiltRight();                        nextStep(); }
    else if (taskStep == 1 && elapsed >= tiltTime)   { footForward(RightFoot);             nextStep(); }
    else if (taskStep == 2 && elapsed >= spinTimeTR) { footStop(RightFoot); Home();        endTask();  }
  }
  else if (currentTask == "wave") {
    if      (taskStep == 0)                    { RightArm.write(120); nextStep(); }
    else if (taskStep == 1 && elapsed >= 250)  { RightArm.write(60);  nextStep(); }
    else if (taskStep == 2 && elapsed >= 250)  { RightArm.write(120); nextStep(); }
    else if (taskStep == 3 && elapsed >= 250)  { RightArm.write(60);  nextStep(); }
    else if (taskStep == 4 && elapsed >= 250)  { RightArm.write(120); nextStep(); }
    else if (taskStep == 5 && elapsed >= 250)  { RightArm.write(90);  endTask();  }
  }
  else if (currentTask == "shake") {
    if      (taskStep == 0)                    { Head.write(60);  nextStep(); }
    else if (taskStep == 1 && elapsed >= 250)  { Head.write(120); nextStep(); }
    else if (taskStep == 2 && elapsed >= 250)  { Head.write(60);  nextStep(); }
    else if (taskStep == 3 && elapsed >= 250)  { Head.write(120); nextStep(); }
    else if (taskStep == 4 && elapsed >= 250)  { Head.write(60);  nextStep(); }
    else if (taskStep == 5 && elapsed >= 250)  { Head.write(90);  endTask();  }
  }
  else if (currentTask == "scan") {
    static int angle = 45;
    static bool scanning_right = true;
    if (taskStep == 0) {
      angle = 45; scanning_right = true;
      Head.write(angle); nextStep();
    }
    else if (taskStep == 1 && elapsed >= 20) {
      if (scanning_right) {
        angle += 3;
        if (angle >= 135) { scanning_right = false; nextStep(); }
      }
      Head.write(angle); taskStartTime = millis();
    }
    else if (taskStep == 2 && elapsed >= 20) {
      angle -= 3;
      if (angle <= 45) { Head.write(90); endTask(); }
      else             { Head.write(angle); taskStartTime = millis(); }
    }
  }
  else if (currentTask == "slash") {
    if      (taskStep == 0)                   { buzzer.sing(3); RightArm.write(180); nextStep(); }
    else if (taskStep == 1 && elapsed >= 150) { RightArm.write(0);   nextStep(); }
    else if (taskStep == 2 && elapsed >= 80)  { RightArm.write(180); nextStep(); }
    else if (taskStep == 3 && elapsed >= 80)  { RightArm.write(90);  endTask();  }
  }
  else if (currentTask == "uppercut") {
    if (taskStep == 0) {
      buzzer.sing(5);
      setLegs(leftLegHome - 15, rightLegHome - 55);
      nextStep();
    }
    else if (taskStep == 1 && elapsed >= 250) { RightArm.write(180); nextStep(); }
    else if (taskStep == 2 && elapsed >= 120) { RightArm.write(0);   nextStep(); }
    else if (taskStep == 3 && elapsed >= 80)  { Home();              endTask();  }
  }
  else if (currentTask == "spin") {
    if      (taskStep == 0)                   { buzzer.sing(5); RightArm.write(45); nextStep(); }
    else if (taskStep == 1 && elapsed >= 150) { Head.write(45);  nextStep(); }
    else if (taskStep == 2 && elapsed >= 120) { Head.write(135); nextStep(); }
    else if (taskStep == 3 && elapsed >= 120) { Head.write(45);  nextStep(); }
    else if (taskStep == 4 && elapsed >= 120) { Head.write(135); nextStep(); }
    else if (taskStep == 5 && elapsed >= 120) { Head.write(90); RightArm.write(90); endTask(); }
  }
  else if (currentTask == "stab") {
    if (taskStep == 0) {
      buzzer.sing(3);
      setLegs(leftLegHome - 25, rightLegHome - 65);
      RightArm.write(135); nextStep();
    }
    else if (taskStep == 1 && elapsed >= 250) {
      setLegs(leftLegHome + 65, rightLegHome - 25);
      RightArm.write(45); nextStep();
    }
    else if (taskStep == 2 && elapsed >= 150) { Home(); endTask(); }
  }
  else if (currentTask == "defense") {
    if (taskStep == 0) {
      buzzer.sing(2);
      RightArm.write(45); LeftArm.write(135);
      setLegs(leftLegHome + 30, rightLegHome - 70);
      nextStep();
    }
    else if (taskStep == 1 && elapsed >= 800) { Home(); endTask(); }
  }
  else if (currentTask == "taunt") {
    if      (taskStep == 0)                   { buzzer.sing(7); RightArm.write(45); nextStep(); }
    else if (taskStep == 1 && elapsed >= 250) { RightArm.write(60); Head.write(120); nextStep(); }
    else if (taskStep == 2 && elapsed >= 150) { RightArm.write(45); Head.write(60);  nextStep(); }
    else if (taskStep == 3 && elapsed >= 150) { RightArm.write(60); Head.write(120); nextStep(); }
    else if (taskStep == 4 && elapsed >= 150) { RightArm.write(45); Head.write(60);  nextStep(); }
    else if (taskStep == 5 && elapsed >= 150) { Head.write(90); RightArm.write(90);  endTask();  }
  }
}

/* ================== SETUP ================== */
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== INICIANDO OTTO NINJA ===");

  loadOffsets();
  loadAllCustomMelodies();
  Home();
  buzzer.sing(0);

  connectWiFi();
  buzzer.sing(11);

  /* ========== RUTAS HTTP ========== */

  server.on("/status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "OK");
  });

  server.on("/joystick", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    int x = server.arg("x").toInt();
    int y = server.arg("y").toInt();
    int L = constrain(y + x, -100, 100);
    int R = constrain(y - x, -100, 100);
    LeftFoot.write(map(L, -100, 100, 0, 180));
    RightFoot.write(map(R, -100, 100, 180, 0));
    server.send(200, "text/plain", "OK");
  });

  server.on("/mode", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String cmd = server.arg("cmd");
    if (cmd == "rodar") {
      buzzer.sing(2); RollMode();
      server.send(200, "text/plain", "Modo RODAR activado");
    } else if (cmd == "caminar") {
      buzzer.sing(2); QuickHome();
      server.send(200, "text/plain", "Modo CAMINAR activado");
    } else {
      server.send(400, "text/plain", "Comando invalido");
    }
  });

  server.on("/walk", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String cmd = server.arg("cmd");
    server.send(200, "text/plain", "OK");
    buzzer.sing(2);
    if      (cmd == "forward")   startTask("walk_forward");
    else if (cmd == "backward")  startTask("walk_backward");
    else if (cmd == "left")      startTask("walk_left");
    else if (cmd == "right")     startTask("walk_right");
    else if (cmd == "home")      QuickHome();
    else if (cmd == "roll_mode") RollMode();
  });

  server.on("/arms", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String cmd = server.arg("cmd");
    server.send(200, "text/plain", "OK");
    buzzer.sing(2);
    if      (cmd == "raise_left")  RaiseLeftArm();
    else if (cmd == "raise_right") RaiseRightArm();
    else if (cmd == "raise_both")  RaiseBothArms();
    else if (cmd == "lower_left")  LowerLeftArm();
    else if (cmd == "lower_right") LowerRightArm();
    else if (cmd == "lower")       LowerArms();
    else if (cmd == "wave")        startTask("wave");
  });

  server.on("/head", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String cmd = server.arg("cmd");
    server.send(200, "text/plain", "OK");
    buzzer.sing(2);
    if      (cmd == "left")   LookLeft();
    else if (cmd == "right")  LookRight();
    else if (cmd == "center") LookCenter();
    else if (cmd == "shake")  startTask("shake");
    else if (cmd == "scan")   startTask("scan");
  });

  server.on("/animation", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String cmd = server.arg("cmd");
    server.send(200, "text/plain", "OK");
  });

  server.on("/attack", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String cmd = server.arg("cmd");
    server.send(200, "text/plain", "OK");
    if      (cmd == "slash")    startTask("slash");
    else if (cmd == "uppercut") startTask("uppercut");
    else if (cmd == "spin")     startTask("spin");
    else if (cmd == "stab")     startTask("stab");
    else if (cmd == "defense")  startTask("defense");
    else if (cmd == "taunt")    startTask("taunt");
  });

  server.on("/buzzer", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!server.hasArg("song")) {
      server.send(400, "text/plain", "Missing song parameter");
      return;
    }
    int songNumber = server.arg("song").toInt();

    // Melodias predefinidas (0-15)
    if (songNumber >= 0 && songNumber <= 15) {
      server.send(200, "text/plain", "OK");
      buzzer.sing(songNumber);
    }
    // Melodias personalizadas (16-20)
    else if (songNumber >= CUSTOM_MELODY_START && songNumber < CUSTOM_MELODY_START + MAX_CUSTOM_MELODIES) {
      server.send(200, "text/plain", "OK");
      playCustomMelody(songNumber);
    }
    // Tono personalizado
    else if (songNumber == 99) {
      int freq     = server.arg("freq").toInt();
      int duration = server.arg("duration").toInt();
      if (freq > 0 && duration > 0) {
        server.send(200, "text/plain", "OK");
        buzzer.playTone(freq, duration);
      } else {
        server.send(400, "text/plain", "Invalid tone parameters");
      }
    } else {
      server.send(400, "text/plain", "Invalid song number");
    }
  });

  // ==================== MELODÍAS PERSONALIZADAS ====================
  server.on("/melody", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");

    String action = server.arg("action");

    // === GUARDAR MELODÍA ===
    if (action == "save") {
      int slot = server.arg("slot").toInt();
      String name = server.arg("name");
      String data = server.arg("data");

      if (slot < CUSTOM_MELODY_START || slot >= CUSTOM_MELODY_START + MAX_CUSTOM_MELODIES) {
        server.send(400, "text/plain", "Invalid slot");
        return;
      }
      if (name.length() == 0 || data.length() == 0) {
        server.send(400, "text/plain", "Missing name or data");
        return;
      }

      uint16_t notes[200]; // max 100 notas x 2 valores
      int noteCount = parseMelodyData(data, notes, MAX_NOTES_PER_MELODY);

      if (noteCount == 0) {
        server.send(400, "text/plain", "No valid notes");
        return;
      }

      saveCustomMelody(slot, name.c_str(), notes, noteCount);
      server.send(200, "text/plain", "Saved: " + name + " (" + String(noteCount) + " notes)");
    }

    // === LISTAR MELODÍAS ===
    else if (action == "list") {
      String json = "{\"melodies\":[";
      bool first = true;
      for (int i = 0; i < MAX_CUSTOM_MELODIES; i++) {
        if (customMelodies[i].loaded) {
          if (!first) json += ",";
          json += "{\"slot\":" + String(CUSTOM_MELODY_START + i);
          json += ",\"name\":\"" + String(customMelodies[i].name) + "\"";
          json += ",\"count\":" + String(customMelodies[i].noteCount) + "}";
          first = false;
        }
      }
      json += "]}";
      server.send(200, "application/json", json);
    }

    // === OBTENER MELODÍA ===
    else if (action == "get") {
      int slot = server.arg("slot").toInt();
      int index = slot - CUSTOM_MELODY_START;

      if (index < 0 || index >= MAX_CUSTOM_MELODIES || !customMelodies[index].loaded) {
        server.send(404, "text/plain", "Not found");
        return;
      }

      String json = "{\"slot\":" + String(slot);
      json += ",\"name\":\"" + String(customMelodies[index].name) + "\"";
      json += ",\"notes\":[";
      for (int i = 0; i < customMelodies[index].noteCount; i++) {
        if (i > 0) json += ",";
        json += "[" + String(customMelodies[index].notes[i * 2]);
        json += "," + String(customMelodies[index].notes[i * 2 + 1]) + "]";
      }
      json += "]}";
      server.send(200, "application/json", json);
    }

    // === ELIMINAR MELODÍA ===
    else if (action == "delete") {
      int slot = server.arg("slot").toInt();
      if (slot < CUSTOM_MELODY_START || slot >= CUSTOM_MELODY_START + MAX_CUSTOM_MELODIES) {
        server.send(400, "text/plain", "Invalid slot");
        return;
      }
      deleteCustomMelody(slot);
      server.send(200, "text/plain", "Deleted slot " + String(slot));
    }

    else {
      server.send(400, "text/plain", "Invalid action");
    }
  });

  server.on("/offset", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String response = "";
    bool changed = false;
    if (server.hasArg("left")) {
      offsetLeftLeg = constrain(server.arg("left").toInt(), -90, 90);
      response += "Left=" + String(offsetLeftLeg) + " ";
      changed = true;
    }
    if (server.hasArg("right")) {
      offsetRightLeg = constrain(server.arg("right").toInt(), -90, 90);
      response += "Right=" + String(offsetRightLeg);
      changed = true;
    }
    if (changed) {
      saveOffsets(); Home(); buzzer.sing(0);
      server.send(200, "text/plain", "Guardado: " + response);
    } else {
      response = "Left=" + String(offsetLeftLeg) + " Right=" + String(offsetRightLeg);
      server.send(200, "text/plain", response);
    }
  });

  server.on("/offset/reset", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    resetOffsets(); Home(); buzzer.sing(0);
    server.send(200, "text/plain", "Offsets reseteados");
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado\n");
}

void loop() {
  server.handleClient();
  processTask();
  yield();
}
