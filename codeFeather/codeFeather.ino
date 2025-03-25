// Feather code avec fonctions pour chaque type de message CAN vers la Pico NFC
#include <ACAN2515.h>
#include <wiring_private.h>
#include <PubSubClient.h>
#include <WiFi101.h>
// CAN -----------------------------------------------

static const byte MCP2515_SCK = 12;
static const byte MCP2515_SI  = 11;
static const byte MCP2515_SO  = 10;
SPIClass mySPI (&sercom1, MCP2515_SO, MCP2515_SI, MCP2515_SCK, SPI_PAD_0_SCK_3, SERCOM_RX_PAD_2);
static const byte MCP2515_CS  = 6;
static const byte MCP2515_INT = 5;

ACAN2515 can (MCP2515_CS, mySPI, MCP2515_INT);
static const uint32_t QUARTZ_FREQUENCY = 16 * 1000 * 1000;
// CAN -----------------------------------------------
// MQTT -----------------------------------------------
#define MQTT_BROKER "10.3.141.1"
#define MQTT_PORT 1883

WiFiClient wifiClient;
PubSubClient client(wifiClient);
char ssid[] = "raspi-webgui2";
char pass[] = "ChangeMe";
int status = WL_IDLE_STATUS;
// MQTT -----------------------------------------------


String buffer1[16];
String buffer2[16];
String buffer3[16];
uint8_t len1 = 0, len2 = 0, len3 = 0;
uint8_t typeRecu = 0;
bool lectureEnCours = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(38400);
  while (!Serial) {
    delay(50);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  mySPI.begin();
  pinPeripheral(MCP2515_SI, PIO_SERCOM);
  pinPeripheral(MCP2515_SCK, PIO_SERCOM);
  pinPeripheral(MCP2515_SO, PIO_SERCOM);

  Serial.println("Configure ACAN2515");
  ACAN2515Settings settings(QUARTZ_FREQUENCY, 125 * 1000);
  const uint32_t errorCode = can.begin(settings, [] { can.isr(); });

  if (errorCode != 0) {
    Serial.print("CAN Init Fail, code: 0x"); Serial.println(errorCode, HEX);
    while (1);
  }
  Serial.println("CAN Init OK");
// MQTT
  WiFi.setPins(8, 7, 4, 2);
  Serial.begin(9600);
  while (!Serial) {
    ; // attente de connexion
  }
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    while (true);
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }

  Serial.println("Connected to the network");
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);

  if (client.connect("Feather-M0 Nicolas", "Nicolass", 1, true, "Fermeture")) {
    Serial.println("Connected to MQTT broker");
    client.subscribe("Nicolass");
  } else {
    Serial.print("MQTT connection failed, rc=");
    Serial.println(client.state());
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  static uint32_t lastRequest = 0;
  // Toutes les 3 secondes, on envoie une requête de lecture
  // requestRead();  // Envoie trame 0x200 avec 0x02
  // sendSMS("06915187375","salut a tous les amis");

  // On lit toutes les trames CAN disponibles et on les traite
  CANMessage msg;
  while (can.available()) {
    can.receive(msg);
    handleCANResponse(msg);  // Met à jour buffers 1/2/3
  }
}

void sendHeader(uint8_t req, uint8_t type, uint8_t l1, uint8_t l2, uint8_t l3) {
  CANMessage msg;
  msg.id = 0x200;
  msg.len = 8;
  msg.data[0] = req;
  msg.data[1] = type;
  msg.data[2] = l1;
  msg.data[3] = l2;
  msg.data[4] = l3;
  msg.data[5] = msg.data[6] = msg.data[7] = 0;
  can.tryToSend(msg);
  Serial.println("Header sent");
}

void sendData(uint16_t id, uint8_t index, const char* data) {
  CANMessage msg;
  msg.id = id;
  msg.len = 8;
  msg.data[0] = index;
  for (uint8_t i = 0; i < 7; i++) {
    msg.data[i + 1] = (data[i] != '\0') ? data[i] : 0;
  }
  can.tryToSend(msg);
  Serial.print("Sent data frame ");
  Serial.print(index);
  Serial.print(" to ID 0x");
  Serial.println(id, HEX);
  delay(5);
}

void sendEnd(uint16_t id) {
  CANMessage msg;
  msg.id = id;
  msg.len = 8;
  for (int i = 0; i < 8; i++) msg.data[i] = 0xFF;
  can.tryToSend(msg);
  Serial.print("Sent end frame to ID 0x");
  Serial.println(id, HEX);
}

