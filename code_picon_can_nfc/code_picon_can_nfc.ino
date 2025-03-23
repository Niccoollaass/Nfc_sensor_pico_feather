// === Bibliothèques ===
#include <Wire.h>
#include <ACAN2515.h>
#include "ST25DVSensor.h"

// === Broches ===
#define I2C_SDA 8
#define I2C_SCL 9
#define MCP2515_SCK 2
#define MCP2515_MOSI 3
#define MCP2515_MISO 4
#define MCP2515_CS 5
#define MCP2515_INT 1

// === CAN ===
ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);
#define QUARTZ_FREQUENCY 16000000UL

// === NFC ===
ST25DV st25dv(-1, -1, &Wire);

// === Buffers ===
String arg1 = "";
String arg2 = "";
String arg3 = "";

uint8_t expected_t1 = 0, expected_t2 = 0, expected_t3 = 0;
uint8_t type_message = 0;
bool receiving = false;

void setup() {
  Serial.begin(115200);
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();
  SPI.setSCK(MCP2515_SCK);
  SPI.setTX(MCP2515_MOSI);
  SPI.setRX(MCP2515_MISO);
  SPI.setCS(MCP2515_CS);
  SPI.begin();

  ACAN2515Settings settings(QUARTZ_FREQUENCY, 125000);
  if (can.begin(settings, [] { can.isr(); }) != 0) {
    Serial.println("Erreur CAN");
    while (1);
  }

  if (st25dv.begin() != 0) {
    Serial.println("Erreur NFC");
    while (1);
  }
  Serial.println("Init OK");
}

void loop() {
  CANMessage msg;
  if (can.available()) {
    can.receive(msg);
    handleFrame(msg);
  }
}

void handleFrame(const CANMessage &msg) {
  switch (msg.id) {
    case 0x200:
      arg1 = ""; arg2 = ""; arg3 = "";
      receiving = true;
      if (msg.data[0] == 0x01) { // Écriture
        type_message = msg.data[1];
        expected_t1 = msg.data[2];
        expected_t2 = msg.data[3];
        expected_t3 = msg.data[4];
        Serial.println("Demande d'écriture reçue");
      } else if (msg.data[0] == 0x02) { // Lecture
        handleReadRequest();
      }
      break;

    case 0x201:
      if (receiving && msg.data[0] < expected_t1) {
        for (int i = 1; i < msg.len; i++) arg1 += (char)msg.data[i];
      }
      break;
    case 0x202:
      if (receiving && msg.data[0] < expected_t2) {
        for (int i = 1; i < msg.len; i++) arg2 += (char)msg.data[i];
      }
      break;
    case 0x203:
      if (receiving && msg.data[0] < expected_t3) {
        for (int i = 1; i < msg.len; i++) arg3 += (char)msg.data[i];
      }
      break;

    case 0x204:
      if (receiving) {
        handleWriteRequest();
        receiving = false;
      }
      break;
  }
}

void handleWriteRequest() {
  Serial.println("Écriture NFC");
  switch (type_message) {
    case 0x01:
      st25dv.writeURI("https://", arg1, ""); break;
    case 0x02:
      st25dv.writeUnabridgedURI(arg1, ""); break;
    case 0x03:
      st25dv.writeGEO(arg1, arg2, ""); break;
    case 0x04:
      st25dv.writeSMS(arg1, arg2, ""); break;
    case 0x05:
      st25dv.writeEMail(arg1, arg2, arg3, ""); break;
  }
  Serial.println("Écriture terminée");
}

void handleReadRequest() {
  Serial.println("Lecture NFC");
  String a1 = "", a2 = "", a3 = "";
  uint8_t t1 = 0, t2 = 0, t3 = 0;
  type_message = st25dv.readNDEFType();

  CANMessage out;
  out.id = 0x300;
  out.len = 8;
  out.data[0] = 0x02; // lecture
  out.data[1] = type_message;

  switch (type_message) {
    case 0x01:
    case 0x02:
      if (st25dv.readURI(&a1) == 0) t1 = sendFragments(0x301, a1);
      break;
    case 0x03:
      if (st25dv.readGEO(&a1, &a2) == 0) {
        t1 = sendFragments(0x301, a1);
        t2 = sendFragments(0x302, a2);
      }
      break;
    case 0x04:
      if (st25dv.readSMS(&a1, &a2) == 0) {
        t1 = sendFragments(0x301, a1);
        t2 = sendFragments(0x302, a2);
      }
      break;
    case 0x05:
      if (st25dv.readEMail(&a1, &a2, &a3) == 0) {
        t1 = sendFragments(0x301, a1);
        t2 = sendFragments(0x302, a2);
        t3 = sendFragments(0x303, a3);
      }
      break;
  }

  out.data[2] = t1;
  out.data[3] = t2;
  out.data[4] = t3;
  out.data[5] = out.data[6] = out.data[7] = 0;
  can.tryToSend(out);

  delay(5);
  CANMessage fin;
  fin.id = 0x305;
  fin.len = 8;
  for (int i = 0; i < 8; i++) fin.data[i] = 0xFF;
  can.tryToSend(fin);
}

uint8_t sendFragments(uint32_t base_id, const String &msg) {
  uint8_t index = 0;
  for (uint16_t i = 0; i < msg.length(); i += 7) {
    CANMessage part;
    part.id = base_id;
    part.len = 8;
    part.data[0] = index++;
    for (uint8_t j = 0; j < 7; j++) {
      part.data[j + 1] = (i + j < msg.length()) ? msg[i + j] : 0;
    }
    can.tryToSend(part);
    delay(2);
  }
  return index;
}
