/*
   Ziel des SMASE Sensors ist es, die Temperatur am Pufferspeicher an drei Höhen auszulesen, sowie die Temperatur am Heizungsvorlauf zu messen.
   Die Temperaturen werden alle 64 Sekunden ausgelesen und ausgegeben. Auf Wunsch können die Daten an einen Thingspeak Account übertragen werden.

   Der Sensor startet, nachdem er sich 3 Minten nicht in einem Netzwerk anmelden kann, als Server. Nachdem man sich mit dem Server verbunden hat,
   kann man die SSID und Passwort des eigenen Netzwerks eingeben und der Sensor startet und und verbindet sich.

   Arduino IDE 1.8.3
   NodeMCU V3 - ESP-12E
   http://arduino.esp8266.com/staging/package_esp8266com_index.json

   Andreas Meier - 18.05.2017
   Tobias Winter - 08.02.2016
*/


/*-----( Import needed libraries )-----*/
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>

#include <EEPROM.h>

#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include <WiFiClient.h>

#include <DNSServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ds2413.h>

// This is required for the Arduino IDE + DS2482
#include <Wire.h>

// This is required for AdaFruit NeoPixel
// #include <Adafruit_NeoPixel.h>

// This is required for Thinger.io IoT Dashboard
// #include <ThingerSmartConfig.h>

// #define USERNAME "your_user_name"
// #define DEVICE_ID "your_device_id"
// #define DEVICE_CREDENTIAL "your_device_credential"

// ThingerSmartConfig thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);




//#include <NTPClient.h>
#include "time_ntp.h"


/*-----( Import 'C' libraries )-----*/
extern "C"
{
#include "user_interface.h"
}
/*-----( Import needed libraries )-----*/





