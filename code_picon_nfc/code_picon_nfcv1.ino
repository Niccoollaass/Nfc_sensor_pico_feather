#include <Wire.h>
#include "ST25DVSensor.h"

// Définition des broches I2C pour la Raspberry Pi Pico
#define I2C_SDA 8  // GP8 (SDA)
#define I2C_SCL 9  // GP9 (SCL)

// Utilisation de l'instance Wire par défaut
#define DEV_I2C Wire

// Création de l'objet ST25DV
ST25DV st25dv(-1, -1, &DEV_I2C);  // -1 pour GPO et LPD (non utilisés)

// Nouvelle URL à stocker
const char uri_write_message[] = "https://www.youtube.com/watch?v=9O-nn93Z5p4";
const char uri_write_protocol[] = URI_ID_0x02_STRING; // "https://"

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialisation de l'I2C avec les broches correctes
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

    // Écriture de la nouvelle URL sur le tag NFC
    writeNewURL();
}

void loop() {
    // Lecture de l'URL enregistrée en boucle
    readStoredURL();
    delay(3000);
    writeNewURL();
}

// Fonction pour écrire une nouvelle URL sur le tag NFC
void writeNewURL() {
    Serial.print("Écriture de l'URL : ");
    Serial.print(uri_write_protocol);
    Serial.println(uri_write_message);

    if (st25dv.writeURI(uri_write_protocol, uri_write_message, "")) {
        Serial.println("Erreur lors de l'écriture !");
    } else {
        Serial.println("Nouvelle URL enregistrée avec succès !");
    }
}

// Fonction pour lire l'URL stockée sur le tag NFC
void readStoredURL() {
    String uri_read;
    
    if (st25dv.readURI(&uri_read) == ST25DV_OK) {
        Serial.print("URL stockée sur le tag NFC : ");
        Serial.println(uri_read.c_str());
    } else {
        Serial.println("Aucune URL trouvée sur le tag NFC.");
    }
}
