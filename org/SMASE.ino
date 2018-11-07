/*
 * Ziel des SMASE Sensors ist es, die Temperatur am Pufferspeicher an drei Höhen auszulesen, sowie die Temperatur am Heizungsvorlauf zu messen.  
 * Die Temperaturen werden alle 64 Sekunden ausgelesen und ausgegeben. Auf Wunsch können die Daten an einen Thingspeak Account übertragen werden.
 * 
 * Der Sensor startet, nachdem er sich 3 Minten nicht in einem Netzwerk anmelden kann, als Server. Nachdem man sich mit dem Server verbunden hat,
 * kann man die SSID und Passwort des eigenen Netzwerks eingeben und der Sensor startet und und verbindet sich. 
 * 
 * Arduino IDE 1.6.5 R5
 * NodeMCU V3 - ESP-12E
 * http://arduino.esp8266.com/staging/package_esp8266com_index.json
 * 
 * Tobias Winter - 08.02.2016
*/




#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <EEPROM.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include "main.h"
#include <OneWire.h>
#include "DallasTemperature.h"

void setup() {
  Serial.begin(115200);
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifi;

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifi.setTimeout(180);
  
  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "SMASE Config"
  //and goes into a blocking loop awaiting configuration
  if(!wifi.autoConnect("SMASE Config")) {
    Serial.println("failed to connect and hit timeout");
    delay(5000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

 mainsetup();
}

void loop() {
  // put your main code here, to run repeatedly:
  mainloop();
}