void setup()
{
  Serial.begin(19200);             // baudrate for serial Logger --> 19200
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
  if (!wifi.autoConnect("SMASE Config")) {
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



void loop()
{
  // put your main code here, to run repeatedly:

  // after Startup / Init Done !
  extern boolean INITDone;                // Merker Init / Setup Done

  while ( INITDone )
  {
    mainloop();
  }


}





/*
  ########################################################################
   Program
  ########################################################################

*/

// storage for Measurements; keep some mem free; allocate remainder
#define KEEP_MEM_FREE 10240 // org 10240
#define MEAS_SPAN_H 24 // 24 1 day , 48 - 2days , 96 - 4 days , 120 - 5days 
unsigned long ulMeasCount     = 0;  // values already measured
unsigned long ulNoMeasValues  = 0;  // size of array
unsigned long ulMeasDelta_ms;       // distance to next meas time
unsigned long ulNextMeas_ms;        // next meas time
unsigned long *pulTime;             // array for time points of measurements
int           *pfTemp[16];          // array for temperature measurements

unsigned long LED_D1_ms = 0;        // will store last time LED was updated
unsigned long LED_D2_ms = 0;        // will store last time LED was updated

unsigned long oWirePoll = 0;        // will store last time oneWire was updated

signed long   tslong    = 0;        // temp calcVar ( signed ! )

boolean  LED_D1_State = LOW;        // ledState used to set the LED
boolean  LED_D2_State = LOW;        // ledState used to set the LED
byte     A, B, C, D;

String  BA        = "AUTOMATIK";      // Betriesbart          ( "AUTOMATIK"  "HAND" )
boolean SoPumpe   = 0;                // Merker SolarPumpe    ( 0=Aus   1= Ein  )
boolean INITDone  = 0;                // Merker Init / Setup Done

int sent    = 0;
int sOffset = 0;    // Sensor Offset ( 0= 1..8 / 8= 9..16 )

String  TS_Status = " OK \n";             // Thinkspeak Status Message ( "OK" / "No 1W Sensor found" )

#define myPeriodic 15 //in sec | Thingspeak pub is 15sec

/* 1-Wire with standard PIO Pins
  #define ONE_WIRE_BUS1 13    // NodeMCU - D7 ( GPIO 13 - D7 )
  #define ONE_WIRE_BUS2 12    // NodeMCU - D6 ( GPIO 12 - D5 )
  #define ONE_WIRE_BUS3 4     // NodeMCU - D2 ( GPIO 4  - D2 )
  #define ONE_WIRE_BUS4 14    // NodeMCU - D5 ( GPIO 14 - D5 )
*/

#define LED_D1 5 // NodeMCU - D1 ( GPIO 5  - D1 )
#define LED_D2 4 // NodeMCU - D2 ( GPIO 4  - D2 )

/*-----( Declare objects )-----*/

/* 1-Wire with standard PIO Pins
  OneWire oneWire1(ONE_WIRE_BUS1);
  OneWire oneWire2(ONE_WIRE_BUS2);
  OneWire oneWire3(ONE_WIRE_BUS3);
  OneWire oneWire4(ONE_WIRE_BUS4);
*/

// When instantiated with no parameters, uses I2C address 18
OneWire oneWire1;
// OneWire oneWire2(2) // use address 20 (18+2)


DallasTemperature sensors1(&oneWire1);
//DallasTemperature sensors2(&oneWire2);
//DallasTemperature sensors3(&oneWire3);
//DallasTemperature sensors4(&oneWire4);

ds2413 ow1_ds2413(&oneWire1);
//ds2413 ow2_ds2413(&oneWire2);
//ds2413 ow3_ds2413(&oneWire3);
//ds2413 ow4_ds2413(&oneWire4);


/*-----( Declare objects )-----*/


/*-----( Declare Variables )-----*/
// Assign the addresses of your 1-Wire temp sensors.
// See the tutorial on how to obtain these addresses:
// http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html

// ow1 DS2428-800 ch0 --------
// HeU / HeM / HeO / KaO / KaM / KaU / SoVL / SoRL

DeviceAddress SensorID01 = { 0x28, 0x4C, 0xEF, 0x23, 0x07, 0x00, 0x00, 0x21 };
DeviceAddress SensorID02 = { 0x28, 0xC4, 0xFC, 0x23, 0x07, 0x00, 0x00, 0x58 };
DeviceAddress SensorID03 = { 0x28, 0x5D, 0x7C, 0x24, 0x07, 0x00, 0x00, 0x32 };

DeviceAddress SensorID04 = { 0x28, 0xFF, 0x18, 0x4B, 0x22, 0x16, 0x03, 0x01 };
DeviceAddress SensorID05 = { 0x28, 0xC5, 0xED, 0x23, 0x07, 0x00, 0x00, 0xDE };
DeviceAddress SensorID06 = { 0x28, 0x0A, 0x26, 0x24, 0x07, 0x00, 0x00, 0xB1 };

DeviceAddress SensorID07 = { 0x28, 0xDB, 0x32, 0x24, 0x07, 0x00, 0x00, 0x21 };
DeviceAddress SensorID08 = { 0x28, 0x40, 0xB9, 0x23, 0x07, 0x00, 0x00, 0x55 };

// ow2 DS2428-800 ch1 ----------------
// AbW / Luft / HzVL / HzRL / WpVL / WpRL / T15 / T16

DeviceAddress SensorID09 = { 0x28, 0x7A, 0xDA, 0x23, 0x07, 0x00, 0x00, 0x28 };
DeviceAddress SensorID10 = { 0x28, 0x92, 0xF7, 0x23, 0x07, 0x00, 0x00, 0xB4 };

DeviceAddress SensorID11 = { 0x28, 0x8B, 0x58, 0x24, 0x07, 0x00, 0x00, 0xA3 };
DeviceAddress SensorID12 = { 0x28, 0xAB, 0xA7, 0x24, 0x07, 0x00, 0x00, 0xDC };

DeviceAddress SensorID13 = { 0x28, 0x0D, 0xEA, 0x1E, 0x00, 0x00, 0x80, 0x92 };
DeviceAddress SensorID14 = { 0x28, 0xEA, 0xBF, 0x1E, 0x00, 0x00, 0x80, 0x79 };

DeviceAddress SensorID15 = { 0x28, 0x36, 0x9C, 0x24, 0x07, 0x00, 0x00, 0xD3 };
DeviceAddress SensorID16 = { 0x28, 0xFF, 0xA4, 0xEA, 0x21, 0x16, 0x04, 0x2E };



// ow3 DS2428-800 ch3 ------------------------
DeviceAddress DS2413ID01 = { 0x3A, 0x07, 0x41, 0x0D, 0x00, 0x00, 0x00, 0xA3 };
DeviceAddress DS2413ID02 = { 0x28, 0x5D, 0x7C, 0x24, 0x07, 0x00, 0x00, 0x32 };


int    numberOfDevices0;  // Number of temperature devices found on ow ch0
int    numberOfDevices1;  // Number of temperature devices found on ow ch1
int    numberOfDevices2;  // Number of temperature devices found on ow ch2

float  temp[16];          // create ARRAY 0..16 this variable to store temperature values
String tString;           // tempString

DeviceAddress tempDeviceAddress;    //  use this variable to store a found device address

/*-----( Declare Variables )-----*/
// ddmmyy.ver.name
const String SOFTWAREVERSION = "180517.005.am";

//Variablen EEPROM Konfiguration
unsigned long RebootCNT;              // Reboot counter

unsigned long ulReqcount;
unsigned long ulReconncount;          // how often did we connect to WiFi

// ntp timestamp
unsigned long ulSecs2000_timer = 0;
unsigned long ulSecs2000_bak   = 0;  // save last Timestamp
String NoNTP;                        // NTP request was not successfull ( debug )

// Create an instance of the server on Port 80
WiFiServer server(80);
WiFiClient client;

String apiKey1       = "12W73SUJ6EMX9ZS9";
String apiKey2       = "95X1FYRYBD1RYTA6";

const char* tsserver = "api.thingspeak.com";

/*
  // WiFi UDP Variables
  unsigned int localPort = 2390;            // local port to listen on

  char udpPacketBuffer[255];                // buffer to hold incoming packet
  char udpReplyBuffer[] = "acknowledged";   // a string to send back

  WiFiUDP Udp;
*/


// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
//#define NeoPixels_D0   16   // NodeMCU - D0 ( GPIO 16  - D0 )

// How many NeoPixels are attached to the Arduino?
//#define NUMPIXELS      1

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.

//Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NeoPixels_D0, NEO_GRB + NEO_KHZ800);



/*-----( Declare User-written Functions )-----*/


void mainsetup()
{
  // setup globals
  ulReqcount = 0;
  ulReconncount = 0;
  WiFiStart();
  delay(10);

  // WiFi UDP ....
  Serial.println("\nStarting connection to server via UDP ...");
  // if you get a connection, report back via serial:
  // Udp.begin(localPort);


  server.begin();

  // set pinMode for LED's
  // initialize digital pin D1 & D2 as an output.
  pinMode(LED_D2, OUTPUT);    // D2
  pinMode(LED_D1, OUTPUT);    // D1

  // allocate ram for data storage
  uint32_t free = system_get_free_heap_size() - KEEP_MEM_FREE;
  ulNoMeasValues = free / ( (sizeof(int) * 16) + sizeof(unsigned long) ); // float temp x  Sensors + time

  pulTime = new unsigned long[ulNoMeasValues];

  for (int i = 1; i <= 16; i++)
  {
    pfTemp[i]  = new int[ulNoMeasValues];
  }

  if (pulTime    == NULL || pfTemp[1]  == NULL || pfTemp[2]  == NULL || pfTemp[3]  == NULL || pfTemp[4]  == NULL ||
      pfTemp[5]  == NULL || pfTemp[6]  == NULL || pfTemp[7]  == NULL || pfTemp[8]  == NULL || pfTemp[9]  == NULL ||
      pfTemp[10] == NULL || pfTemp[11] == NULL || pfTemp[12] == NULL || pfTemp[13] == NULL || pfTemp[14] == NULL ||
      pfTemp[15] == NULL || pfTemp[16] == NULL )
  {
    ulNoMeasValues = 0;
    Serial.println("Error in memory allocation!");
  }
  else
  {
    Serial.print("Allocated storage for ");
    Serial.print(ulNoMeasValues);
    Serial.println(" data points.");

    float fMeasDelta_sec = MEAS_SPAN_H * 3600.0 / ulNoMeasValues;
    ulMeasDelta_ms = ( (unsigned long)(fMeasDelta_sec + 0.5) ) * 1000; // round to full sec
    Serial.print("Measurements will happen each ");
    Serial.print(ulMeasDelta_ms / 1000 );
    Serial.println(" sec.");

    ulNextMeas_ms = millis() + ulMeasDelta_ms;
  }



  /* --- read EEPROM settings ------------- */

  /* EEPROM Aufteilung für Konfigurationsdaten
    Position     Länge    Wert
    0            1        Firststart Flag
    1            1        Updateintervall
    2            1        ConnectionTimeout
    3            1        Flag für zu viele Connection Timeouts -> SETUP Modus wird aktiviert bei 3 aufeinanderfolgenden Timeouts
    4            1        IP Typ 0=DHCP, 1=Statisch
    5-8          4        IP-Adresse
    9-12         4        Gateway IP-Adresse
    13-16        4        Subnet Mask

    17-20        4        RebootCNT

    81-99        18       Thingspeak API-Key 1
    100-117      18       Thingspeak API-Key 2

    120-124      4        Zeitverschiebung
    149-212      64       SENSORPWD
    213-237      25       Standort
    238          1        frei
    256-259      4        IP-Adresse PC Client oder Externer Server
    260-275      16       PC-Client / Externer Server Access-Key
    276-280      6        PC-Client Port
  */


  EEPROM.begin(512);
  delay(10);

  /*
      // Clear EEPROM
      for (int i = 0; i <= 512; i++)
      {
       EEPROM.write(i, 0);
      }
  */

  String tString = "";
  /*
  for (int i = 0; i <= 512; i++)
  {
    tString += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("EEProm settings ... ");
  Serial.print(tString);
  Serial.println();
    
  String zeit = "";
  // 120-124      4        Zeitverschiebung
  // Read zeit from EEPROM
  EEPROM.get(120, zeit);
  Serial.print("zeit ... ");
  Serial.println(zeit);

  String apiKey = "";
  // 81-99        18       Thingspeak API-Key1
  // Read zeit from EEPROM
  EEPROM.get(81, apiKey);
  Serial.print("Thinkspeak Key1 ... ");
  Serial.println(apiKey);

  // 100-117      18       Thingspeak API-Key2
  // Read zeit from EEPROM
  EEPROM.get(100, apiKey);
  Serial.print("Thinkspeak Key2 ... ");
  Serial.println(apiKey);

  */
  // 17-20        4        RebootCNT
  // Read RebootCNT from EEPROM
  EEPROM.get(17, RebootCNT);

  // INC RebootCNT
  RebootCNT++;
  //RebootCNT = 0;

  // Write RebootCNT to EEPROM
  EEPROM.put(17, RebootCNT);
  Serial.print("Reboot counter  ... ");
  Serial.println(RebootCNT);


  Serial.println();

  EEPROM.end();

  Serial.println();
  Serial.println("Start up the <Adafruit_NeoPixel.h> NeoPixel library ... ");
  //pixels.begin();
  //pixels.show(); // This clear all pixels


  Serial.println();
  Serial.println("Start up the <Wire.h> (I2C) library ... ");

  //Wire.begin(4, 5); // sda, scl - with sda cable connected to D2(4) and scl cable connected to D1(5).
  Wire.begin(14, 12); // sda, scl - with sda cable connected to D5(14) and scl cable connected to D6(12).

  I2C_Scanner();    // Scan I2C Bus ...
  delay(10);

  Serial.println();
  Serial.println("Start up the DallasTemperature library ... ");

  /*-( Start up the DallasTemperature library )-*/
  sensors1.begin();   // after Wire.begin !! cause no Comm with DS2482 possible befor !
  //sensors2.begin(); // after Wire.begin !! cause no Comm with DS2482 possible befor !
  delay(2);

  // Reset 1-Wire Bus and search for hotplug / unplug Sensors ... ( no hotplug ! )
  oneWire1.reset_search();
  //oneWire2.reset_search();
  //oneWire4.reset_search();


  //configure DS2482 to use
  // APU - active pull-up instead of pull-up resistor
  // SPU - szrong pull-up
  //oneWire1.configure(DS2482_CONFIG_SPU | DS2482_CONFIG_APU);
  //delay(2);

  //configure returns 0 if it cannot find DS2482 connected
  if ( !oneWire1.configure(DS2482_CONFIG_SPU | DS2482_CONFIG_APU) )
  {
    Serial.println();
    Serial.print("DS2482 not found\n");
    delay(2000);
  }
  else
    Serial.println();
  Serial.println("Activ  PullUp (APU) on DS2482 activated !");
  Serial.println("Strong PullUp (SPU) on DS2482 activated !");
  //Serial.println();



  // locate devices on the bus
  Serial.println();
  Serial.println("Locating 1-Wire devices ... ");
  Serial.println();

  // Grab a count of devices on the wire Bus Master 1 Port 0..7

  // select DS2482 Channel ...
  oneWire1.selectChannel(0); // Master1 select channel 0..7
  delay(2);

  // Reset 1-Wire Bus and search for hotplug / unplug Sensors ... ( no hotplug ! )
  // by restart DallasTemperature library
  // on selected channel
  sensors1.begin();   // after Wire.begin !! cause no Comm with DS2482 possible befor !
  numberOfDevices0 = sensors1.getDeviceCount();
  delay(2);

  Serial.print("Found ");
  Serial.print(numberOfDevices0, DEC);
  Serial.println(" devices on ow1 ch0 ");

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices0; i++)
  {
    // Search the wire for address
    if (sensors1.getAddress(tempDeviceAddress, i))
    {
      Serial.print("Found on ow1 ch0 :");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      // Serial.println();

      //  Serial.print("Resolution set to: ");
      //  Serial.print(sensors1.getResolution(tempDeviceAddress), DEC);

      Serial.println();
      delay(2);

    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
      Serial.println();
    }
  }

  delay(2);
  // select DS2482 Channel ...
  oneWire1.selectChannel(1); // Master1 select channel 0..7
  delay(2);

  // Reset 1-Wire Bus and search for hotplug / unplug Sensors ... ( no hotplug ! )
  // by restart DallasTemperature library
  // on selected channel
  sensors1.begin();   // after Wire.begin !! cause no Comm with DS2482 possible befor !
  numberOfDevices1 = sensors1.getDeviceCount();
  delay(2);

  Serial.println(" ");
  Serial.print("Found ");
  Serial.print(numberOfDevices1, DEC);
  Serial.println(" devices on ow1 ch1 ");

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices1; i++)
  {
    // Search the wire for address
    if (sensors1.getAddress(tempDeviceAddress, i))
    {
      Serial.print("Found on ow1 ch1 :");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      // Serial.println();

      //  Serial.print("Resolution set to: ");
      //  Serial.print(sensors2.getResolution(tempDeviceAddress), DEC);

      Serial.println();
      delay(2);
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
      Serial.println();
    }
  }

  delay(2);
  // select DS2482 Channel ...
  oneWire1.selectChannel(2); // Master 1 select channel 0..7

  // Reset 1-Wire Bus and search for hotplug / unplug Sensors ... ( no hotplug ! )
  // by restart DallasTemperature library
  // on selected channel
  sensors1.begin();   // after Wire.begin !! cause no Comm with DS2482 possible befor !
  numberOfDevices2 = sensors1.getDeviceCount();
  delay(2);

  Serial.println(" ");
  Serial.print("Found ");
  Serial.print(numberOfDevices2, DEC);
  //Serial.println(" devices on ow1 ( NodeMCU Pin D7 ) ");
  Serial.println(" devices on ow1 ch2 ");

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices2; i++)
  {
    // Search the wire for address
    if (sensors1.getAddress(tempDeviceAddress, i))
    {
      Serial.print("Found on ow1 ch2 :");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      // Serial.println();

      Serial.println();
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
      Serial.println();
    }
  }

  // If no 1-Wire Sensor found on any activ channel ...
  if ( (numberOfDevices0 + numberOfDevices1 + numberOfDevices2) == 0 )
  {
    Serial.println();
    Serial.print("DS2482> no 1-Wire Sensor found on any activ channel \n");
    TS_Status = " DS2482> no 1-Wire Sensor found on any activ channel !! \n";
    delay(2000);
  }


  Serial.println();
  Serial.print(" --- INIT DONE --- ");
  Serial.println();
  Serial.println();
  delay(2000);
  INITDone = HIGH;

}/*--(end setup)---*/



