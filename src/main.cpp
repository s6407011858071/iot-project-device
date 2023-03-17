#include <Arduino.h>
#include <GyverOLED.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPConnect.h>
#include <FirebaseESP32.h>
#include <Time.h>
#include <ESP32Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define FIREBASE_HOST "iot-final-project-aec6d-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "gPMmTjw8LoAeEtj1EMT1EvWVpWdEnxSSk2BPXexm"
#define DEVICE_SSID_NAME ".@fish-tank"

#define PIN_LED_INTERNAl 2
#define PIN_TBSENSOR 34
#define PIN_SERVO 23

#define PIN_ONE_WIRE_BUS 5

GyverOLED<SSH1106_128x64> display;
AsyncWebServer server(80);
FirebaseData fbdo;
Servo servo;
OneWire oneWire(PIN_ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

float sensor_read_tb = 0;
float sensor_read_temp = 0;
int servo_pos = 0;

unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return (0);
  }
  time(&now);
  return now;
}
void doUpdateDeviceInfo()
{
  FirebaseJson json;
  json.add("ip_address", String(WiFi.localIP().toString()));
  json.add("ssid", String(ESPConnect.getSSID()));
  json.add("device_time", getTime());
  json.add("device_model", ESP.getChipModel());
  json.add("device_n_core", ESP.getChipCores());
  json.add("device_wifi_mac", WiFi.macAddress());
  json.add("device_psram", ESP.getFreePsram());
  json.add("device_psram_size", ESP.getPsramSize());
  json.add("device_heap", ESP.getFreeHeap());
  json.add("device_heap_size", ESP.getHeapSize());
  FirebaseData _fbdo;
  Firebase.updateNodeAsync(fbdo, "/device_info", json);
  Firebase.end(_fbdo);
}
void doUpdateSensorValue()
{
  long ts = getTime();

  FirebaseJson current_sensor_val_json;
  current_sensor_val_json.add("tb", sensor_read_tb);
  current_sensor_val_json.add("temp", sensor_read_temp);
  FirebaseData _fbdo;
  Firebase.updateNodeAsync(fbdo, "/current_sensor_value", current_sensor_val_json);
  Firebase.end(_fbdo);

  FirebaseJson tb_val_json;
  FirebaseData _fbdo_tb;
  tb_val_json.add("value", sensor_read_tb);
  tb_val_json.add("ts", ts);
  Firebase.pushAsync(_fbdo_tb, "/sensor/tb", tb_val_json);
  Firebase.end(_fbdo_tb);

  FirebaseJson temp_val_json;
  FirebaseData temp_fbdo;
  temp_val_json.add("value", sensor_read_tb);
  temp_val_json.add("ts", ts);
  Firebase.pushAsync(temp_fbdo, "/sensor/temp", temp_val_json);
  Firebase.end(temp_fbdo);
}
void doSyncTime()
{
  const char *ntpServer = "pool.ntp.org";
  configTime(0, 0, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
void autoUpdateDeviceInfo(void *pvParameters)
{
  while (1)
  {
    doUpdateDeviceInfo();
    vTaskDelay(5000);
  }
}
void autoUpdateSensorValue(void *pvParameters)
{
  while (1)
  {
    doUpdateSensorValue();
    vTaskDelay(10000);
  }
}
void doFeed()
{
  if (!servo.attached())
  {
    return;
  }
  servo.writeMicroseconds(0);
  delay(500);
  servo.writeMicroseconds(3000);
  delay(500);
  servo.writeMicroseconds(0);
}

void setup()
{
  Serial.begin(9600);
  Serial.println("begin setup device");
  Serial.println();

  display.init();
  display.setScale(1);
  display.autoPrintln(true);
  display.home();
  display.println("GPIO pin initialize");

  pinMode(PIN_LED_INTERNAl, OUTPUT);
  pinMode(PIN_TBSENSOR, INPUT);
  servo.attach(PIN_SERVO);

  delay(500);

  display.update();
  display.println("wifi initialize");
  display.update();

  delay(500);

  if (!ESPConnect.isConfigured())
  {
    display.clear();
    display.println("=== wifi connection mode ===");
    display.println("please connnect to wifi");
    display.println(String("SSID: ") + String(DEVICE_SSID_NAME));
    display.update();
  }

  ESPConnect.autoConnect(DEVICE_SSID_NAME);
  display.println("network initialize");
  display.update();
  delay(500);

  if (ESPConnect.begin(&server))
  {

    display.println("sync device time");
    display.update();
    delay(500);
    doSyncTime();

    display.println("firebase initialize");
    display.update();
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    delay(500);

    display.println("task initialize");
    display.update();
    xTaskCreatePinnedToCore(
        autoUpdateDeviceInfo, "autoUpdateDeviceInfo", 10000, NULL, 0, NULL, 0);

    xTaskCreatePinnedToCore(
        autoUpdateSensorValue, "autoUpdateSensorValue", 10000, NULL, 0, NULL, 0);

    display.println("done.");
    display.update();
    delay(1000);

    display.clear();
    display.home();
    display.println("SSID: " + String(ESPConnect.getSSID()));
    display.println();
    display.println("IP: " + WiFi.localIP().toString());
    display.update();
    Serial.println("Connected to WiFi");
    Serial.println(String("IPAddress: ") + String(WiFi.localIP().toString()));
  }
  else
  {
    Serial.println("Failed to connect to WiFi");
  }

  server.on("/", HTTP_GET, [&](AsyncWebServerRequest *request)
            {
              request->send(200, "text/plain", "Hello from ESP");
              //
            });

  server.on("/restart", HTTP_GET, [&](AsyncWebServerRequest *request)
            {
              request->send(200, "text/plain", "ok");
              delay(500);
              ESP.restart();
              //
            });

  server.on("/feed", HTTP_GET, [&](AsyncWebServerRequest *request)
            {
              doFeed();
              request->send(200, "text/plain", "ok");
              //
            });

  server.on("/reconfig", HTTP_GET, [&](AsyncWebServerRequest *request)
            {
              digitalWrite(PIN_LED_INTERNAl, HIGH);
              delay(100);
              digitalWrite(PIN_LED_INTERNAl, LOW);
              delay(100);
              digitalWrite(PIN_LED_INTERNAl, HIGH);
              delay(3000);
              ESPConnect.erase();
              digitalWrite(PIN_LED_INTERNAl, LOW);
              delay(100);
              ESP.restart();
              //
            });

  server.begin();
}

void loop()
{
  int v = analogRead(PIN_TBSENSOR);
  if (v > 0)
  {

    sensor_read_tb = v * (5.0 / 1024.0);
  }

  tempSensors.requestTemperatures();
  sensor_read_temp = tempSensors.getTempCByIndex(0);

  display.clear(1, 30, 0, 0);
  display.setCursorXY(1, 30);
  display.print("Temp: " + String(sensor_read_temp) + " *C");
  display.clear(1, 40, 0, 0);
  display.setCursorXY(1, 40);
  display.print("Tb: " + String(sensor_read_tb) + " v");

  display.update();

  delay(5000);
}