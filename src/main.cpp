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
#define PIN_PH 35
#define PIN_ONE_WIRE_BUS 5
#define PIN_WATER_LV 4
#define PIN_BTN_RECONFIG_WIFI 21

GyverOLED<SSH1106_128x64> display;
AsyncWebServer server(80);
FirebaseData fbdo;
Servo servo;
OneWire oneWire(PIN_ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

const float esp32ADCMultipiler = (3.3 / 4095.0);

float sensor_read_tb = 0;
float sensor_read_temp = 0;
int servo_pos = 0;
float sensor_ph = 0;
int sensor_water_level = 1;

long phTot;
float phAvg;
int x;

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
  json.clear();
}
void doUpdateSensorValue()
{
  long ts = getTime();

  FirebaseJson current_sensor_val_json;
  current_sensor_val_json.add("tb", sensor_read_tb);
  current_sensor_val_json.add("temp", sensor_read_temp);
  current_sensor_val_json.add("water_level", sensor_water_level);
  current_sensor_val_json.add("ph", sensor_ph);
  current_sensor_val_json.add("ts", ts);
  FirebaseData _fbdo;
  Firebase.updateNodeAsync(fbdo, "/current_sensor_value", current_sensor_val_json);
  Firebase.end(_fbdo);
  current_sensor_val_json.clear();

  if (false)
  {

    delay(100);

    FirebaseJson tb_val_json;
    FirebaseData _fbdo_tb;
    tb_val_json.add("value", sensor_read_tb);
    tb_val_json.add("ts", ts);
    Firebase.pushAsync(_fbdo_tb, "/sensor/tb", tb_val_json);
    Firebase.end(_fbdo_tb);

    delay(100);

    FirebaseJson temp_val_json;
    FirebaseData temp_fbdo;
    temp_val_json.add("value", sensor_read_tb);
    temp_val_json.add("ts", ts);
    Firebase.pushAsync(temp_fbdo, "/sensor/temp", temp_val_json);
    Firebase.end(temp_fbdo);

    delay(100);

    FirebaseJson wl_val_json;
    FirebaseData wl_fbdo;
    wl_val_json.add("value", sensor_water_level);
    wl_val_json.add("ts", ts);
    Firebase.pushAsync(wl_fbdo, "/sensor/water_level", temp_val_json);
    Firebase.end(wl_fbdo);

    delay(100);

    FirebaseJson ph_val_json;
    FirebaseData ph_fbdo;
    ph_val_json.add("value", sensor_ph);
    ph_val_json.add("ts", ts);
    Firebase.pushAsync(ph_fbdo, "/sensor/ph", ph_val_json);
    Firebase.end(ph_fbdo);

    delay(100);
  }
}
void doSyncTime()
{
  const char *ntpServer = "ntp.ku.ac.th";
  configTime(7 * 3600, 0, ntpServer);
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
    doUpdateDeviceInfo();
    vTaskDelay(3000);
  }
}