void WiFiStart()
{
  ulReconncount++;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());

}



void NTP()
{
  EEPROM.begin(512);
  delay(10);
  String zeit = "";
  for (int i = 100; i < 105; i++)
  {
    zeit += char(EEPROM.read(i));
  }
  EEPROM.end();
  Serial.print("Das ist die Zeitverschiebung: ");
  Serial.println(zeit);
  ///////////////////////////////
  // connect to NTP and get time
  ///////////////////////////////
  ulSecs2000_timer = getNTPTimestamp();

  Serial.print("*NTP debug: ");
  Serial.print( ulSecs2000_timer );
  Serial.print(" , ");
  Serial.println( ulSecs2000_bak );

  if (ulSecs2000_timer == 0)
  {
    ulSecs2000_timer = ulSecs2000_bak + ( ulMeasDelta_ms / 1000 ) ;    // take backup Time ( + sec since last Mesurment
    NoNTP = "local" ;                          // NTP request was not successfull ( debug )
  }
  else
  {
    ulSecs2000_timer = ulSecs2000_timer + 3600 +  zeit.toInt();
    ulSecs2000_bak   = ulSecs2000_timer;
    NoNTP = "NTP" ;                          // NTP request was successfull ( debug )
  }

  Serial.print("*NTP debug: " );
  Serial.print( ulSecs2000_timer );
  Serial.print(" , ");
  Serial.print( ulSecs2000_bak );
  Serial.print(" , ");
  Serial.println( NoNTP );

  Serial.println( );
  Serial.print("Current Time UTC from NTP server: " );
  Serial.println(epoch_to_string(ulSecs2000_timer).c_str());

  ulSecs2000_timer -= millis() / 1000; // keep distance to millis() counter

}



/////////////////////////////////////
// make html table for measured data
/////////////////////////////////////
unsigned long MakeTable (WiFiClient *pclient, bool bStream)
{
  unsigned long ulLength = 0;

  // here we build a big table.
  // we cannot store this in a string as this will blow the memory
  // thus we count first to get the number of bytes and later on
  // we stream this out
  if (ulMeasCount == 0)
  {
    String sTable = "Noch keine Daten verf&uuml;gbar.<BR>";
    if (bStream)
    {
      pclient->print(sTable);
    }
    ulLength += sTable.length();
  }
  else
  {
    unsigned long ulEnd;
    if (ulMeasCount > ulNoMeasValues)
    {
      ulEnd = ulMeasCount - ulNoMeasValues;
    }
    else
    {
      ulEnd = 0;
    }

    String sTable;

    if (sOffset == 0)     // Sensoren 1..8
    {
      sOffset = 0;        // Sensor Offset ( 0= 1..8 / 8= 9..16 )
      sTable = "<table style=\"width:100%\"><tr><th>Zeit / UTC : Idx : NTP</th><th>T1 &deg;C</th><th>T2 &deg;C</th><th>T3 &deg;C</th><th>T4 &deg;C</th><th>T5 &deg;C</th><th>T6 &deg;C</th><th>T7 &deg;C</th><th>T8 &deg;C</th></tr>";
    }
    else                  // Sensoren 9..16
    {
      sOffset = 8;        // Sensor Offset ( 0= 1..8 / 8= 9..16 )
      sTable = "<table style=\"width:100%\"><tr><th>Zeit / UTC : Idx : NTP</th><th>T9 &deg;C</th><th>T10 &deg;C</th><th>T11 &deg;C</th><th>T12 &deg;C</th><th>T13 &deg;C;</th><th>T14 &deg;C;</th><th>T15 &deg;C;</th><th>T16 &deg;C</th></tr>";
    }

    sTable += "<style>table, th, td {border: 2px solid black; border-collapse: collapse;} th, td {padding: 5px;} th {text-align: left;}</style>";
    for (unsigned long li = ulMeasCount; li > ulEnd; li--)
    {
      unsigned long ulIndex = (li - 1) % ulNoMeasValues;
      sTable += "<tr><td>";
      sTable += epoch_to_string(pulTime[ulIndex]).c_str();
      sTable += " : ";
      sTable += ulIndex;
      sTable += " : ";
      sTable += NoNTP;

      // convert INT back to FLOAT
      temp[1] = pfTemp[1 + sOffset][ulIndex];
      temp[2] = pfTemp[2 + sOffset][ulIndex];
      temp[3] = pfTemp[3 + sOffset][ulIndex];
      temp[4] = pfTemp[4 + sOffset][ulIndex];
      temp[5] = pfTemp[5 + sOffset][ulIndex];
      temp[6] = pfTemp[6 + sOffset][ulIndex];
      temp[7] = pfTemp[7 + sOffset][ulIndex];
      temp[8] = pfTemp[8 + sOffset][ulIndex];

      sTable += "</td><td>";
      sTable += String(temp[1] / 100);
      sTable += "</td><td>";
      sTable += String(temp[2] / 100);
      sTable += "</td><td>";
      sTable += String(temp[3] / 100);
      sTable += "</td><td>";
      sTable += String(temp[4] / 100);
      sTable += "</td><td>";
      sTable += String(temp[5] / 100);
      sTable += "</td><td>";
      sTable += String(temp[6] / 100);
      sTable += "</td><td>";
      sTable += String(temp[7] / 100);
      sTable += "</td><td>";
      sTable += String(temp[8] / 100);
      sTable += "</td></tr>";

      // play out in chunks of 1k
      if (sTable.length() > 1024)
      {
        if (bStream)
        {
          pclient->print(sTable);
          //pclient->write(sTable.c_str(),sTable.length());
        }
        ulLength += sTable.length();
        sTable = "";
      }
    }

    // remaining chunk
    sTable += "</table>";
    ulLength += sTable.length();
    if (bStream)
    {
      pclient->print(sTable);
      //pclient->write(sTable.c_str(),sTable.length());
    }
  }

  return (ulLength);
}

