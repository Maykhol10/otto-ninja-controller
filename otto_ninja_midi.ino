#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
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

/* ================== OLED DISPLAY ================== */
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS 0x3C   // Cambia a 0x3D si tu pantalla lo requiere
// Pines I2C por defecto en ESP32: SDA=21, SCL=22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;
String lastDisplayMessage = "";

// Buffer para bitmap recibido desde la app (128x48 / 8 = 768 bytes — zona azul del OLED)
static uint8_t receivedBitmap[768];
bool bitmapActive = false;

/* ================== SENSOR ULTRASÓNICO HC-SR04 ================== */
const int TRIG_PIN = 4;
const int ECHO_PIN = 5;

bool   usEnabled       = false;
int    usDangerDist    = 15;    // cm — zona de peligro
int    usAlertDist     = 40;    // cm — zona de alerta
String usReaction      = "stop";
bool   usBuzzerAlert   = false;
bool   usDisplayAlert  = false;

unsigned long usLastAutoRead = 0;
const  unsigned long US_INTERVAL = 250; // ms entre lecturas automáticas

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

/* ================== OLED - FUNCIONES ================== */

void oledSplash() {
  if (!oledAvailable) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("  Otto");
  display.setCursor(10, 32);
  display.println("  Ninja");
  display.display();
}

void oledShowIP(String ip) {
  if (!oledAvailable) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Otto Ninja - Online");
  display.drawLine(0, 10, SCREEN_WIDTH - 1, 10, SSD1306_WHITE);
  display.setCursor(0, 18);
  display.println("IP:");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println(ip);
  display.setCursor(0, 50);
  display.println("Listo para controlar");
  display.display();
}

void displayMessage(String msg) {
  if (!oledAvailable) return;
  lastDisplayMessage = msg;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Cabecera
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Mensaje:");
  display.drawLine(0, 10, SCREEN_WIDTH - 1, 10, SSD1306_WHITE);

  // Cuerpo del mensaje
  display.setCursor(0, 16);

  // Tamaño grande para textos cortos, pequeño para largos
  if (msg.length() <= 9) {
    display.setTextSize(2);
  } else if (msg.length() <= 20) {
    display.setTextSize(1);
  } else {
    // Texto largo: partir en líneas de ~21 caracteres
    display.setTextSize(1);
  }

  display.println(msg);
  display.display();

  Serial.println("OLED Display: " + msg);
}

void displayClear() {
  if (!oledAvailable) return;
  lastDisplayMessage = "";
  oledShowIP(WiFi.localIP().toString());
}

/* ================== SENSOR ULTRASÓNICO - FUNCIONES ================== */

long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms ≈ ~5m
  if (duration == 0) return -1;                   // sin eco = fuera de rango
  return duration / 29.0 / 2;                     // cm (29µs por cm, ida y vuelta)
}