void oledDisplayTask(void *pvParameters)
{
  while (1)
  {

    struct tm t;
    if (!getLocalTime(&t))
    {
      vTaskDelay(500);
      continue;
    }

    char buff[80];

    sprintf(buff, "%02d/%02d/%02d %02d:%02d:%02d",
            1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

    display.setCursorXY(1, 0);
    display.print(String(buff));

    // display.clear(1, 23, 0, 0);
    display.setCursorXY(1, 23 + 3);
    display.print("Temp: " + String(sensor_read_temp) + " *C");
    // display.clear(1, 36, 0, 0);
    display.setCursorXY(1, 36 + 4);
    display.print("Tb: " + String(sensor_read_tb) + " v");
    // display.clear(1, 49, 0, 0);
    display.setCursorXY(1, 49 + 3);
    display.print("pH: " + String(sensor_ph));
    display.update();

    vTaskDelay(1000);
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

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String getFeedConfig(int indx)
{
  String s = "";
  FirebaseData _fbdo;
  if (Firebase.getString(_fbdo, "/feed_config/" + String(indx)))
  {
    s = _fbdo.to<String>();
  }
  Firebase.end(_fbdo);
  return s;
}

String getFeedConfigLastRun(int indx)
{
  String s = "";
  FirebaseData _fbdo;
  if (Firebase.getString(_fbdo, "/feed_config_last_run/" + String(indx)))
  {
    s = _fbdo.to<String>();
  }
  Firebase.end(_fbdo);
  return s;
}

void updateFeedConfigLastRun(int indx, String s)
{

  FirebaseData _fbdo;
  FirebaseJson patch;
  patch.add(String(indx), s);
  Firebase.updateNode(_fbdo, "/feed_config_last_run", patch);
  Firebase.end(_fbdo);
}

void doListenFeed()
{
  FirebaseData _fbdo;
  if (Firebase.getJSON(_fbdo, "/feed_control"))
  {
    FirebaseJsonData result;
    _fbdo.jsonObject().get(result, "value");
    if (result.success)
    {
      int v = result.to<int>();
      if (v == 1)
      {

        FirebaseJson patch;
        patch.add("value", 0);
        patch.add("ts", getTime());
        Firebase.updateNode(_fbdo, "/feed_control", patch);
        doFeed();
      }
    }
    Firebase.end(_fbdo);
  }
}
void listenFeed(void *pvParameters)
{
  while (1)
  {
    doListenFeed();
  }
  vTaskDelay(1000);
}

void listenFeedConfig(void *pv)
{
  while (1)
  {

    doListenFeed();

    struct tm t;
    if (!getLocalTime(&t))
    {
      vTaskDelay(100);
      continue;
    }

    char buff[80];

    for (int i = 1; i < 4; i++)
    {
      String s1 = getFeedConfig(i);
      if (s1 != "")
      {
        int s1HH = getValue(s1, ':', 0).toInt();
        int s1MM = getValue(s1, ':', 1).toInt();

        String s1r = getFeedConfigLastRun(i);

        sprintf(buff, "%02d/%02d/%02d %02d:%02d:%02d",
                1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, s1HH, s1MM, 0);

        if (s1r != String(buff) && t.tm_hour == s1HH && t.tm_min == s1MM)
        {
          updateFeedConfigLastRun(i, String(buff));
          doFeed();
        }
      }
      delay(100);
    }
    vTaskDelay(200);
  }
}

void setup()
{

  log_d("Used PSRAM: %d", ESP.getPsramSize() - ESP.getFreePsram());
  byte *psdRamBuffer = (byte *)ps_malloc(500000);
  log_d("Used PSRAM: %d", ESP.getPsramSize() - ESP.getFreePsram());
  free(psdRamBuffer);
  log_d("Used PSRAM: %d", ESP.getPsramSize() - ESP.getFreePsram());

  Serial.begin(9600);
  Serial.println("begin setup device");
  Serial.println();

  display.init();
  display.setScale(1);
  display.autoPrintln(true);
  display.home();
  display.println("GPIO pin initialize");
  display.update();

  pinMode(PIN_LED_INTERNAl, OUTPUT);
  pinMode(PIN_TBSENSOR, INPUT);
  pinMode(PIN_WATER_LV, INPUT);
  pinMode(PIN_BTN_RECONFIG_WIFI, INPUT);
  servo.attach(PIN_SERVO);

  delay(500);

  for (int i = 0; i < 5; i++)
  {
    if (!ESPConnect.isConfigured())
    {
      break;
    }

    digitalWrite(PIN_LED_INTERNAl, HIGH);
    delay(100);
    digitalWrite(PIN_LED_INTERNAl, LOW);
    if (digitalRead(PIN_BTN_RECONFIG_WIFI) == 0)
    {
      display.clear();
      display.home();

      display.update();
      delay(500);
      ESPConnect.erase();
      digitalWrite(PIN_LED_INTERNAl, LOW);
      delay(100);
      digitalWrite(PIN_LED_INTERNAl, HIGH);
      delay(5000);
      display.clear();
      break;
    }

    delay(1000);
  }

  delay(500);

  if (!ESPConnect.isConfigured())
  {
    display.clear();
    display.println("WIFI-CONFIG MODE !!!");
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

    // xTaskCreatePinnedToCore(
    //     autoUpdateDeviceInfo, "autoUpdateDeviceInfo", 10000, NULL, 0, NULL, 0);

    xTaskCreatePinnedToCore(
        autoUpdateSensorValue, "autoUpdateSensorValue", 10000, NULL, 0, NULL, 0);

    display.println("done.");
    display.update();
    delay(1000);

    display.clear();
    display.autoPrintln(false);

    xTaskCreatePinnedToCore(
        oledDisplayTask, "oledDisplayTask", 5000, NULL, 0, NULL, 1);

    // xTaskCreatePinnedToCore(
    //     listenFeed, "listenFeed", 10000, NULL, 0, NULL, 1);

    xTaskCreatePinnedToCore(
        listenFeedConfig, "listenFeedConfig", 10000, NULL, 0, NULL, 1);

    display.clear(1, 13, 0, 0);
    display.setCursorXY(1, 13);
    display.print("wifi-ssid: " + String(ESPConnect.getSSID()));
    // display.println("IP: " + WiFi.localIP().toString());
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
  int tbAvg = 0;
  for (x = 0; x < 10; x++)
  {
    int v = analogRead(PIN_TBSENSOR);
    tbAvg += v;
    delay(10);
  }
  if (tbAvg > 0)
  {
    tbAvg = tbAvg / 10;
    sensor_read_tb = tbAvg * esp32ADCMultipiler;
  }

  tempSensors.requestTemperatures();
  sensor_read_temp = tempSensors.getTempCByIndex(0);

  int phAvg = 0;

  // taking 10 sample and adding with 10 milli second delay
  for (x = 0; x < 10; x++)
  {
    int v = analogRead(PIN_PH);
    phAvg += v;
    delay(10);
  }
  phAvg = phAvg / 10;
  float phVoltage = phAvg * esp32ADCMultipiler;
  float pHValue = 3.3 * phVoltage;
  sensor_ph = pHValue;

  sensor_water_level = digitalRead(PIN_WATER_LV);

  delay(1000);
}