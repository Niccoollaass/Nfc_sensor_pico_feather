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
  readStoredData();
  delay(3000);
  
}

void handleFrame(const CANMessage &msg) {
  switch (msg.id) {
    case 0x200:
      arg1 = ""; arg2 = ""; arg3 = "";
      receiving = true;
      if (msg.data[0] == 0x01) {
        type_message = msg.data[1];
        expected_t1 = msg.data[2];
        expected_t2 = msg.data[3];
        expected_t3 = msg.data[4];
        Serial.println("Demande d'écriture reçue");
      } else if (msg.data[0] == 0x02) {
        Serial.println("Demande de lecture reçue");
        readStoredData();
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
  Serial.println("\n-> Écriture sur le tag NFC...");
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
  Serial.println("-> Écriture terminée.");
}

void readStoredData() {
  String type,arg1,arg2,arg3;

  String dataURL;
  if (st25dv.readURI(&dataURL) == ST25DV_OK) {
    Serial.print("-> unaURL : "); Serial.println(dataURL);
    Serial.print("-> unaURL complète : "); Serial.println(dataURL);
    type="URL";
    arg1=dataURL;
    arg2="";
    arg3="";
  }
  String dataUnaULR;
  if (st25dv.readUnabridgedURI(&dataUnaULR) == ST25DV_OK) {
    Serial.print("-> URL : "); Serial.println(dataUnaULR);
    Serial.print("-> URL complète : "); Serial.println(dataUnaULR);
    type="UnaURL";
    arg1=dataUnaULR;
    arg2="";
    arg3="";

  }

  String email, subject, message;
  if (st25dv.readEMail(&email, &subject, &message) == ST25DV_OK) {
    Serial.print("-> Email : "); Serial.println(email);
    Serial.print("-> Sujet : "); Serial.println(subject);
    Serial.print("-> Message : "); Serial.println(message);
    type="email";
    arg1=email;
    arg2=subject;
    arg3=message;
  }

  String num, sms;
  if (st25dv.readSMS(&num, &sms) == ST25DV_OK) {
    Serial.print("-> Numéro : "); Serial.println(num);
    Serial.print("-> SMS : "); Serial.println(sms);
    type="SMS";
    arg1=num;
    arg2=sms;
    arg3="";
  }

  String lat, lon;
  if (st25dv.readGEO(&lat, &lon) == ST25DV_OK) {
    Serial.print("-> Latitude : "); Serial.println(lat);
    Serial.print("-> Longitude : "); Serial.println(lon);
    type="GEO";
    arg1=lat;
    arg2=lon;
    arg3="";
  }

  Serial.println("-> Lecture terminée.\n");
  SendNFCDataToCan(type,arg1,arg2,arg3);
}
void SendNFCDataToCan(String type, String arg1, String arg2, String arg3) {
  CANMessage msg;

  // Envoi du type de données (0x300)
  msg.id = 0x300;
  msg.len = 8;
  msg.data[0] = 0; // Index (inutile ici)
  if (type == "URL") {
    msg.data[1] = 0x01;
  } else if (type == "UnaURL") {
    msg.data[1] = 0x02;
  } else if (type == "GEO") {
    msg.data[1] = 0x03;
  } else if (type == "SMS") {
    msg.data[1] = 0x04;
  } else if (type == "email") {
    msg.data[1] = 0x05;
  } else {
    msg.data[1] = 0x00; // Type inconnu
  }
  for (int i = 2; i < 8; i++) msg.data[i] = 0;
  can.tryToSend(msg);

  // Envoi de arg1 (0x301)
  for (int i = 0; i < arg1.length(); i += 7) {
    msg.id = 0x301;
    msg.len = 8;
    msg.data[0] = i / 7; // Index de la trame
    for (int j = 0; j < 7; j++) {
      if (i + j < arg1.length()) {
        msg.data[j + 1] = arg1[i + j];
      } else {
        msg.data[j + 1] = 0;
      }
    }
    can.tryToSend(msg);
  }

  // Envoi de arg2 (0x302)
  for (int i = 0; i < arg2.length(); i += 7) {
    msg.id = 0x302;
    msg.len = 8;
    msg.data[0] = i / 7; // Index de la trame
    for (int j = 0; j < 7; j++) {
      if (i + j < arg2.length()) {
        msg.data[j + 1] = arg2[i + j];
      } else {
        msg.data[j + 1] = 0;
      }
    }
    can.tryToSend(msg);
  }

  // Envoi de arg3 (0x303)
  for (int i = 0; i < arg3.length(); i += 7) {
    msg.id = 0x303;
    msg.len = 8;
    msg.data[0] = i / 7; // Index de la trame
    for (int j = 0; j < 7; j++) {
      if (i + j < arg3.length()) {
        msg.data[j + 1] = arg3[i + j];
      } else {
        msg.data[j + 1] = 0;
      }
    }
    can.tryToSend(msg);
  }

  // Envoi de la fin de la transmission (0x305)
  msg.id = 0x305;
  msg.len = 8;
  for (int i = 0; i < 8; i++) msg.data[i] = 0xFF;
  can.tryToSend(msg);
}
