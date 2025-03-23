#include <Wire.h>
#include "ST25DVSensor.h"

// Définition des broches I2C pour la Raspberry Pi Pico
#define I2C_SDA 8  // GP8 (SDA)
#define I2C_SCL 9  // GP9 (SCL)
#define DEV_I2C Wire

// Création de l'objet ST25DV
ST25DV st25dv(-1, -1, &DEV_I2C);

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialisation de l'I2C
    Wire.setSDA(I2C_SDA);
    Wire.setSCL(I2C_SCL);
    Wire.begin();

    // Initialisation du ST25DV
    Serial.println("Initialisation du ST25DV...");
    if (st25dv.begin() == 0) {
        Serial.println("ST25DV prêt !");
    } else {
        Serial.println("Erreur : ST25DV non détecté !");
        while (1);
    }

    // Écriture d'une URL et d'un e-mail dans le NFC
    writeNewEmail();
    writeNewURL();
    
}

void loop() {
    // Lecture des données NFC en boucle
    delay(3000);
    writeNewURL();
}

// Écriture d'une URL NFC
void writeNewURL() {
    Serial.println("Écriture de l'URL : https://example.com");
    if (st25dv.writeURI("https://", "manonjetaime.com", "") == ST25DV_OK) {
        Serial.println("URL enregistrée !");
    } else {
        Serial.println("Erreur d'écriture !");
    }
}

// Écriture d'un E-mail NFC
void writeNewEmail() {
    Serial.println("Écriture d'un e-mail vers contact@example.com.");
    if (st25dv.writeEMail("contact@example.com", "Sujet NFC", "Ceci est un test NFC.", "") == ST25DV_OK) {
        Serial.println("E-mail enregistré !");
    } else {
        Serial.println("Erreur d'écriture !");
    }
}

// Lecture de toutes les données stockées sur le NFC
void readStoredData() {
    String data;

      if (st25dv.readURI(&data) == ST25DV_OK) {
          Serial.println(data);
      }

    if (st25dv.readEMail(&data, &data, &data) == ST25DV_OK) {
        Serial.print("E-mail lu : ");
        Serial.println(data);
    }

    Serial.println("Lecture terminée.");
}
