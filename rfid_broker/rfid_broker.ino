#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ------------------- RFID -------------------
#define SS_PIN 5
#define RST_PIN 22
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ------------------- LEDs -------------------
#define LED_VERTE 2
#define LED_ROUGE 4

// ------------------- WiFi -------------------
const char* ssid = "Adam";
const char* password = "123456789";

// ------------------- MQTT Secure Broker -------------------
const char *mqtt_broker = "tebebeeb.ala.eu-central-1.emqxsl.com";
const char *mqtt_topic_rfid = "esp32/rfid";
const char *mqtt_username = "BTSESP";
const char *mqtt_password = "123";
const int mqtt_port = 8883;
const String client_id = "esp32-rfid";

// Certificat Root CA pour la connexion SSL/TLS
const char *ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)EOF";

// ------------------- Clients -------------------
WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23, 5); // SCK, MISO, MOSI, SS
  
  mfrc522.PCD_Init();

  pinMode(LED_VERTE, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);
  digitalWrite(LED_VERTE, LOW);
  digitalWrite(LED_ROUGE, LOW);

  connectToWiFi();  // Correction : appel de la fonction WiFi ici
  espClient.setCACert(ca_cert);
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  connectToMQTT();

  Serial.println("Système prêt. Approche un badge...");
}

// ------------------- Loop principal -------------------
void loop() {
  if (!mqtt_client.connected()) connectToMQTT();
  mqtt_client.loop();

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  Serial.println("Badge détecté : " + uid);

  String statut;
  if (uid == "BBD82250" || uid == "1475205E") {
    statut = "Autorisé";
    digitalWrite(LED_VERTE, HIGH);
  } else {
    statut = "Refusé";
    digitalWrite(LED_ROUGE, HIGH);
  }

  // Format InfluxDB
  String payload = "rfid,device=esp32 uid=\"" + uid + "\",statut=\"" + statut + "\"";
  Serial.println("Publication MQTT : " + payload);
  mqtt_client.publish(mqtt_topic_rfid, payload.c_str());

  delay(2000);
  digitalWrite(LED_VERTE, LOW);
  digitalWrite(LED_ROUGE, LOW);

  mfrc522.PICC_HaltA();       // Arrêt de la communication avec la carte
  mfrc522.PCD_StopCrypto1();  // Libère le module pour la prochaine lecture
  verifierRC522();

}

// ------------------- Connexion WiFi -------------------
void connectToWiFi() {
  Serial.print("Connexion Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n✅ Connecté au Wi-Fi avec l’IP : " + WiFi.localIP().toString());
}

// ------------------- Connexion MQTT -------------------
void connectToMQTT() {
  while (!mqtt_client.connected()) {
    Serial.printf("\nTentative de connexion MQTT avec l'ID : %s\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("✅ Connecté au broker MQTT");
    } else {
      Serial.print("❌ Échec MQTT, code : ");
      Serial.println(mqtt_client.state());
      delay(5000);
    }
  }
}

void verifierRC522() {
  if (mfrc522.PICC_IsNewCardPresent() == false && mfrc522.PCD_ReadRegister(mfrc522.VersionReg) == 0x00) {
    Serial.println("RC522 inactif, tentative de réinitialisation...");
    mfrc522.PCD_Init();  // Réinitialise le RC522
  }
}
