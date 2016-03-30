# SMASE
Pufferspeicher Anzeige

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

 * Changelog: 
 * 29.03.2016 | Alle Temperaturen werden auf einer Seite angezeigt. Das Datum und die Uhrzeit aktualisiert sich bei jedem Temperaturaufruf.Die Sommerzeit muss in den Einstellungen geändert werden.
 
 * 30.03.2016 | gemessene Temperaturen werden geprüft ob Werte plausibel sind, wenn nein wird nochmal gemessen bis ein plausibler Wert erscheint.(max. 5 Wiederholungen)