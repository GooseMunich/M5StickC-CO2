#include <M5StickC.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include "DHT12.h"
#include <ArduinoJson.h>
DHT12 dht12;

char response[7];
byte cmd[7] = { 0xFE, 0x44, 0x00, 0x08, 0x02, 0x9F, 0x25 };
int responseHigh;
int responseLow;
int co2;
float tmp;
float hum;
unsigned long timing1;
unsigned long timing2;

//sensor polling time in ms
int sensors = 5000;
//display off time in ms
int dispoff = 300000;

//CO2 sensor
int errorCO2 = 2500;
int warningCO2 = 1000;

//MQTT server
#define mqtt_server "SERVERIP"
#define mqtt_user "USER" 
#define mqtt_password "PASSWORD"

//unique id !!!
#define mqtt_client_id "m5bedroom"
#define co2_topic "m5bedroom/sensor/co2"
#define humidity_topic "m5bedroom/sensor/humidity"
#define temperature_topic "m5bedroom/sensor/temperature"

WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "SSID";
const char* password = "PASSWORD";

void setup(){
  
  M5.begin();
  Wire.begin(0,26);
  Serial.begin(9600, SERIAL_8N1, 33, 32);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation(3);
  M5.Lcd.setCursor(0, 3);
  M5.Lcd.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    M5.Lcd.println(".");
    delay(250);
  }

  M5.Lcd.println("connected");
  M5.Lcd.print("IP address: ");
  M5.Lcd.println(WiFi.localIP());

  // startup mqtt connection
  client.setServer(mqtt_server, 1883);

  pinMode(M5_LED, OUTPUT);
  pinMode(M5_BUTTON_HOME, INPUT);

  delay(5000);

}

void reconnect() {
  while (!client.connected()) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 3);
    M5.Lcd.println("Attempting MQTT connection");
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      M5.Lcd.println("connected");
      delay(5000);
      M5.Lcd.fillScreen(BLACK);
    } else {
      M5.Lcd.println("failed");
      M5.Lcd.print(client.state());
      M5.Lcd.println("try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// the loop routine runs over and over again forever
void loop() {

// Uptime

  M5.Lcd.setCursor(0, 3);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print("Uptime: ");
  
  int time = millis()/1000;
  if (time/60/60<10) { 
    M5.Lcd.print("0");
  }
  M5.Lcd.print(time/60/60);
  M5.Lcd.print(":");
  if (time/60%60<10) {
    M5.Lcd.print("0");
  }
  M5.Lcd.print((time/60)%60);
  M5.Lcd.print(":");
  if (time%60<10) {
    M5.Lcd.print("0");
  }
  M5.Lcd.print(time%60);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

// Sensors check
  
  if ( millis() - timing1 > sensors ) {

    timing1 = millis(); 
    
    int i = 0;
    for (i = 0; i < 7; i++) {
      Serial.write(cmd[i]);
    }

    delay(100);

    i = 0;
    while (Serial.available()) {
      response[i] = Serial.read();
      i++;
    }

    responseHigh = (int) response[3]; // returned High level concentration
    responseLow  = (int) response[4]; // returned Low level concentration
    co2 = (256 * responseHigh) + responseLow;

    tmp = dht12.readTemperature();
    hum = dht12.readHumidity();
    
    M5.Lcd.fillScreen(BLACK);
    
  }

// LED on/off

  if (co2 > warningCO2) {
    digitalWrite(M5_LED, LOW);
  }
  else {
    digitalWrite(M5_LED, HIGH);
  }

// Display off time

  if ( millis() - timing2 > dispoff ) {
    timing2 = millis();
    Wire1.beginTransmission(0x34);
    Wire1.write(0x12);
    Wire1.write(0b01001011);  // LDO2, aka OLED_VDD, off
    Wire1.endTransmission();
  }

// If the display is off and press button M5 then turn on the display 

  if (digitalRead(M5_BUTTON_HOME) == LOW) {
    Wire1.beginTransmission(0x34);
    Wire1.write(0x12);
    Wire1.write(0x4d); // Enable LDO2, aka OLED_VDD
    Wire1.endTransmission();
  }

// Color selection

  if ( co2 <= 600) {
    M5.Lcd.setTextColor(GREEN, BLACK);
  }
  else if ( co2 <= 800) {
    M5.Lcd.setTextColor(YELLOW, BLACK);
  }
  else {
    M5.Lcd.setTextColor(RED, BLACK);
  }

// Display text

  M5.Lcd.setCursor(25, 25);
  M5.Lcd.setTextSize(4);
  M5.Lcd.print(co2);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("ppm");
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 72);
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("Temp: %2.1fC, ", tmp);
  M5.Lcd.printf("hum: %2.0f%%", hum);

// MQTT

  client.publish(co2_topic, String(co2).c_str(), true);
  client.publish(temperature_topic, String(tmp).c_str(), true);
  client.publish(humidity_topic, String(hum).c_str(), true);

// If CO2 > errorCO2 then reset M5

  if (co2 > errorCO2) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.setCursor(0, 3);
    M5.Lcd.println("Value CO2 is too big");
    M5.Lcd.println("Reboot in 60 sec");
    delay(60000);
    ESP.restart();
  }

}
