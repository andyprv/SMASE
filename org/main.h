#ifndef main_h
#define main_h

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include "time_ntp.h"
#include <OneWire.h>
#include "DallasTemperature.h"

void WiFiStart();

extern "C"
{
#include "user_interface.h"
}

void mainsetup();

void mainloop();

#endif