////////////////////////////////////////////////////
// make google chart object table for measured data
////////////////////////////////////////////////////
unsigned long MakeList (WiFiClient *pclient, bool bStream)
{
  unsigned long ulLength = 0;

  // here we build a big list.
  // we cannot store this in a string as this will blow the memory
  // thus we count first to get the number of bytes and later on
  // we stream this out
  if (ulMeasCount > 0)
  {
    unsigned long ulBegin;
    if (ulMeasCount > ulNoMeasValues)
    {
      ulBegin = ulMeasCount - ulNoMeasValues;
    }
    else
    {
      ulBegin = 0;
    }

    String sTable = "";
    for (unsigned long li = ulBegin; li < ulMeasCount; li++)
    {
      // result shall be ['18:24:08 - 21.5.2015',21.10,49.00],
      unsigned long ulIndex = li % ulNoMeasValues;
      sTable += "['";
      sTable += epoch_to_string(pulTime[ulIndex]).c_str();
      sTable += "',";

      // convert INT back to FLOAT
      temp[1] = pfTemp[1 + sOffset][ulIndex];
      temp[2] = pfTemp[2 + sOffset][ulIndex];
      temp[3] = pfTemp[3 + sOffset][ulIndex];
      temp[4] = pfTemp[4 + sOffset][ulIndex];
      temp[5] = pfTemp[5 + sOffset][ulIndex];
      temp[6] = pfTemp[6 + sOffset][ulIndex];
      temp[7] = pfTemp[7 + sOffset][ulIndex];
      temp[8] = pfTemp[8 + sOffset][ulIndex];


      sTable += String(temp[1] / 100);
      sTable += ",";
      sTable += String(temp[2] / 100);
      sTable += ",";
      sTable += String(temp[3] / 100);
      sTable += ",";
      sTable += String(temp[4] / 100);
      sTable += ",";
      sTable += String(temp[5] / 100);
      sTable += ",";
      sTable += String(temp[6] / 100);
      sTable += ",";
      sTable += String(temp[7] / 100);
      sTable += ",";
      sTable += String(temp[8] / 100);

      sTable += "],\n";

      // play out in chunks of 1k
      if (sTable.length() > 1024)
      {
        if (bStream)
        {
          pclient->print(sTable);
          //pclient->write(sTable.c_str(),sTable.length());
        }
        ulLength += sTable.length();
        sTable = "";
      }
    }

    // remaining chunk
    if (bStream)
    {
      pclient->print(sTable);
      //pclient->write(sTable.c_str(),sTable.length());
    }
    ulLength += sTable.length();
  }

  return (ulLength);
}

//////////////////////////
// create HTTP 1.1 header
//////////////////////////
String MakeHTTPHeader(unsigned long ulLength)
{
  String sHeader;

  sHeader  = F("HTTP/1.1 200 OK\r\nContent-Length: ");
  sHeader += ulLength;
  sHeader += F("\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");

  return (sHeader);
}

////////////////////
// make html footer
////////////////////
String MakeHTTPFooter()
{
  String sResponse;

  sResponse  = F("<FONT SIZE=-2><BR>Visit= ");
  sResponse += ulReqcount;
  sResponse += F(" | Reconnect= ");
  sResponse += ulReconncount;
  sResponse += F(" | Free ram= ");
  sResponse += (uint32_t)system_get_free_heap_size();
  sResponse += F(" | max. Data= ");
  sResponse += ulNoMeasValues;
  sResponse += F(" | act. Data= ");
  sResponse += ulMeasCount;
  sResponse += F(" | Reboot= ");
  sResponse += RebootCNT;

  sResponse += F("<BR>SMASE ");
  sResponse += SOFTWAREVERSION;
  sResponse += F(" | Meier Andreas -> Mail: info@meierltd.de <BR></body></html>");

  return (sResponse);
}






void sendTeperatureTS2(float temp1, float temp2, float temp3, float temp4, float temp5, float temp6, float temp7, float temp8)
{
  WiFiClient client;

  if (client.connect(tsserver, 80)) { // use ip 184.106.153.149 or api.thingspeak.com
    Serial.println("WiFi Client connected ... ");

    String apiKey = apiKey2;
    Serial.println("Thingspeak-API 2: " + apiKey);

    String postStr = apiKey;

    postStr += "&field1=";
    postStr += String(temp1);
    postStr += "&field2=";
    postStr += String(temp2);
    postStr += "&field3=";
    postStr += String(temp3);
    postStr += "&field4=";
    postStr += String(temp4);

    postStr += "&field5=";
    postStr += String(temp5);
    postStr += "&field6=";
    postStr += String(temp6);
    postStr += "&field7=";
    postStr += String(temp7);
    postStr += "&field8=";
    postStr += String(temp8);

    postStr += "&status=";
    postStr += " NodeMCU 2 > ";
    postStr += String(TS_Status);
    postStr += "snd_count: ";
    postStr += String(sent);
    postStr += " MeasCount: ";
    postStr += String(ulMeasCount);
    postStr += " MeasValue: ";
    postStr += String(ulNoMeasValues);

    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);

    delay(1000);

    Serial.println("*main: " + postStr);
    Serial.println("*main: Daten an Thingspeak gesendet");

  }//end if

  sent++;
  client.stop();

}//end send




void sendTeperatureTS(float temp1, float temp2, float temp3, float temp4, float temp5, float temp6, float temp7, float temp8)
{
  WiFiClient client;

  if (client.connect(tsserver, 80)) { // use ip 184.106.153.149 or api.thingspeak.com
    Serial.println("WiFi Client connected ... ");

    /*
      EEPROM.begin(512);
      delay(10);
      String apiKey = "";
      for (int i = 81; i < 97; i++)
      {
      apiKey += char(EEPROM.read(i));
      }
      EEPROM.end();
    */

    String apiKey = apiKey1;
    Serial.println("Thingspeak-API 1: " + apiKey);


    String postStr = apiKey;

    postStr += "&field1=";
    postStr += String(temp1);
    postStr += "&field2=";
    postStr += String(temp2);
    postStr += "&field3=";
    postStr += String(temp3);
    postStr += "&field4=";
    postStr += String(temp4);

    postStr += "&field5=";
    postStr += String(temp5);
    postStr += "&field6=";
    postStr += String(temp6);
    postStr += "&field7=";
    postStr += String(temp7);
    postStr += "&field8=";
    postStr += String(temp8);

    postStr += "&status=";
    postStr += " NodeMCU 2 > ";
    postStr += String(TS_Status);
    postStr += "snd_count: ";
    postStr += String(sent);
    postStr += " MeasCount: ";
    postStr += String(ulMeasCount);
    postStr += " MeasValue: ";
    postStr += String(ulNoMeasValues);

    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    delay(1000);

    Serial.println("*main: " + postStr);
    Serial.println("*main: Daten an Thingspeak gesendet");

  }//end if

  sent++;
  client.stop();

}//end send






void ds18b20() {

  // Setup oWire Lib to "NO WaitForConversion" ------------------

  //sensors1.setWaitForConversion(false);
  //sensors2.setWaitForConversion(false);	// only wait for last oWire Pin ( sensors4 )
  //sensors3.setWaitForConversion(false);
  //sensors4.setWaitForConversion(false);

  // Send the command to get temperatures ------------------

  // select DS2482 Channel ...
  oneWire1.selectChannel(0); // Master 1 select channel 0..7
  Serial.print("Requesting temperature on ch0 ...");
  oneWire1.configure(DS2482_CONFIG_SPU | DS2482_CONFIG_APU);  
  sensors1.requestTemperatures();
  Serial.println("DONE");

  // select DS2482 Channel ...
  oneWire1.selectChannel(1); // Master 1 select channel 0..7
  Serial.print("Requesting temperature on ch1 ...");
  oneWire1.configure(DS2482_CONFIG_SPU | DS2482_CONFIG_APU);  
  sensors1.requestTemperatures();

  //delay(10);                       		// wait 800ms to complete the conversion
  // and give the PullUp Resitor a chance to reload the sensors ....
  Serial.println("DONE");

  // select DS2482 Channel ...
  oneWire1.selectChannel(0); // Master 1 select channel 0..7

  temp[1] = sensors1.getTempC(SensorID01);                // temp1 should be real value
  temp[2] = sensors1.getTempC(SensorID02);
  temp[3] = sensors1.getTempC(SensorID03);
   
  temp[7] = sensors1.getTempC(SensorID07);
  temp[8] = sensors1.getTempC(SensorID08);
  
  temp[9] = sensors1.getTempC(SensorID09);               // temp9 should be real value
  
  // select DS2482 Channel ...
  oneWire1.selectChannel(1); // Master 1 select channel 0..7
 
  temp[4] = sensors1.getTempC(SensorID04);
  temp[5] = sensors1.getTempC(SensorID05);                // temp5 should be real value
  temp[6] = sensors1.getTempC(SensorID06);
 
  temp[10] = sensors1.getTempC(SensorID10);
  temp[11] = sensors1.getTempC(SensorID11);
  temp[12] = sensors1.getTempC(SensorID12);

  temp[13] = sensors1.getTempC(SensorID13);               // temp13 should be real value
  temp[14] = sensors1.getTempC(SensorID14);
  temp[15] = sensors1.getTempC(SensorID15);
  temp[16] = sensors1.getTempC(SensorID16);

  /*
    // round to 1 decimal place
    temp1 = (temp1 / 10.0) * 10.0;  // 123.45 --> 12.34 --> 123.4
    temp2 = (temp2 / 10.0) * 10.0;
    temp3 = (temp3 / 10.0) * 10.0;
    temp4 = (temp4 / 10.0) * 10.0;
  */

  //String tempC = dtostrf(temp, 4, 1, buffer);//handled in sendTemp()
  Serial.println("*main: snd_count: " + String(sent));

  Serial.print("*main: T1..8 : ");
  for (int i = 1; i <= 8; i++)      // 1..8
  {
    //DEBUG_PRINT(i);
    Serial.print(String(temp[i], 2) );
    Serial.print(" , ");
  }
  Serial.println(" ");

  Serial.print("*main: T9..16: " );
  for (int i = 9; i <= 16; i++)      // 9..16
  {
    //DEBUG_PRINT(i);
    Serial.print(String(temp[i], 2) );
    Serial.print(" , ");
  }
  Serial.println(" ");

  Serial.println("*main: --------------- ");
  Serial.println(" ");

}