void processUltrasonic() {
  unsigned long now = millis();
  if (now - usLastAutoRead < US_INTERVAL) return;
  usLastAutoRead = now;

  long dist = readDistanceCM();

  if (dist < 0 || !usEnabled) return;

  // Reacciones y alertas (solo si monitoring está habilitado)
  if (dist <= usDangerDist) {
    if (usBuzzerAlert) buzzer.sing(1);
    if (usDisplayAlert && oledAvailable) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 0);
      display.println("PELIGRO!");
      display.setTextSize(1);
      display.setCursor(0, 32);
      display.print(dist); display.println(" cm");
      display.display();
    }
    if (usReaction == "stop") {
      endTask();
      footStop(LeftFoot);
      footStop(RightFoot);
    } else if (usReaction == "back") {
      endTask();
      startTask("walk_backward");
    }
  } else if (dist <= usAlertDist) {
    if (usDisplayAlert && oledAvailable) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("Alerta:");
      display.setTextSize(2);
      display.setCursor(0, 16);
      display.print(dist); display.println(" cm");
      display.display();
    }
  }
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

  // Inicializar sensor ultrasónico
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("Sensor ultrasonico: OK (TRIG=4, ECHO=5)");

  // Inicializar OLED (SDA=21, SCL=22 por defecto en ESP32)
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED: No encontrada, continuando sin display");
    oledAvailable = false;
  } else {
    oledAvailable = true;
    Serial.println("OLED: OK");
    oledSplash();
  }

  loadOffsets();
  loadAllCustomMelodies();
  Home();
  buzzer.sing(0);

  connectWiFi();
  buzzer.sing(11);

  // Mostrar IP en el display al conectar
  oledShowIP(WiFi.localIP().toString());

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

  // ==================== DISPLAY OLED ====================
  server.on("/message", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");

    if (!server.hasArg("text")) {
      server.send(400, "text/plain", "Missing 'text' parameter");
      return;
    }

    String msg = server.arg("text");
    msg.trim();

    if (msg.length() == 0) {
      server.send(400, "text/plain", "Empty message");
      return;
    }

    if (msg == "clear") {
      displayClear();
      server.send(200, "text/plain", "Display cleared");
      return;
    }

    displayMessage(msg);
    server.send(200, "text/plain", "OK");
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

  // ==================== BITMAP OLED ====================
  // CORS preflight — el navegador lo envía antes del POST con Content-Type: application/json
  server.on("/bitmap", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
  });

  // Recibe 768 bytes de bitmap (128x48) y los dibuja en la zona azul del OLED
  server.on("/bitmap", HTTP_POST, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");

    if (!oledAvailable) {
      server.send(503, "text/plain", "OLED no disponible");
      return;
    }

    String body = server.arg("plain");

    // Buscar el array JSON: {"data":[b0,b1,...,b767]}
    int arrStart = body.indexOf('[');
    int arrEnd   = body.lastIndexOf(']');
    if (arrStart < 0 || arrEnd <= arrStart) {
      server.send(400, "text/plain", "JSON invalido");
      return;
    }

    String arrStr = body.substring(arrStart + 1, arrEnd);
    int byteCount = 0;
    int pos = 0;
    int len = arrStr.length();

    while (pos < len && byteCount < 768) {
      while (pos < len && (arrStr[pos] == ' ' || arrStr[pos] == '\n' || arrStr[pos] == '\r')) pos++;
      int numStart = pos;
      while (pos < len && arrStr[pos] != ',' && arrStr[pos] != ']') pos++;
      if (pos > numStart) {
        receivedBitmap[byteCount++] = (uint8_t)arrStr.substring(numStart, pos).toInt();
      }
      pos++;
    }

    if (byteCount != 768) {
      server.send(400, "text/plain", "Se esperaban 768 bytes, recibidos: " + String(byteCount));
      return;
    }

    // Extraer título e inversión del JSON: {"title":"...","invert":...,"data":[...]}
    String oledTitle = "Otto Ninja";
    int tStart = body.indexOf("\"title\"");
    if (tStart >= 0) {
      int q1 = body.indexOf('"', tStart + 7);
      int q2 = body.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1) {
        oledTitle = body.substring(q1 + 1, q2);
        if (oledTitle.length() == 0) oledTitle = "Otto Ninja";
      }
    }
    bool oledTitleInvert = (body.indexOf("\"titleInvert\":true") >= 0);

    display.clearDisplay();
    // Zona amarilla (filas 0-15): título personalizado centrado
    display.setTextSize(1);
    int titleX = max(0, (128 - (int)oledTitle.length() * 6) / 2);
    if (oledTitleInvert) {
      display.fillRect(0, 0, 128, 16, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(titleX, 4);
    display.print(oledTitle);
    // Zona azul (filas 16-63): imagen 128x48
    display.drawBitmap(0, 16, receivedBitmap, 128, 48, SSD1306_WHITE);
    display.display();
    bitmapActive = true;

    Serial.print("OLED: bitmap con titulo '"); Serial.print(oledTitle); Serial.println("'");
    server.send(200, "text/plain", "OK");
  });

  // ==================== SENSOR ULTRASÓNICO ====================
  server.on("/ultrasonic", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String action = server.arg("action");

    if (action == "read") {
      long dist = readDistanceCM();
      String json = "{\"distance\":" + String(dist) + "}";
      server.send(200, "application/json", json);
    }
    else if (action == "config") {
      if (server.hasArg("enabled"))  usEnabled     = server.arg("enabled").toInt() == 1;
      if (server.hasArg("danger"))   usDangerDist  = server.arg("danger").toInt();
      if (server.hasArg("alert"))    usAlertDist   = server.arg("alert").toInt();
      if (server.hasArg("reaction")) usReaction    = server.arg("reaction");
      if (server.hasArg("buzzer"))   usBuzzerAlert  = server.arg("buzzer").toInt() == 1;
      if (server.hasArg("display"))  usDisplayAlert = server.arg("display").toInt() == 1;
      Serial.print("US config: enabled="); Serial.print(usEnabled);
      Serial.print(" danger=");  Serial.print(usDangerDist);
      Serial.print(" alert=");   Serial.println(usAlertDist);
      server.send(200, "text/plain", "OK");
    }
    else {
      server.send(400, "text/plain", "Accion invalida");
    }
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado\n");
}

void loop() {
  server.handleClient();
  processTask();
  processUltrasonic();
  yield();
}