// ===== Fonctions spécifiques pour chaque type de message =====

void sendSMS(const char* number, const char* message) {
  uint8_t numberFrames = (strlen(number) + 6) / 7;
  uint8_t messageFrames = (strlen(message) + 6) / 7;

  sendHeader(0x01, 0x04, numberFrames, messageFrames, 0);

  for (uint8_t i = 0; i < numberFrames; i++) {
    sendData(0x201, i, number + (i * 7));
  }

  for (uint8_t i = 0; i < messageFrames; i++) {
    sendData(0x202, i, message + (i * 7));
  }

  sendEnd(0x204);
}

void sendEmail(const char* email, const char* subject, const char* content) {
  uint8_t emailFrames = (strlen(email) + 6) / 7;
  uint8_t subjectFrames = (strlen(subject) + 6) / 7;
  uint8_t contentFrames = (strlen(content) + 6) / 7;

  sendHeader(0x01, 0x05, emailFrames, subjectFrames, contentFrames);

  for (uint8_t i = 0; i < emailFrames; i++) {
    sendData(0x201, i, email + (i * 7));
  }

  for (uint8_t i = 0; i < subjectFrames; i++) {
    sendData(0x202, i, subject + (i * 7));
  }

  for (uint8_t i = 0; i < contentFrames; i++) {
    sendData(0x203, i, content + (i * 7));
  }

  sendEnd(0x204);
}

void sendURL(const char* url) {
  uint8_t urlFrames = (strlen(url) + 6) / 7;

  sendHeader(0x01, 0x01, urlFrames, 0, 0);

  for (uint8_t i = 0; i < urlFrames; i++) {
    sendData(0x201, i, url + (i * 7));
  }

  sendEnd(0x204);
}

void sendUnabridgedURL(const char* fullurl) {
  uint8_t fullurlFrames = (strlen(fullurl) + 6) / 7;

  sendHeader(0x01, 0x02, fullurlFrames, 0, 0);

  for (uint8_t i = 0; i < fullurlFrames; i++) {
    sendData(0x201, i, fullurl + (i * 7));
  }

  sendEnd(0x204);
}
void sendGEO(const char* lat, const char* lon) {
  uint8_t latFrames = (strlen(lat) + 6) / 7;
  uint8_t lonFrames = (strlen(lon) + 6) / 7;

  sendHeader(0x01, 0x03, latFrames, lonFrames, 0);

  for (uint8_t i = 0; i < latFrames; i++) {
    sendData(0x201, i, lat + (i * 7));
  }

  for (uint8_t i = 0; i < lonFrames; i++) {
    sendData(0x202, i, lon + (i * 7));
  }

  sendEnd(0x204);
}