void mainloop()
{

  // Check if a client has connected
  WiFiClient client = server.available();


  // --- oWire Poll -------------------------------------------------------------------------

  tslong = (millis() - oWirePoll);      // has to be signed value , cause it could be negativ !!

  if ( tslong >= 5000 )
  {
    Serial.print("oWire Poll    .... ");
    Serial.print(millis());
    Serial.print(" / ");
    Serial.print(oWirePoll);
    Serial.print(" / ");
    Serial.println(tslong);

    oWirePoll = millis();               // save actual ms into LED oWirePoll status ms

    // update Temperatur Sensoren
    if (!client)
    {
      ds18b20();   // read Sensors  -------  poll only if no Webclient ( Browser ) connected ...
    }
    else
    {
      Serial.println("Suspend polling for 5000 ms due to Webclient request ....");
      oWirePoll = millis() + 15000;        // save actual ms +5000 ms into oWirePoll status ms
    }

  }
  // --- oWire Poll -------------------------------------------------------------------------



  // --- LifeBit / PLC  ---------------------------------------------------------------------

  tslong = (millis() - LED_D2_ms);      // has to be signed value , cause it could be negativ !!

  if ( tslong >= 1000 )
  {
    Serial.print("LifeBit / PLC .... ");
    Serial.print(millis());
    Serial.print(" / ");
    Serial.print(LED_D2_ms);
    Serial.print(" / ");
    Serial.println(tslong);

    LED_D2_ms = millis();               // save actual ms into LED status ms

    // invert LED status
    LED_D2_State = !LED_D2_State;

    // write State to OutPin
    digitalWrite(LED_D2, LED_D2_State);   // turn the LED on (HIGH is the voltage level)

 
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    //pixels.setPixelColor(0, pixels.Color(0,150,0)); // Moderately bright green color.
    //pixels.show(); // This sends the updated pixel color to the hardware.


    /*
      Serial.print("PinState : ");
      Serial.println(digitalRead(LED_D2));
      Serial.println(LED_D2_State);
    */


    // --- PLC Logic --------------------------------------------

    // --- PLC local Var --------------------------------------
    float HeU  = temp[1];      // Temperatur Heiss Unten
    float HeM  = temp[2];      // Temperatur Heiss Mitte
    float HeO  = temp[3];      // Temperatur Heiss Oben
    float SoVL = temp[7];      // Temperatur Heiss Mitte
    float SoRL = temp[8];      // Temperatur Heiss Oben

    float dtHeU_SoP = temp[16] - HeU;      // differenz Temperatur Solar Panel / Heiss Unten

    // --- PLC local Var --------------------------------------



    // --- END BA Automatik ----------
    if ( BA == "AUTOMATIK" )
    {
      if  ( dtHeU_SoP >= 10.0 )                      {
        SoPumpe = HIGH;
      }
      if (( dtHeU_SoP <=  3.0 ) || ( HeU >= 85.0 ))  {
        SoPumpe = LOW;
      }
    };
    // --- END BA Automatik ----------


    // Signalausgabe an Ausgang ---------
    LED_D1_State = SoPumpe;
    digitalWrite(LED_D1, LED_D1_State);
    // Signalausgabe an Ausgang ---------


    // --- PLC Logic --------------------------------------------

    ow1_ds2413.setAdr(DS2413ID01);      	//configure the ds2413 address

    //READ PINS STATES
    //ow1_ds2413.write(0,0);              //sometimes they "hang" in high state
    ow1_ds2413.read(C, D);
    Serial.print("PIO Read  function result (C,D): ");
    Serial.print(C, HEX);
    Serial.print(",");
    Serial.println(D, HEX);

    //Write Pins
    //A=LOW ;                 // LED off, PIO pin=1=HIGH=true
    //B=HIGH;                 // Switch pin output latch always HIGH.

    if (A == HIGH) {
      A = LOW;
      B = HIGH;
    }  else {
      A = HIGH;
      B = LOW;
    }

    ow1_ds2413.write(A, B);
    Serial.print("PIO Write function States (A,B): ");
    Serial.print(A, HEX);
    Serial.print(",");
    Serial.println(B, HEX);

    Serial.println();

  }
  // --- LifeBit / PLC  ---------------------------------------------------------------------




  ///////////////////
  // do data logging
  ///////////////////
  if ( (millis() >= ulNextMeas_ms ) or ( sent == 0 ) and (!client)  )
  {
    //ds18b20();      // read Sensors
    NTP();
    sendTeperatureTS ( temp[1], temp[2], temp[3], temp[4], temp[5], temp[6], temp[7], temp[8] );         // TS API Eprom
    sendTeperatureTS2( temp[9], temp[10], temp[11], temp[12], temp[13], temp[14], temp[15], temp[16] );  // TS API 2

    ulNextMeas_ms = millis() + ulMeasDelta_ms;
    /*
      Serial.println("dddddd");
      Serial.println(ulMeasCount);
      Serial.println(ulNoMeasValues);
      Serial.println(ulSecs2000_timer);
    */

    //  % -  modulo operator rolls over variable
    pulTime[ulMeasCount % ulNoMeasValues] = ( millis() / 1000 ) + ulSecs2000_timer;

    Serial.println("Logging Temperatures: ");

    for (int i = 1; i <= 16; i++)      // 1..16
    {
      //DEBUG_PRINT(i);
      pfTemp[i] [ulMeasCount % ulNoMeasValues] = temp[i] * 100;   // save 2 digits
      Serial.print(pfTemp[i][ulMeasCount % ulNoMeasValues]);
      Serial.println(" deg Celsius ");
    }

    Serial.print("Time : ");
    Serial.println(pulTime[ulMeasCount % ulNoMeasValues]);

    Serial.print("Index (Modulo) : ");
    Serial.println(ulMeasCount % ulNoMeasValues);

    Serial.print("MesCount / maxCount : ");
    Serial.print(ulMeasCount);
    Serial.print(" / ");
    Serial.println(ulNoMeasValues);
    Serial.println();

    ulMeasCount++;

  }






  // Check if a client has connected
  // WiFiClient client = server.available();
  if (!client)
  {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");

  Serial.println("Suspend polling for 15 s due to Webclient request ....");
  oWirePoll = millis() + 15000;           // save actual ms +15000 ms into oWirePoll status ms
  ulNextMeas_ms = ulNextMeas_ms + 10000;

  unsigned long ultimeout = millis() + 500;   // 250
  while (!client.available() && (millis() < ultimeout) )
  {
    delay(1);
  }
  if (millis() > ultimeout)
  {
    Serial.println("client connection time-out!");
    return;
  }

  // Read the first line of the request
  String sRequest = client.readStringUntil('\r');
  //Serial.println(sRequest);
  client.flush();

  // stop client, if request is empty
  if (sRequest == "")
  {
    Serial.println("empty request! - stopping client");
    client.stop();
    return;
  }

  // get path; end of path is either space or ?
  // Syntax is e.g. GET /?pin=MOTOR1STOP HTTP/1.1
  String sPath = "", sParam = "", sCmd = "";
  String sGetstart = "GET ";
  int iStart, iEndSpace, iEndQuest;
  iStart = sRequest.indexOf(sGetstart);
  if (iStart >= 0)
  {
    iStart += +sGetstart.length();
    iEndSpace = sRequest.indexOf(" ", iStart);
    iEndQuest = sRequest.indexOf("?", iStart);

    // are there parameters?
    if (iEndSpace > 0)
    {
      if (iEndQuest > 0)
      {
        // there are parameters
        sPath  = sRequest.substring(iStart, iEndQuest);
        sParam = sRequest.substring(iEndQuest, iEndSpace);
      }
      else
      {
        // NO parameters
        sPath  = sRequest.substring(iStart, iEndSpace);
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////////
  // output parameters to serial, you may connect e.g. an Arduino and react on it
  ///////////////////////////////////////////////////////////////////////////////
  if (sParam.length() > 0)
  {
    int iEqu = sParam.indexOf("=");
    if (iEqu >= 0)
    {
      sCmd = sParam.substring(iEqu + 1, sParam.length());
      Serial.print("Die Eingabe ist: ");
      Serial.println(sCmd);


      // Switch ......................................

      // Betriesart 0 = Auto / 1 = Hand
      if ( sCmd == "ON1" )   {
        BA = "HAND";
      }
      if ( sCmd == "OFF1" )  {
        BA = "AUTOMATIK";
      }

      if ( sCmd == "ON2"  & BA == "HAND" )    {
        SoPumpe = HIGH;
      }
      if ( sCmd == "OFF2" & BA == "HAND" )    {
        SoPumpe = LOW;
      }

    }

  }


  ///////////////////////////
  // format the html response
  ///////////////////////////
  // Startseite ////////////////////////////////

  String sResponse, sResponse2, sHeader;


  if (sPath == "/")
  {
    ulReqcount++;
    int iIndex = (ulMeasCount - 1) % ulNoMeasValues;
    sResponse  = F("<html>\n<head>\n<title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title>\n");
    sResponse += F("<link rel='shortcut icon' type='image/x-icon' href='//www.arduino.cc/en/favicon.png' /> ");
    sResponse += F("<meta http-equiv='refresh' content='60'> ");
    sResponse += F("</head>\n<body>\n<font color=\"#000000\"><body bgcolor=\"#d0d0f0\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>SMASE<BR><BR><FONT SIZE=+1>Letzte Messung um ");
    sResponse += F("Seite: Temperaturen -- > Zeigt die gemessenen Temperaturen an <br>");
    sResponse += F("<hr>Seite: Grafik    -- > Zeigt den Temperaturverlauf (Diagramm) der letzten 24 h an <br>");
    sResponse += F("<hr>Seite: Tabelle  -- > Zeigt den Temperaturverlauf (Tabelle) der letzten 24 h an <br>");
    sResponse += F("<hr>Seite: Settings -- > Einstellungen <br>");
    sResponse += F(" <div style=\"clear:both;\"></div>");

    sResponse2  = F("<p>Temperaturverlauf - Seiten laden l&auml;nger:<BR>  <a href=\"/anzeige\"><button>Temperaturen 1..8 </button></a>    <a href=\"/grafik\"><button>Grafik 1..8 </button></a>     <a href=\"/tabelle\"><button>Tabelle 1..8 </button></a>     <a href=\"/settings\"><button>Settings</button></a></p>");
    sResponse2 += F("<p> <a href=\"/anzeige2\"><button>Temperaturen 9..16</button></a>    <a href=\"/grafik2\"><button>Grafik 9..16</button></a>     <a href=\"/tabelle2\"><button>Tabelle 9..16 </button></a>   </p>");
    sResponse2 += F("<p>Betriebsart : <a href=\"?pin=OFF1\"><button>Automatik</button></a>&nbsp;<a href=\"?pin=ON1\"><button>Hand</button></a>");

    // Anzeige Betriesart .. HAND in blau , AUTOMATIK in grün
    if (BA == "HAND")
    {
      sResponse2 += F("<font color='blue'>");
    }
    else
    {
      sResponse2 += F("<font color='LimeGreen'>");
    }

    sResponse2 += BA;
    sResponse2 += F("</font>");
    sResponse2 += F("</p> ");


    sResponse2 += F("<p>Solar Pumpe : <a href=\"?pin=ON2\"><button>Ein</button></a>&nbsp;<a href=\"?pin=OFF2\"><button>Aus</button></a> ");

    // Anzeige SolarPumpe .. AUS in blau , EIN in grün
    if (SoPumpe == LOW)
    {
      sResponse2 += F("<font color='blue'>");
    }
    else
    {
      sResponse2 += F("<font color='LimeGreen'>");
    }

    sResponse2 += SoPumpe;
    sResponse2 += F("</font>");
    sResponse2 += F("</p> ");


    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }


  if ( (sPath == "/anzeige") or (sPath == "/anzeige2") )
  {
    ulReqcount++;
    int iIndex = (ulMeasCount - 1) % ulNoMeasValues;
    sResponse  = F("<html>\n<head>\n<title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title>\n<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n");
    sResponse += F("<link rel='shortcut icon' type='image/x-icon' href='//www.arduino.cc/en/favicon.png' /> ");

    sResponse2 += F("<BR>\n<div id=\"curve_chart\" style=\"width: 1200px; height: 400px\"></div>");

    sResponse += F("<script type=\"text/javascript\">\ngoogle.charts.load('current', {'packages':['gauge']});\n");
    sResponse += F("  google.charts.setOnLoadCallback(drawGauge); \n");
    sResponse += F("\nvar gaugeOptions = { width: 800, height: 350, min: -10, max: 100, greenColor: '#0000ff', greenFrom: -10, greenTo: 10, yellowFrom: 65, yellowTo: 90, redFrom: 90, redTo: 100, minorTicks: 10, majorTicks: ['-10','0','10','20','30','40','50','60','70','80','90','100']};\n");
    sResponse += F("  var gauge;  \n");

    // greenColor: '#0000ff',  // default '#FF9900'
    // ‘#0000ff’ blue color

    sResponse += F("  function drawGauge() { \n");
    sResponse += F("  gaugeData = new google.visualization.DataTable();  \n");

    if (sPath == "/anzeige")    // Sensoren 1..8
    {
      sOffset = 0;    // Sensor Offset ( 0= 1..8 / 8= 9..16 )

      sResponse += F("  gaugeData.addColumn('number', 'HeU');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'HeM');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'HeO');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'KaO');  \n");

      sResponse += F("  gaugeData.addColumn('number', 'KaM');  \n");
      sResponse += F("  gaugeData.addColumn('number', 'KaU');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'SoVL');  \n");
      sResponse += F("  gaugeData.addColumn('number', 'SoRL');  \n");
    }
    else                         // Sensoren 9..16
    {
      sOffset = 8;    // Sensor Offset ( 0= 1..8 / 8= 9..16 )

      sResponse += F("  gaugeData.addColumn('number', 'Abw');    \n");
      sResponse += F("  gaugeData.addColumn('number', 'Luft');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'HzVL');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'HzRL');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'WpVL');   \n");
      sResponse += F("  gaugeData.addColumn('number', 'WpRL');   \n");

      sResponse += F("  gaugeData.addColumn('number', 'T15');    \n");
      sResponse += F("  gaugeData.addColumn('number', 'T16');    \n");
    }

    sResponse += F("  gaugeData.addRows(2);  \n");

    sResponse += F("  gaugeData.setCell(0, 0, ");
    // sResponse += pfTemp[1+sOffset][iIndex];
    sResponse += temp[1 + sOffset];
    sResponse += F(" ); \n");
    sResponse += F("  gaugeData.setCell(0, 1, ");
    sResponse += temp[2 + sOffset];
    sResponse += F(" ); \n");
    sResponse += F("  gaugeData.setCell(0, 2, ");
    sResponse += temp[3 + sOffset];
    sResponse += F(" ); \n");
    sResponse += F("  gaugeData.setCell(0, 3, ");
    sResponse += temp[4 + sOffset];
    sResponse += F(" ); \n");

    sResponse += F("  gaugeData.setCell(0, 4, ");
    sResponse += temp[5 + sOffset];
    sResponse += F(" ); \n");
    sResponse += F("  gaugeData.setCell(0, 5, ");
    sResponse += temp[6 + sOffset];
    sResponse += F(" ); \n");
    sResponse += F("  gaugeData.setCell(0, 6, ");
    sResponse += temp[7 + sOffset];
    sResponse += F(" ); \n");
    sResponse += F("  gaugeData.setCell(0, 7, ");
    sResponse += temp[8 + sOffset];
    sResponse += F(" ); \n");

    sResponse += F("  gauge = new google.visualization.Gauge(document.getElementById('gauge_div'));  \n");
    sResponse += F("  gauge.draw(gaugeData, gaugeOptions);  \n");

    sResponse += F("  } \n");
    sResponse += F("  </script> \n  </head> \n <body> \n<font color=\"#000000\"><body bgcolor=\"#d0d0f0\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>SMASE</h1><BR><BR><FONT SIZE=+1>Letzte Messung um ");
    sResponse += epoch_to_string(pulTime[iIndex]).c_str();
    sResponse += F(" UTC<BR>\n");

    if (sPath == "/anzeige")    // Sensoren 1..8
    {
      sResponse += F("<fieldset><legend>Pufferspeicher  Sensoren 1..8 </legend>");
    }
    else                        // Sensoren 9..16
    {
      sResponse += F("<fieldset><legend>Pufferspeicher  Sensoren 9..16 </legend>");
    }

    sResponse += F("<div id=\"gauge_div\" style=\"width:140px; height: 360px;\"></div> \n");
    sResponse += F("</fieldset>");
    sResponse += F(" <div style=\"clear:both;\"></div>");

    if (sPath == "/anzeige")    // Sensoren 1..8
    {
      sResponse2 = F("<p>Temperaturverlauf - Seiten laden l&auml;nger:<BR>  <a href=\"/\"><button>Startseite</button></a>  <a href=\"/grafik\"><button>Grafik</button></a>     <a href=\"/tabelle\"><button>Tabelle</button></a>     <a href=\"/settings\"><button>Settings</button></a></p>");
    }
    else                        // Sensoren 9..16
    {
      sResponse2 = F("<p>Temperaturverlauf - Seiten laden l&auml;nger:<BR>  <a href=\"/\"><button>Startseite</button></a>  <a href=\"/grafik2\"><button>Grafik</button></a>     <a href=\"/tabelle2\"><button>Tabelle</button></a>     <a href=\"/settings\"><button>Settings</button></a></p>");
    }

    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }
  // Tabelle ////////////////////////////////
  else if ( (sPath == "/tabelle")  or (sPath == "/tabelle2") )
    ////////////////////////////////////
    // format the html page for /tabelle
    ////////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title> \n");
    sResponse += F("<link rel='shortcut icon' type='image/x-icon' href='//www.arduino.cc/en/favicon.png' /> \n");
    sResponse += F("</head> \n <body> \n");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");

    if (sPath == "/tabelle")    // Sensoren 1..8
    {
      sOffset = 0;    // Sensor Offset ( 0= 1..8 / 8= 9..16 )
      sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur  Sensoren 1..8 </h1>");
    }
    else                        // Sensoren 9..16
    {
      sOffset = 8;    // Sensor Offset ( 0= 1..8 / 8= 9..16 )
      sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur  Sensoren 9..16 </h1>");
    }

    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Letzte Messungen im Abstand von ");
    sResponse += (ulMeasDelta_ms / 1000);
    sResponse += F("s<BR>");
    // here the big table will follow later - but let us prepare the end first

    // part 2 of response - after the big table
    sResponse2 = MakeHTTPFooter().c_str();

    // Send the response to the client - delete strings after use to keep mem low
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length() + ulSizeList).c_str());
    client.print(sResponse); sResponse = "";
    MakeTable(&client, true);
    client.print(sResponse2);
  }
  // Diagramm ////////////////////////////////
  else if ( (sPath == "/grafik")  or (sPath == "/grafik2") )
    ///////////////////////////////////
    // format the html page for /grafik
    ///////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeList(&client, false); // get size of list first

    sResponse  = F("<html>\n<head>\n<title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['corechart']}]}\"></script>\n");
    sResponse += F("<link rel='shortcut icon' type='image/x-icon' href='//www.arduino.cc/en/favicon.png' /> ");

    if (sPath == "/grafik")    // Sensoren 1..8
    {
      sOffset = 0;    // Sensor Offset ( 0= 1..8 / 8= 9..16 )
      sResponse += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);\nfunction drawChart() {var data = google.visualization.arrayToDataTable([\n['Zeit / UTC', 'HeU', 'HeM', 'HeO', 'KaO', 'KaM', 'KaU', 'SoVL', 'SoRL'],\n");
    }
    else                        // Sensoren 9..16
    {
      sOffset = 8;    // Sensor Offset ( 0= 1..8 / 8= 9..16 )
      sResponse += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);\nfunction drawChart() {var data = google.visualization.arrayToDataTable([\n['Zeit / UTC', 'Abw', 'Luft', 'HzVL', 'HzRL', 'WpVL', 'WpRL', 'Sensor 15', 'Sensor 16'],\n");
    }


    // here the big list will follow later - but let us prepare the end first


    // part 2 of response - after the big list
    sResponse2  = F("]);\nvar options = {title: 'Verlauf',\n");
    sResponse2 += F("curveType:'function',animation:{ duration: 1000, easing: 'linear' }, animation:{ startup : 'true' } , legend:{ position: 'bottom'}};");
    sResponse2 += F("var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));chart.draw(data, options);}\n</script>\n</head>\n");
    sResponse2 += F("<body>\n<font color=\"#000000\"><body bgcolor=\"#d0d0f0\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>WLAN Logger f&uuml;r Pufferspeichertemperatur Sensoren 1..8 </h1><a href=\"/\"><button>Startseite</button></a><BR>");
    sResponse2 += F("<BR>\n<div id=\"curve_chart\" style=\"width: 1200px; height: 400px\"></div>");

    sResponse2 += MakeHTTPFooter().c_str();

    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length() + ulSizeList).c_str());
    client.print(sResponse); sResponse = "";
    MakeList(&client, true);
    client.print(sResponse2);
  }


  // Einstellungen ////////////////////////////////
  else if (sPath == "/settings")
  {
    EEPROM.begin(512);
    delay(10);
    String apiKey = "";
    for (int i = 81; i < 100; i++)
    {
      //DEBUG_PRINT(i);
      apiKey += char(EEPROM.read(i));
    }
    EEPROM.end();
    Serial.println("Thinkspeak apiKey1: " + apiKey);
    Serial.println("Thinkspeak apiKey2: " + apiKey);

    EEPROM.begin(512);
    delay(10);
    String zeit = "";
    for (int i = 100; i < 105; i++)
    {
      zeit += char(EEPROM.read(i));
    }
    EEPROM.end();
    Serial.print("Das ist die Zeitverschiebung: ");
    Serial.println(zeit);
    String zeittext = "";
    if (zeit != "0000")
    {
      zeittext = "Sommerzeit";
    }
    else
    {
      zeittext = "Winterzeit";
    }

    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title> \n");
    sResponse += F("<link rel='shortcut icon' type='image/x-icon' href='//www.arduino.cc/en/favicon.png' /> \n");
    sResponse += F("</head><body> \n");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Thingspeak apiKey1 ist momentan: ");
    sResponse += apiKey1;
    sResponse += F("<BR><BR>Thingspeak apiKey2 ist momentan: ");
    sResponse += apiKey2;
    sResponse += F("<BR><BR>Das ist die Zeitverschiebung: ");
    sResponse += zeittext;
    sResponse += F("<fieldset><legend>EEPROM Setting</legend>");
    sResponse += F("<p><a href=\"/reset\"><button>RESET</button></a>&nbsp;<a href=\"?pin=FUNCTION2ON\"><button>AUSLESEN</button></a></p>");
    sResponse += F("</fieldset>");
    sResponse += F("<fieldset><legend>Allgemein Setting</legend>");
    sResponse += F("<p>Zeitumstellung <a href=\"?pin=SOMMERZEIT\"><button>SOMMERZEIT</button></a>&nbsp;<a href=\"?pin=WINTERZEIT\"><button>WINTERZEIT</button></a></p>");
    sResponse += F("<form name=\"input\" action=\"\" method=\"get\">THINGSPEAK apiKey1: <input type=\"text\" name=\"$\"><input type=\"submit\" value=\"Submit\"></form>");
    sResponse += F("<form name=\"input\" action=\"\" method=\"get\">THINGSPEAK apiKey2: <input type=\"text\" name=\"$\"><input type=\"submit\" value=\"Submit\"></form>");
    sResponse += F("</fieldset>");
    sResponse += F("<fieldset><legend>Temperatur kalibrieren</legend>");
    sResponse += F("<p><a href=\"/temp1\"><button>Speicher oben</button></a>&nbsp;<a href=\"/temp2\"><button>Speicher mitte</button></a>&nbsp;<a href=\"/temp3\"><button>Speicher unten</button></a>&nbsp;<a href=\"/temp4\"><button>Vorlauf</button></a></p>");
    sResponse += F("</fieldset>");

    sResponse2 = MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
    delay(100);

    //////////////////////
    // react on parameters
    //////////////////////
    if (sCmd.length() > 0)
    {
      // write received command to html page
      sResponse += "Kommando:" + sCmd + "<BR>";

      // EEPROM RESET ////////////////////////////////
      if (sCmd.indexOf("FUNCTION1ON") >= 0)
      {
        EEPROM.begin(512);
        // write a 0 to all 512 bytes of the EEPROM
        for (int i = 0; i < 512; i++)
        {
          EEPROM.write(i, 0);

          EEPROM.end();
        }
      }
      // SHOW EEPROM ////////////////////////////////
      else if (sCmd.indexOf("FUNCTION2ON") >= 0)
      {
        EEPROM.begin(512);
        delay(10);
        String string3 = "";
        for (int i = 0; i < 150; i++)
        {
          //DEBUG_PRINT(i);
          string3 += char(EEPROM.read(i));
        }
        EEPROM.end();
        Serial.println(string3);
      }
      // SOMMERZEIT EINSTELLEN ////////////////////////////////
      else if (sCmd.indexOf("SOMMERZEIT") >= 0)
      {
        String sommer = "3600";
        Serial.print("Das wird gespeichert in der seite: ");
        Serial.println(sommer);
        EEPROM.begin(512);
        delay(10);
        int si = 0;
        for (int i = 100; i < 105; i++)
        {
          char c;
          if (si < sommer.length())
          {
            c = sommer[si];
          }
          else
          {
            c = 0;
          }

          EEPROM.write(i, c);
          si++;
        }
        EEPROM.end();
        Serial.println("Wrote " + sommer);
      }
      // WINTERZEIT EINSTELLEN ////////////////////////////////
      else if (sCmd.indexOf("WINTERZEIT") >= 0)
      {
        String winter = "0000";
        Serial.print("Das wird gespeichert in der seite: ");
        Serial.println(winter);
        EEPROM.begin(512);
        delay(10);
        int si = 0;
        for (int i = 100; i < 105; i++)
        {
          char c;
          if (si < winter.length())
          {
            c = winter[si];
          }
          else
          {
            c = 0;
          }
          EEPROM.write(i, c);
          si++;
        }
        EEPROM.end();
        Serial.println("Wrote " + winter);
      }
      // SET THINGSPEAK API ////////////////////////////////
      else
      {
        Serial.print("Das wird gespeichert in der seite: ");
        Serial.println(sCmd);
        EEPROM.begin(512);
        delay(10);
        int si = 0;
        for (int i = 81; i < 100; i++)
        {
          char c;
          if (si < sCmd.length())
          {
            c = sCmd[si];
            //DEBUG_PRINT("Wrote: ");
            //DEBUG_PRINT(c);
          }
          else
          {
            c = 0;
          }
          EEPROM.write(i, c);
          si++;
        }
        EEPROM.end();
        Serial.println("Wrote " + sCmd);
      }
    }
  }

  // Kalibrieren Temperatur 1 ////////////////////////////////
  else if (sPath == "/temp1")
    ////////////////////////////////////
    // format the html page for /tabelle
    ////////////////////////////////////
  {
    EEPROM.begin(512);
    delay(10);
    String temp1k = "";
    for (int i = 110; i < 115; i++)
    {
      temp1k += char(EEPROM.read(i));
    }
    EEPROM.end();
    Serial.print("Das ist die Zeitverschiebung: ");
    Serial.println(temp1k);
    // settings();
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Speicher oben: ");
    sResponse += temp1k;
    sResponse += F("Grad C<BR>");
    sResponse += F("<form name=\"input\" action=\"\" method=\"get\">Speicher oben: <input type=\"text\" name=\"$\"><input type=\"submit\" value=\"Submit\"></form>");
    sResponse += F("<p>Temperatur kalibrieren: <a href=\"/temp1\"><button>Speicher oben</button></a>&nbsp;<a href=\"/temp2\"><button>Speicher mitte</button></a>&nbsp;<a href=\"/temp3\"><button>Speicher unten</button></a>&nbsp;<a href=\"/temp4\"><button>Vorlauf</button></a>&nbsp;</p>");

    sResponse2 = MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
    delay(100);

    //////////////////////
    // react on parameters
    //////////////////////
    if (sCmd.length() > 0)
    {
      // write received command to html page
      sResponse += "Kommando:" + sCmd + "<BR>";
      // SET THINGSPEAK API ////////////////////////////////

      if (sCmd.toInt() != 0)
      {

        Serial.print("Das wird gespeichert in der seite: ");
        Serial.println(sCmd);
        EEPROM.begin(512);
        delay(10);
        int si = 0;
        for (int i = 110; i < 115; i++)
        {
          char c;
          if (si < sCmd.length())
          {
            c = sCmd[si];
            //DEBUG_PRINT("Wrote: ");
            //DEBUG_PRINT(c);
          }
          else
          {
            c = 0;
          }
          EEPROM.write(i, c);
          si++;
        }
        EEPROM.end();
        Serial.println("Wrote " + sCmd);
      }
      else
      {
        Serial.println("Der Wert " + sCmd + " war keine Zahl!!!");
      }

    }
  }
  // Kalibrieren Temperatur 2 ////////////////////////////////
  else if (sPath == "/temp2")
    ////////////////////////////////////
    // format the html page for /tabelle
    ////////////////////////////////////
  {
    EEPROM.begin(512);
    delay(10);
    String temp2k = "";
    for (int i = 115; i < 120; i++)
    {
      temp2k += char(EEPROM.read(i));
    }
    EEPROM.end();
    Serial.print("Das ist die Zeitverschiebung: ");
    Serial.println(temp2k);
    // settings();
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Speicher mitte: ");
    sResponse += temp2k;
    sResponse += F("Grad C<BR>");
    sResponse += F("<form name=\"input\" action=\"\" method=\"get\">Speicher mitte: <input type=\"text\" name=\"$\"><input type=\"submit\" value=\"Submit\"></form>");
    sResponse += F("<p>Temperatur kalibrieren: <a href=\"/temp1\"><button>Speicher oben</button></a>&nbsp;<a href=\"/temp2\"><button>Speicher mitte</button></a>&nbsp;<a href=\"/temp3\"><button>Speicher unten</button></a>&nbsp;<a href=\"/temp4\"><button>Vorlauf</button></a>&nbsp;</p>");

    sResponse2 = MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
    delay(100);

    //////////////////////
    // react on parameters
    //////////////////////
    if (sCmd.length() > 0)
    {
      // write received command to html page
      sResponse += "Kommando:" + sCmd + "<BR>";
      // SET THINGSPEAK API ////////////////////////////////


      if (sCmd.toInt() != 0)
      {

        Serial.print("Das wird gespeichert in der seite: ");
        Serial.println(sCmd);
        EEPROM.begin(512);
        delay(10);
        int si = 0;
        for (int i = 115; i < 120; i++)
        {
          char c;
          if (si < sCmd.length())
          {
            c = sCmd[si];
            //DEBUG_PRINT("Wrote: ");
            //DEBUG_PRINT(c);
          }
          else
          {
            c = 0;
          }
          EEPROM.write(i, c);
          si++;
        }
        EEPROM.end();
        Serial.println("Wrote " + sCmd);
      }
      else
      {
        Serial.println("Der Wert " + sCmd + " war keine Zahl!!!");
      }

    }
  }
  // Kalibrieren Temperatur 3 ////////////////////////////////
  else if (sPath == "/temp3")
    ////////////////////////////////////
    // format the html page for /tabelle
    ////////////////////////////////////
  {
    EEPROM.begin(512);
    delay(10);
    String temp3k = "";
    for (int i = 120; i < 125; i++)
    {
      temp3k += char(EEPROM.read(i));
    }
    EEPROM.end();
    Serial.print("Das ist der aktuelle Korrekturwert: ");
    Serial.println(temp3k);
    // settings();
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Speicher unten ");
    sResponse += temp3k;
    sResponse += F("Grad C<BR>");
    sResponse += F("<form name=\"input\" action=\"\" method=\"get\">Speicher unten: <input type=\"text\" name=\"$\"><input type=\"submit\" value=\"Submit\"></form>");
    sResponse += F("<p>Temperatur kalibrieren: <a href=\"/temp1\"><button>Speicher oben</button></a>&nbsp;<a href=\"/temp2\"><button>Speicher mitte</button></a>&nbsp;<a href=\"/temp3\"><button>Speicher unten</button></a>&nbsp;<a href=\"/temp4\"><button>Vorlauf</button></a>&nbsp;</p>");

    sResponse2 = MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
    delay(100);

    //////////////////////
    // react on parameters
    //////////////////////
    if (sCmd.length() > 0)
    {
      // write received command to html page
      sResponse += "Kommando:" + sCmd + "<BR>";
      // SET THINGSPEAK API ////////////////////////////////
      if (sCmd.toInt() != 0)
      {

        Serial.print("Das wird gespeichert in der seite: ");
        Serial.println(sCmd);
        EEPROM.begin(512);
        delay(10);
        int si = 0;
        for (int i = 120; i < 125; i++)
        {
          char c;
          if (si < sCmd.length())
          {
            c = sCmd[si];
            //DEBUG_PRINT("Wrote: ");
            //DEBUG_PRINT(c);
          }
          else
          {
            c = 0;
          }
          EEPROM.write(i, c);
          si++;
        }
        EEPROM.end();
        Serial.println("Wrote " + sCmd);
      }
      else
      {
        Serial.println("Der Wert " + sCmd + " war keine Zahl!!!");
      }
    }
  }
  // Kalibrieren Temperatur 4 ////////////////////////////////
  else if (sPath == "/temp4")
    ////////////////////////////////////
    // format the html page for /tabelle
    ////////////////////////////////////
  {
    EEPROM.begin(512);
    delay(10);
    String temp4k = "";
    for (int i = 125; i < 130; i++)
    {
      temp4k += char(EEPROM.read(i));
    }
    EEPROM.end();
    Serial.print("Das ist die Zeitverschiebung: ");
    Serial.println(temp4k);
    // settings();
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Anpassung Vorlauf: ");
    sResponse += temp4k;
    sResponse += F("Grad C<BR>");
    sResponse += F("<form name=\"input\" action=\"\" method=\"get\">Vorlauf: <input type=\"text\" name=\"$\"><input type=\"submit\" value=\"Submit\"></form>");
    sResponse += F("<p>Temperatur kalibrieren: <a href=\"/temp1\"><button>Speicher oben</button></a>&nbsp;<a href=\"/temp2\"><button>Speicher mitte</button></a>&nbsp;<a href=\"/temp3\"><button>Speicher unten</button></a>&nbsp;<a href=\"/temp4\"><button>Vorlauf</button></a>&nbsp;</p>");

    sResponse2 = MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
    delay(100);

    //////////////////////
    // react on parameters
    //////////////////////
    if (sCmd.length() > 0)
    {
      // write received command to html page
      sResponse += "Kommando:" + sCmd + "<BR>";
      // SET THINGSPEAK API ////////////////////////////////


      if (sCmd.toInt() != 0)
      {

        Serial.print("Das wird gespeichert in der seite: ");
        Serial.println(sCmd);
        EEPROM.begin(512);
        delay(10);
        int si = 0;
        for (int i = 125; i < 130; i++)
        {
          char c;
          if (si < sCmd.length())
          {
            c = sCmd[si];
            //DEBUG_PRINT("Wrote: ");
            //DEBUG_PRINT(c);
          }
          else
          {
            c = 0;
          }
          EEPROM.write(i, c);
          si++;
        }
        EEPROM.end();
        Serial.println("Wrote " + sCmd);
      }
      else
      {
        Serial.println("Der Wert " + sCmd + " war keine Zahl!!!");
      }

    }
  }



  // Send the response to the client
  client.print(sHeader);
  client.print(sResponse);

  // and stop the client
  client.stop();
  Serial.println("Client disconnected");

}/* --(end main loop )-- */






/* ( THE END ) */

