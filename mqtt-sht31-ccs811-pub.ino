//
// mqtt-sht31-ccs811.ino - sample sketch for SHT31 & CCS811
//
// Requirements:
//
//   Akiduki ESP-WROOM-02 development board
//     http://akizukidenshi.com/catalog/g/gK-12236/
//
//   SparkFun Air Quality Breakout - CCS811
//     https://www.sparkfun.com/products/14193
//
//   Akiduki SHT31 module
//     http://akizukidenshi.com/catalog/g/gK-12125/
//
//   Arduino Client for MQTT
//     https://github.com/knolleary/pubsubclient/
//
//   Adafruit_SHT31
//     https://github.com/adafruit/Adafruit_SHT31
//
// How to use:
//
//     $ git clone https://github.com/yoggy/mqtt-sht31-ccs811.git
//     $ cd mqtt-sht31-ccs811
//     $ cp config.ino.sample config.ino
//     $ vi config.ino
//       - edit wifi_ssid, wifi_password, mqtt_server, mqtt_publish_topic, ... etc
//     $ open mqtt-sht31-ccs811.ino
//
// license:
//     Copyright (c) 2018 yoggy <yoggy0@gmail.com>
//     Released under the MIT license
//     http://opensource.org/licenses/mit-license.php;
//
#include <ESP8266WiFi.h>
#include <PubSubClient.h>   // https://github.com/knolleary/pubsubclient/
#include <Wire.h>

// https://github.com/AmbientDataInc/Ambient_AirQuality/tree/master/examples/CCS811_test
#include "SparkFunCCS811.h"

// Adafruit_SHT31
#include <Adafruit_SHT31.h> //https://github.com/adafruit/Adafruit_SHT31
Adafruit_SHT31 sht31 = Adafruit_SHT31();

#define PIN_SDA 14
#define PIN_SCL 12
#define PIN_CCS811_RST 13

void reboot() {
  Serial.println("REBOOT!!!!!");
  delay(1000);

  ESP.reset();

  while (true) {
    Serial.println("REBOOT!!!!!");
    delay(500);
  };
}

//////////////////////////////////////////////////////////////////////

unsigned long last_updated_t;

void clear_time() {
  last_updated_t = millis();
}

unsigned long diff_time() {
  return millis() - last_updated_t;
}

//////////////////////////////////////////////////////////////////////

#define CCS811_ADDR 0x5B
CCS811 ccs811(CCS811_ADDR);

void reset_ccs811() {
  pinMode(PIN_CCS811_RST, OUTPUT);
  digitalWrite(PIN_CCS811_RST, LOW);
  delay(100);
  digitalWrite(PIN_CCS811_RST, HIGH);
  delay(100);
}

void setup_ccs811() {
  Serial.println("setup_ccs811() : start");

  reset_ccs811();

  CCS811Core::status rv = ccs811.begin();
  if (rv != CCS811Core::SENSOR_SUCCESS) {
    Serial.println("setup_ccs811() : ccs811.begin() failed...");
    reboot();
  }

  ccs811.setDriveMode(1); // read every 1sec
  clear_time();
  Serial.println("setup_ccs811() : success");
}

//////////////////////////////////////////////////////////////////////

// Wif config (from config.ino)
extern char *wifi_ssid;
extern char *wifi_password;
extern char *mqtt_server;
extern int  mqtt_port;

extern char *mqtt_client_id;
extern bool mqtt_use_auth;
extern char *mqtt_username;
extern char *mqtt_password;

extern char *mqtt_publish_topic;

WiFiClient wifi_client;
PubSubClient mqtt_client(mqtt_server, mqtt_port, NULL, wifi_client);

void setup_wifi()
{
  Serial.println("setup_wifi() : start");

  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.mode(WIFI_STA);

  int wifi_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    wifi_count ++;
    delay(300);
    if (wifi_count > 100) reboot();
  }

  Serial.println("setup_wifi() : wifi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("setup_wifi() : mqtt connecting");
  bool rv = false;
  if (mqtt_use_auth == true) {
    rv = mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password);
  }
  else {
    rv = mqtt_client.connect(mqtt_client_id);
  }
  if (rv == false) {
    Serial.println("setup_wifi() : mqtt connecting failed...");
    reboot();
  }
  Serial.println("setup_wifi() : mqtt connected");

  Serial.println("setup_wifi() : success");
}

/////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);

  Wire.begin(PIN_SDA, PIN_SCL); //SDA, SCL
  Wire.setClockStretchLimit(30000);  // !!!! ATTENTION : The CCS811 clock stretch is very short time. !!!!

  setup_wifi();
  setup_ccs811();
  sht31.begin(0x45);
}

void loop() {
  if (!mqtt_client.connected()) {
    Serial.println("MQTT disconnected...");
    reboot();
  }
  mqtt_client.loop();

  // every 1sec
  if (diff_time() > 1000) {
    if (ccs811.dataAvailable()) {
      float t = sht31.readTemperature();
      float h = sht31.readHumidity();

      ccs811.setEnvironmentalData(h, t);
      ccs811.readAlgorithmResults();

      uint16_t co2 = ccs811.getCO2();
      uint16_t tvoc = ccs811.getTVOC();

      publish_message(t, h, co2, tvoc);

      clear_time();
    }
  }

  if (diff_time() > 7000) {
    Serial.println("timeout...");
    reboot();
  }
}

void publish_message(float temperature, float humidity, uint32_t co2, uint32_t tvoc) {
  volatile int16_t t = (int16_t)temperature;
  volatile int16_t h = (int16_t)humidity;

  char msg[128];
  memset(msg, 0, 128);
  snprintf(msg, 128, "{\"tmp\":%d,\"hum\":%d,\"co2\":%u,\"tvos\":%u}",
           t,
           h,
           co2,
           tvoc
           );
           
  Serial.print("mqtt_publish : ");
  Serial.println(msg);

  mqtt_client.publish(mqtt_publish_topic, msg, true);
}