void requestRead() {
  sendHeader(0x02, 0x00, 0, 0, 0);
  lectureEnCours = true;
  len1 = len2 = len3 = 0;
}
void handleCANResponse(const CANMessage &msg) {
  if (!lectureEnCours) return;

  if (msg.id == 0x300) {
    typeRecu = msg.data[1];
    Serial.print("Type NFC : ");
    switch (typeRecu) {
      case 0x01: Serial.println("URL"); break;
      case 0x02: Serial.println("UnaURL"); break;
      case 0x03: Serial.println("GEO"); break;
      case 0x04: Serial.println("SMS"); break;
      case 0x05: Serial.println("Email"); break;
      default: Serial.println("Inconnu"); break;
    }
  } else if (msg.id == 0x301) {
    buffer1[msg.data[0]] = extractPayload(msg);
    len1++;
  } else if (msg.id == 0x302) {
    buffer2[msg.data[0]] = extractPayload(msg);
    len2++;
  } else if (msg.id == 0x303) {
    buffer3[msg.data[0]] = extractPayload(msg);
    len3++;
  } else if (msg.id == 0x305) {
    lectureEnCours = false;
    Serial.println("\n--- CONTENU DU TAG NFC ---");
    String payload = "Type: ";
    switch (typeRecu) {
      case 0x01: // URL
        printBuffer("URL:", buffer1, len1);
        payload += "URL, URL: " + concatBuffer(buffer1, len1); // Utiliser concatBuffer
        break;
      case 0x02: // UnaURL
        printBuffer("UnaURL:", buffer1, len1);
        payload += "UnaURL, UnaURL: " + concatBuffer(buffer1, len1); // Utiliser concatBuffer
        break;
      case 0x03: // GEO
        printBuffer("Latitude:", buffer1, len1);
        printBuffer("Longitude:", buffer2, len2);
        payload += "GEO, Latitude: " + concatBuffer(buffer1, len1) + ", Longitude: " + concatBuffer(buffer2, len2); // Utiliser concatBuffer
        break;
      case 0x04: // SMS
        printBuffer("Numéro:", buffer1, len1);
        printBuffer("SMS:", buffer2, len2);
        payload += "SMS, Numéro: " + concatBuffer(buffer1, len1) + ", SMS: " + concatBuffer(buffer2, len2); // Utiliser concatBuffer
        break;
      case 0x05: // Email
        printBuffer("Email:", buffer1, len1);
        printBuffer("Sujet:", buffer2, len2);
        printBuffer("Message:", buffer3, len3);
        payload += "Email, Email: " + concatBuffer(buffer1, len1) + ", Sujet: " + concatBuffer(buffer2, len2) + ", Message: " + concatBuffer(buffer3, len3); // Utiliser concatBuffer
        break;
      default:
        printBuffer("Arg1:", buffer1, len1);
        printBuffer("Arg2:", buffer2, len2);
        printBuffer("Arg3:", buffer3, len3);
        payload += "Inconnu, Arg1: " + concatBuffer(buffer1, len1) + ", Arg2: " + concatBuffer(buffer2, len2) + ", Arg3: " + concatBuffer(buffer3, len3); // Utiliser concatBuffer
        break;
    }
    Serial.println("--------------------------");
    // Publication sur MQTT
    client.publish("Nicolass", payload.c_str());
    Serial.println("Publié sur MQTT: " + payload);
  }
}

String concatBuffer(String *buf, uint8_t len) {
  String result = "";
  for (uint8_t i = 0; i < len; i++) {
    result += buf[i];
  }
  return result;
}

String extractPayload(const CANMessage &msg) {
  String s = "";
  for (uint8_t i = 1; i < msg.len; i++) {
    if (msg.data[i] != 0) s += (char)msg.data[i];
  }
  return s;
}

void printBuffer(const char* label, String *buf, uint8_t len) {
  Serial.print(label);
  Serial.print(" ");
  for (uint8_t i = 0; i < len; i++) Serial.print(buf[i]);
  Serial.println();
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message ="";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println();
  if (strcmp(topic, "Nicolass") == 0) { // Vérifier le topic
    if (message == "READ") {
      client.publish("Nicolass","Request_Read");
      Serial.print("Request Read");
      requestRead();
    } else if (message.startsWith("WRITE-EMAIL-")) {
      String email = message.substring(message.indexOf("WRITE-EMAIL-") + 12, message.indexOf("-", message.indexOf("WRITE-EMAIL-") + 12));
      String subject = message.substring(message.indexOf("-", message.indexOf("WRITE-EMAIL-") + 12) + 1, message.indexOf("-", message.indexOf("-", message.indexOf("WRITE-EMAIL-") + 12) + 1));
      String body = message.substring(message.indexOf("-", message.indexOf("-", message.indexOf("-", message.indexOf("WRITE-EMAIL-") + 12) + 1)) + 1);
      sendEmail(email.c_str(), subject.c_str(), body.c_str());
    } else if (message.startsWith("WRITE-URL-")) {
      String url = message.substring(message.indexOf("WRITE-URL-") + 10);
      sendURL(url.c_str());
    } else if (message.startsWith("WRITE-SMS-")) {
      String number = message.substring(message.indexOf("WRITE-SMS-") + 10, message.indexOf("-", message.indexOf("WRITE-SMS-") + 10));
      String sms = message.substring(message.indexOf("-", message.indexOf("WRITE-SMS-") + 10) + 1);
      sendSMS(number.c_str(), sms.c_str());
    } else if (message.startsWith("WRITE-GEO-")) {
      String lat = message.substring(message.indexOf("WRITE-GEO-") + 10, message.indexOf("-", message.indexOf("WRITE-GEO-") + 10));
      String lon = message.substring(message.indexOf("-", message.indexOf("WRITE-GEO-") + 10) + 1);
      sendGEO(lat.c_str(), lon.c_str());
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("arduinoClient")) {
      Serial.println("connected");
      client.subscribe("Nicolass");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}