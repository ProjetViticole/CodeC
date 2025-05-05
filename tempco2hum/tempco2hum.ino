#include <DHT.h>
#include "Air_Quality_Sensor.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ---------- Brochage des capteurs ----------
#define DHTPIN 13
#define DHTTYPE DHT22
#define AIR_QUALITY_PIN 34

DHT dht(DHTPIN, DHTTYPE);
AirQualitySensor airSensor(AIR_QUALITY_PIN);

// ---------- Réseau Wi-Fi ----------
const char* ssid = "Adam";
const char* password = "123456789";

// ---------- MQTT sécurisé ----------
const char *mqtt_broker = "tebebeeb.ala.eu-central-1.emqxsl.com";
const char *mqtt_username = "BTSESP";
const char *mqtt_password = "123";
const int mqtt_port = 8883;
String client_id = "esp32-capteurs-seuils";

// Topics MQTT
const char *topic_temp = "esp32/temp";
const char *topic_hum = "esp32/hum";
const char *topic_air = "esp32/air";

// Certificat SSL (raccourci ici, remplace par le tien)
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

// ---------- Objets Wi-Fi / MQTT ----------
WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

// ---------- Seuils critiques ----------
const float TEMP_SEUIL = 30.0;   // température en °C
const int AIR_SEUIL = 600;       // valeur analogique du capteur air

// ---------- Minuterie pour envoi régulier ----------
unsigned long lastSend = 0;
const unsigned long interval = 1 * 60 * 1000; // 10 minutes en ms

// ---------- Connexion Wi-Fi ----------
void connectToWiFi() {
  Serial.print("Connexion au Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n✅ Connecté au Wi-Fi !");
}

// ---------- Connexion MQTT ----------
void connectToMQTT() {
  while (!mqtt_client.connected()) {
    Serial.printf("Connexion au broker MQTT avec ID : %s\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("✅ Connecté au broker MQTT !");
    } else {
      Serial.print("❌ Erreur MQTT : ");
      Serial.println(mqtt_client.state());
      delay(5000);
    }
  }
}

// ---------- Initialisation ----------
void setup() {
  Serial.begin(115200);
  dht.begin();
  delay(100);

  // Initialisation capteur de qualité de l’air
  Serial.println("Initialisation du capteur de qualité de l'air...");
  delay(20000); // préchauffage obligatoire
  if (airSensor.init()) {
    Serial.println("Capteur air prêt.");
  } else {
    Serial.println("⚠️ Erreur capteur air !");
  }

  // Connexions réseau
  connectToWiFi();
  espClient.setCACert(ca_cert);
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  connectToMQTT();
}

// ---------- Boucle principale ----------
void loop() {
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  if (!mqtt_client.connected()) connectToMQTT();
  mqtt_client.loop();

  static unsigned long lastRead = 0;
  static const unsigned long readInterval = 5000; // lecture toutes les 5 sec
  unsigned long now = millis();

  if (now - lastRead >= readInterval) {
    lastRead = now;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int airRaw = airSensor.getValue();
    int airQuality = airSensor.slope();
    float airPPM = airRaw * 1.5; // estimation ppm après avoir airRaw

    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.print(" °C | Hum: ");
    Serial.print(humidity);
    Serial.print(" % | Air: ");
    Serial.println(airRaw);

    bool humidityValide = !isnan(humidity);
    bool temperatureValide = !isnan(temperature);

    bool seuilDepasse = (
      (temperatureValide && temperature > TEMP_SEUIL) ||
      airRaw > AIR_SEUIL ||
      (humidityValide && (humidity < 20 || humidity > 80))
    );

    if ((now - lastSend >= interval) || seuilDepasse) {
      lastSend = now;

      if (temperatureValide) {
        String payload = "temperature,device=esp32 value=" + String(temperature, 2);
        mqtt_client.publish(topic_temp, payload.c_str());
        Serial.println("📤 Température envoyée");
      }

      if (humidityValide) {
        String payload = "humidity,device=esp32 value=" + String(humidity, 2);
        mqtt_client.publish(topic_hum, payload.c_str());
        Serial.println("📤 Humidité envoyée");
      }

      String airDesc;
      if (airQuality == AirQualitySensor::FORCE_SIGNAL) {
        airDesc = "Pollution sévère";
      } else if (airQuality == AirQualitySensor::HIGH_POLLUTION) {
        airDesc = "Pollution élevée";
      } else if (airQuality == AirQualitySensor::LOW_POLLUTION) {
        airDesc = "Pollution faible";
      } else if (airQuality == AirQualitySensor::FRESH_AIR) {
        airDesc = "Air frais";
      } else {
        airDesc = "Indéterminé";
      }

      String payload = "air_quality,device=esp32 value=" + String(airRaw, 2) +
                       ",ppm=" + String(airPPM, 2) +
                       ",description=\"" + airDesc + "\"";
      mqtt_client.publish(topic_air, payload.c_str());
      Serial.println("📤 Qualité de l'air envoyée : " + airDesc);

      if (seuilDepasse) {
        Serial.println("⚠️ Seuil dépassé ! Envoi anticipé effectué.");
      }
    }

    // Description qualité d'air
    if (airQuality == AirQualitySensor::FORCE_SIGNAL) {
      Serial.println("💨 Pollution sévère !");
    } else if (airQuality == AirQualitySensor::HIGH_POLLUTION) {
      Serial.println("🌫️ Pollution élevée");
    } else if (airQuality == AirQualitySensor::LOW_POLLUTION) {
      Serial.println("🌤️ Pollution faible");
    } else if (airQuality == AirQualitySensor::FRESH_AIR) {
      Serial.println("🍃 Air frais");
    } else {
      Serial.println("❓ Qualité d'air indéterminée");
    }

    Serial.println("-----------------------------");
  }
}


