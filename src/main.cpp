#include <Wire.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define SEALEVELPRESSURE_HPA (1013.25)

// 20 másodperc van csatlakozni a WIFI-hez
#define WIFI_TIMEOUT (20 * 1000)

// 3 perc deep sleep
#define DEEPSLEEP_TIMEOUT (3 * 60 * 1000000)

boolean error = false;

Adafruit_BME280 bme280;
OneWire oneWire(D1);
DallasTemperature ds18b20(&oneWire);
DeviceAddress deviceAddress;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

#define TEMPERATURE_PRECISION 12

#include "settings.h"
//
// A settings.h tartalma: (a settings.h nincs felcommitolva):
//
// #ifndef homero1_settings_h
// #define homero1_settings_h
//
// const char* ssid = "...";
// const char* password = "...";
// const char* mqtt_server = "...";
// const int mqtt_port = ...;
// const char* mqtt_client = "...";
// const char* mqtt_user = "...";
// const char* mqtt_password = "...";
// const char* mqtt_topic = "...";
//
// #endif
//

float temperature1 = 0;
float temperature2 = 0;
float pressure = 0;
float altitude = 0;
float humidity = 0;

void setupSerial()
{
        if (!error)
        {
                Serial.begin(74880);
                delay(5000);
                Serial.println();
                Serial.println();
        }
}

void setupBME280()
{
        if (!error)
        {
                Serial.println(F("A BME280 inicializálás."));
                Wire.begin(D3, D4);
                //Wire.setClock(100000);
                if (!bme280.begin(0x76)) {
                        Serial.println(F("A BME280 szenzor nem található."));
                        error = true;
                }
        }
}

void setupDS18B20()
{
        if (!error)
        {
                Serial.println(F("A DS18B20 inicializálás."));

                // byte i;
                // byte addr[8];
                // if (oneWire.search(addr)) {
                //         Serial.print(" ROM =");
                //         for (i = 0; i < 8; i++) {
                //                 Serial.write(' ');
                //                 Serial.print(addr[i], HEX);
                //         }
                //         oneWire.reset_search();
                // }
                // Serial.println();

                ds18b20.begin();
                if (ds18b20.getDeviceCount() == 0)
                {
                        Serial.println(F("A DS18B20 szenzor nem található."));
                        error = true;
                }
                else
                {
                        ds18b20.getAddress(deviceAddress, 0);
                        ds18b20.setResolution(deviceAddress, TEMPERATURE_PRECISION);
                }
        }
}

void setupWifi()
{
        if (!error)
        {
                Serial.print(F("Kapcsolódás: "));
                Serial.println(ssid);

                WiFi.forceSleepWake();
                delay(1);

                WiFi.persistent(false);
                WiFi.mode(WIFI_STA);
                WiFi.begin(ssid, password);

                uint64 time = millis();
                while (WiFi.status() != WL_CONNECTED || time + WIFI_TIMEOUT < millis()) {
                        delay(500);
                        Serial.print(".");
                }
                if (WiFi.status() != WL_CONNECTED)
                {
                        Serial.print(F("WiFi kapcsolat létrehozása nem sikerült."));
                        error = true;
                }
                else
                {
                        Serial.println();
                        Serial.print(F("WiFi kapcsolat létrehozva: "));
                        Serial.println(WiFi.localIP());
                }
        }
}

void setupMqtt()
{
        if (!error)
        {
                Serial.print(F("Csatlakozás az MQTT szerverhez: "));
                Serial.print(mqtt_server);
                Serial.print(F(":"));
                Serial.println(mqtt_port);
                mqttClient.setServer(mqtt_server, mqtt_port);
                if(!mqttClient.connected())
                {
                        if (mqttClient.connect(mqtt_client, mqtt_user, mqtt_password))
                        {
                                Serial.println(F("Csatlakozva az MQTT szerverhez."));
                        }
                        else
                        {
                                Serial.print(F("A csatlakozás az MQTT szerverhez nem sikerült: "));
                                Serial.println(mqttClient.state());
                                error = true;
                        }
                }
                else
                {
                        Serial.println(F("MQTT: már csatlakozva van."));
                }
        }
}

void publish(char* payload, int length)
{
        if (mqttClient.connected())
        {
                Serial.print(F("MQTT: adatok küldése: "));
                Serial.print(mqtt_topic);
                Serial.print(F(" = "));
                Serial.println(payload);
                if (mqttClient.publish(mqtt_topic, payload))
                {
                        Serial.println(F("MQTT: az adatok elküldése sikerült."));
                }
                else
                {
                        Serial.println(F("MQTT: az adatok küldése nem sikerült."));
                }
        }
        else
        {
                Serial.println(F("Az MQTT kliens nincs csatlakozva, az adatok küldése ezért nem sikerült."));
        }
}

float readDS18B20Temperature()
{
        float temp;
        for (;;)
        {
                ds18b20.requestTemperatures();
                temp = ds18b20.getTempCByIndex(0);
                if (temp == 85.0 || temp == -127.0)
                {
                        delay(100);
                }
                else
                {
                        break;
                }
        }
        return temp;
}

void readSensorValues()
{
        Serial.println(F("Mérés."));
        temperature1 = readDS18B20Temperature();
        temperature2 = bme280.readTemperature();
        pressure = bme280.readPressure() / 100.0F;
        altitude = bme280.readAltitude(SEALEVELPRESSURE_HPA);
        humidity = bme280.readHumidity();
}

void sendValues()
{
        StaticJsonDocument<128> doc;
        JsonObject root = doc.to<JsonObject>();
        root["t1"] = temperature1;
        root["t2"] = temperature2;
        root["p"] = pressure;
        root["a"] = altitude;
        root["h"] = humidity;

        char json[200];
        int length = measureJson(doc) + 1;
        serializeJson(doc, json, length);

        publish(json, length);
}

void deepSleep()
{
        Serial.println(F("Alvás."));
        Serial.println();

        mqttClient.disconnect();
        WiFi.forceSleepBegin();
        delay(1);
        WiFi.disconnect(true);
        delay(1);
        ESP.deepSleep(DEEPSLEEP_TIMEOUT, WAKE_RF_DISABLED);
}

void setup()
{
        WiFi.mode(WIFI_OFF);
        WiFi.forceSleepBegin();
        delay(1);

        setupSerial();
        setupBME280();
        setupDS18B20();

        if (error)
        {
                Serial.println("Hiba történt inicializálás közben.");
        }
        else
        {
                readSensorValues();
                delay(500);
                readSensorValues();
                delay(500);
                readSensorValues();

                setupWifi();
                setupMqtt();
                if (error)
                {
                        Serial.println(F("Hiba történt inicializálás közben."));
                }
                else
                {
                        sendValues();
                }
        }

        deepSleep();
}

void loop()
{
        //
}
