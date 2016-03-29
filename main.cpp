#include <WiFiManager.h>

#include "main.h"

// storage for Measurements; keep some mem free; allocate remainder
#define KEEP_MEM_FREE 10240
#define MEAS_SPAN_H 24
unsigned long ulMeasCount = 0;  // values already measured
unsigned long ulNoMeasValues = 0; // size of array
unsigned long ulMeasDelta_ms;   // distance to next meas time
unsigned long ulNextMeas_ms;    // next meas time
unsigned long *pulTime;         // array for time points of measurements
float *pfTemp1, *pfTemp2, *pfTemp3, *pfTemp4;         // array for temperature and humidity measurements
int temp1, temp2, temp3, temp4;
int temp11, temp22, temp33, temp44;
int sent = 0;
float prevTemp1 = 0, prevTemp2 = 0, prevTemp3 = 0, prevTemp4 = 0;
#define myPeriodic 15 //in sec | Thingspeak pub is 15sec
#define ONE_WIRE_BUS1 5
#define ONE_WIRE_BUS2 12
#define ONE_WIRE_BUS3 4
#define ONE_WIRE_BUS4 14


OneWire oneWire1(ONE_WIRE_BUS1);
OneWire oneWire2(ONE_WIRE_BUS2);
OneWire oneWire3(ONE_WIRE_BUS3);
OneWire oneWire4(ONE_WIRE_BUS4);

DallasTemperature DS18B201(&oneWire1);
DallasTemperature DS18B202(&oneWire2);
DallasTemperature DS18B203(&oneWire3);
DallasTemperature DS18B204(&oneWire4);

unsigned long ulReqcount;
unsigned long ulReconncount;    // how often did we connect to WiFi

// ntp timestamp
unsigned long ulSecs2000_timer = 0;

// Create an instance of the server on Port 80
WiFiServer server(80);
WiFiClient client;

String apiKey = "7XQL253GLO49GANL";
const char* tsserver = "api.thingspeak.com";

void mainsetup()
{
  // setup globals
  ulReqcount = 0;
  ulReconncount = 0;
  WiFiStart();
  delay(1);

  server.begin();

  // allocate ram for data storage
  uint32_t free = system_get_free_heap_size() - KEEP_MEM_FREE;
  ulNoMeasValues = free / (sizeof(float) * 5 + sizeof(unsigned long)); // humidity & temp --> 2 + time
  pulTime = new unsigned long[ulNoMeasValues];
  pfTemp1 = new float[ulNoMeasValues];
  pfTemp2 = new float[ulNoMeasValues];
  pfTemp3 = new float[ulNoMeasValues];
  pfTemp4 = new float[ulNoMeasValues];

  if (pulTime == NULL || pfTemp1 == NULL || pfTemp2 == NULL || pfTemp3 == NULL || pfTemp4 == NULL)
  {
    ulNoMeasValues = 0;
    Serial.println("Error in memory allocation!");
  }
  else
  {
    Serial.print("Allocated storage for ");
    Serial.print(ulNoMeasValues);
    Serial.println(" data points.");

    float fMeasDelta_sec = MEAS_SPAN_H * 3600. / ulNoMeasValues;
    ulMeasDelta_ms = ( (unsigned long)(fMeasDelta_sec + 0.5) ) * 1000; // round to full sec
    Serial.print("Measurements will happen each ");
    Serial.print(ulMeasDelta_ms);
    Serial.println(" ms.");

    ulNextMeas_ms = millis() + ulMeasDelta_ms;
  }
}

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
  ulSecs2000_timer = getNTPTimestamp() + 3600 +  zeit.toInt();
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
    sTable = "<table style=\"width:100%\"><tr><th>Zeit / UTC</th><th>T1 &deg;C</th><th>T2 &deg;C</th><th>T3 &deg;C</th><th>T4 &deg;C</th></tr>";
    sTable += "<style>table, th, td {border: 2px solid black; border-collapse: collapse;} th, td {padding: 5px;} th {text-align: left;}</style>";
    for (unsigned long li = ulMeasCount; li > ulEnd; li--)
    {
      unsigned long ulIndex = (li - 1) % ulNoMeasValues;
      sTable += "<tr><td>";
      sTable += epoch_to_string(pulTime[ulIndex]).c_str();
      sTable += "</td><td>";
      sTable += pfTemp1[ulIndex];
      sTable += "</td><td>";
      sTable += pfTemp2[ulIndex];
      sTable += "</td><td>";
      sTable += pfTemp3[ulIndex];
      sTable += "</td><td>";
      sTable += pfTemp4[ulIndex];
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
      sTable += pfTemp1[ulIndex];
      sTable += ",";
      sTable += pfTemp2[ulIndex];
      sTable += ",";
      sTable += pfTemp3[ulIndex];
      sTable += ",";
      sTable += pfTemp4[ulIndex];
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

  sResponse  = F("<FONT SIZE=-2><BR>Aufrufz&auml;hler=");
  sResponse += ulReqcount;
  sResponse += F(" - Verbindungsz&auml;hler=");
  sResponse += ulReconncount;
  sResponse += F(" - Freies RAM=");
  sResponse += (uint32_t)system_get_free_heap_size();
  sResponse += F(" - Max. Datenpunkte=");
  sResponse += ulNoMeasValues;
  sResponse += F("<BR>SMASE 03/2016 | Tobias Winter -> Mail: tobias.winter90@gmail.com <BR></body></html>");

  return (sResponse);
}

void sendTeperatureTS(int temp1, int temp2, int temp3, int temp4)
{
  WiFiClient client;

  if (client.connect(tsserver, 80)) { // use ip 184.106.153.149 or api.thingspeak.com
    Serial.println("WiFi Client connected_neu ");

    EEPROM.begin(512);
    delay(10);
    String apiKey = "";
    for (int i = 81; i < 97; i++)
    {
      apiKey += char(EEPROM.read(i));
    }
    EEPROM.end();
    Serial.println("Thingspeak-API_neu: " + apiKey);

    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(temp1);
    postStr += "&field2=";
    postStr += String(temp2);
    postStr += "&field3=";
    postStr += String(temp3);
    postStr += "&field4=";
    postStr += String(temp4);
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
    Serial.println("Daten an Thingspeak gesendet");
  }//end if
  sent++;
  client.stop();
}//end send

void ds18b20() {
  //int temp1, temp2, temp3, temp4;
  //char buffer[10];
  DS18B201.requestTemperatures();
  DS18B202.requestTemperatures();
  DS18B203.requestTemperatures();
  DS18B204.requestTemperatures();
  temp11 = DS18B201.getTempCByIndex(0);
  temp22 = DS18B202.getTempCByIndex(0);
  temp33 = DS18B203.getTempCByIndex(0);
  temp44 = DS18B204.getTempCByIndex(0);


  EEPROM.begin(512);
  delay(10);
  String temp1k = "";
  String temp2k = "";
  String temp3k = "";
  String temp4k = "";
  for (int i = 110; i < 115; i++)
  {
    temp1k += char(EEPROM.read(i));
  }
  for (int i = 115; i < 120; i++)
  {
    temp2k += char(EEPROM.read(i));
  }
  for (int i = 120; i < 125; i++)
  {
    temp3k += char(EEPROM.read(i));
  }
  for (int i = 125; i < 130; i++)
  {
    temp4k += char(EEPROM.read(i));
  }
  EEPROM.end();
  Serial.println("Kalibrierung T1: " + temp1k);
  Serial.println("Kalibrierung T2: " + temp2k);
  Serial.println("Kalibrierung T3: " + temp3k);
  Serial.println("Kalibrierung T4: " + temp4k);


  temp1 = temp11 + temp1k.toInt();
  temp2 = temp22 + temp2k.toInt();
  temp3 = temp33 + temp3k.toInt();
  temp4 = temp44 + temp4k.toInt();
  //String tempC = dtostrf(temp, 4, 1, buffer);//handled in sendTemp()
  Serial.print(String(sent) + " Temperature_neu1: ");
  Serial.println(temp1);
  Serial.println("Temperature_neu2: ");
  Serial.println(temp2);
  Serial.println("Temperature_neu3: ");
  Serial.println(temp3);
  Serial.println("Temperature_neu4: ");
  Serial.println(temp4);

  NTP();
}


void mainloop()
{
  ///////////////////
  // do data logging
  ///////////////////
  if (millis() >= ulNextMeas_ms)
  {
    ds18b20();
    sendTeperatureTS(temp1, temp2, temp3, temp4);

    ulNextMeas_ms = millis() + ulMeasDelta_ms;

    pfTemp1[ulMeasCount % ulNoMeasValues] = temp1;
    pfTemp2[ulMeasCount % ulNoMeasValues] = temp2;
    pfTemp3[ulMeasCount % ulNoMeasValues] = temp3;
    pfTemp4[ulMeasCount % ulNoMeasValues] = temp4;
    pulTime[ulMeasCount % ulNoMeasValues] = millis() / 1000 + ulSecs2000_timer;

    Serial.print("Logging Temperature1: ");
    Serial.print(pfTemp1[ulMeasCount % ulNoMeasValues]);
    Serial.print(" deg Celsius - Temperature2: ");
    Serial.print(pfTemp2[ulMeasCount % ulNoMeasValues]);
    Serial.print(" deg Celsius - Temperature3: ");
    Serial.print(pfTemp3[ulMeasCount % ulNoMeasValues]);
    Serial.print(" deg Celsius - Temperature4: ");
    Serial.print(pfTemp4[ulMeasCount % ulNoMeasValues]);
    Serial.print(" deg Celsius - Time: ");
    Serial.println(pulTime[ulMeasCount % ulNoMeasValues]);

    ulMeasCount++;
  }

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client)
  {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  unsigned long ultimeout = millis() + 250;
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
   /* sResponse += pfTemp1[iIndex];
    sResponse += F(",temp2=");
    sResponse += pfTemp2[iIndex];
    sResponse += F(",temp3=");
    sResponse += pfTemp3[iIndex];
    sResponse += F(";\ngoogle.load('visualization', '1', {packages: ['gauge']});google.setOnLoadCallback(drawgaugetemp1);google.setOnLoadCallback(drawgaugetemp2)");
    sResponse += F(";google.setOnLoadCallback(drawgaugetemp3);");
    sResponse += F("\nvar gaugetemp1Options = {min: 0, max: 100, greenFrom: 50, greenTo:75, yellowFrom: 75, yellowTo: 90,redFrom: 90, redTo: 100, minorTicks: 10, majorTicks: ['0','10','20','30','40','50','60','70','80','90','100']};\n");
    sResponse += F("var gaugetemp2Options = {min: 0, max: 100, greenFrom: 50, greenTo:75, yellowFrom: 75, yellowTo: 90,redFrom: 90, redTo: 100, minorTicks: 10, majorTicks: ['0','10','20','30','40','50','60','70','80','90','100']};\n");
    sResponse += F("var gaugetemp3Options = {min: 0, max: 100, greenFrom: 50, greenTo:75, yellowFrom: 75, yellowTo: 90,redFrom: 90, redTo: 100, minorTicks: 10, majorTicks: ['0','10','20','30','40','50','60','70','80','90','100']};\n");
    sResponse += F("var gaugetemp1,gaugetemp2,gaugetemp3,gaugetemp4;\n\nfunction drawgaugetemp1() {\ngaugetemp1Data = new google.visualization.DataTable();\n");
    sResponse += F("gaugetemp1Data.addColumn('number', '\260C');\ngaugetemp1Data.addRows(1);\ngaugetemp1Data.setCell(0, 0, temp1);\ngaugetemp1 = new google.visualization.Gauge(document.getElementById('gaugetemp1_div'));\ngaugetemp1.draw(gaugetemp1Data, gaugetemp1Options);\n}\n\n");
    sResponse += F("function drawgaugetemp2() {\ngaugetemp2Data = new google.visualization.DataTable();\ngaugetemp2Data.addColumn('number', '\260C');\ngaugetemp2Data.addRows(1);\ngaugetemp2Data.setCell(0, 0, temp2);\ngaugetemp2 = new google.visualization.Gauge(document.getElementById('gaugetemp2_div'));\ngaugetemp2.draw(gaugetemp2Data, gaugetemp2Options);\n}\n");
    sResponse += F("function drawgaugetemp3() {\ngaugetemp3Data = new google.visualization.DataTable();\ngaugetemp3Data.addColumn('number', '\260C');\ngaugetemp3Data.addRows(1);\ngaugetemp3Data.setCell(0, 0, temp3);\ngaugetemp3 = new google.visualization.Gauge(document.getElementById('gaugetemp3_div'));\ngaugetemp3.draw(gaugetemp3Data, gaugetemp3Options);\n}\n");
    sResponse += F("</script>\n
    
    */
    sResponse += F("</head>\n<body>\n<font color=\"#000000\"><body bgcolor=\"#d0d0f0\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>SMASE<BR><BR><FONT SIZE=+1>Letzte Messung um ");
    //sResponse += epoch_to_string(pulTime[iIndex]).c_str();
    //sResponse += F(" UTC<BR>\n");
    sResponse += F("Seite: Temperaturen -> Zeigt die gemessenen Temperaturen an <br>");
    sResponse += F("Seite: Grafik -> Zeigt den Temperaturverlauf (Diagramm) der letzten 24 h an <br>");
        sResponse += F("Seite: Grafik -> Zeigt den Temperaturverlauf (Tabelle) der letzten 24 h an <br>");
                sResponse += F("Seite: Settings -> Einstellungen <br>");
    //sResponse += F("</fieldset>");
    sResponse += F(" <div style=\"clear:both;\"></div>");
    sResponse2 = F("<p>Temperaturverlauf - Seiten laden l&auml;nger:<BR>  <a href=\"/anzeige\"><button>Temperaturen</button></a>    <a href=\"/grafik\"><button>Grafik</button></a>     <a href=\"/tabelle\"><button>Tabelle</button></a>     <a href=\"/settings\"><button>Settings</button></a></p>");
    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }
  
/*
  ///////////////////////////
  // format the html response
  ///////////////////////////
  // Startseite ////////////////////////////////
 // String sResponse, sResponse2, sHeader;
  if (sPath == "/test")
  {
    ulReqcount++;
    int iIndex = (ulMeasCount - 1) % ulNoMeasValues;
    sResponse  = F("<html>\n<head>\n<title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title>\n<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n<script type=\"text/javascript\">\n");
/*    sResponse += F(" var temp1=");
    sResponse += pfTemp1[iIndex];
    sResponse += F(",temp2=");
    sResponse += pfTemp2[iIndex];
    sResponse += F(",temp3=");
    sResponse += pfTemp3[iIndex];
    sResponse += F(",temp4=");
    sResponse += pfTemp4[iIndex];
    */
    /*
    sResponse += F("google.charts.load('current', {'packages':['gauge']});\n");
    sResponse += F("google.charts.setOnLoadCallback(drawGauge);\n"); 
    sResponse += F("\nvar gaugeOptions = {min: 0, max: 100, greenFrom: 50, greenTo:75, yellowFrom: 75, yellowTo: 90,redFrom: 90, redTo: 100, minorTicks: 10, majorTicks: ['0','10','20','30','40','50','60','70','80','90','100']};\n");
    sResponse += F("var gauge;\n");
    sResponse += F("function drawgauge() {gaugeData = new google.visualization.DataTable();\n");
    sResponse += F("gaugeData.addColumn('number', 'P. oben \260C');/n");
    sResponse += F("gaugeData.addColumn('number', 'P. mitte \260C');/n");
    sResponse += F("gaugeData.addColumn('number', 'P. unten \260C');/n");
    sResponse += F("gaugeData.addColumn('number', 'Hz VL \260C');/n");
    sResponse += F("gaugeData.addRows(2);/n");
    sResponse += F("gaugeData.setCell(0, 0, 10);\n");
    sResponse += F("gaugeData.setCell(0, 1, 20);\n");
    sResponse += F("gaugeData.setCell(0, 2, 30);\n");
    sResponse += F("gaugeData.setCell(0, 3, 40);\n");
    sResponse += F("gauge = new google.visualization.Gauge(document.getElementById('gauge_div'));\n");
    sResponse += F("gauge.draw(gaugeData, gaugeOptions);\n}\n");
    sResponse += F("</script>\n</head>\n<body>\n<font color=\"#000000\"><body bgcolor=\"#d0d0f0\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>SMASE<BR><BR><FONT SIZE=+1>Letzte Messung um ");
    sResponse += epoch_to_string(pulTime[iIndex]).c_str();
    sResponse += F(" UTC<BR>\n");
    sResponse += F("<fieldset><legend>Pufferspeicher</legend>");
    sResponse += F(" <div id=\"gauge_div\" style=\"float:left; width:160px; height: 640px;\"></div>\n");
    sResponse += F("</fieldset>");
    sResponse += F(" <div style=\"clear:both;\"></div>");
    sResponse2 = F("<p>Temperaturverlauf - Seiten laden l&auml;nger:<BR>  <a href=\"/lauf\"><button>Vorlauf</button></a>    <a href=\"/grafik\"><button>Grafik</button></a>     <a href=\"/tabelle\"><button>Tabelle</button></a>     <a href=\"/settings\"><button>Settings</button></a></p>");
    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }

*/
  // Startseite ////////////////////////////////
  // String sResponse, sResponse2, sHeader;
 // if (sPath == "/lauf")
  if (sPath == "/anzeige")
  {
    ulReqcount++;
    int iIndex = (ulMeasCount - 1) % ulNoMeasValues;
  //  sResponse  = F("<html>\n<head>\n<title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title>\n<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n<script type=\"text/javascript\">\n");
    sResponse  = F("<html>\n<head>\n<title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title>\n<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n");
    sResponse += F("<script type=\"text/javascript\">\ngoogle.charts.load('current', {'packages':['gauge']});\n");
    sResponse += F("  google.charts.setOnLoadCallback(drawGauge); \n");
  sResponse += F("\nvar gaugeOptions = {min: 0, max: 100, greenFrom: 50, greenTo:75, yellowFrom: 75, yellowTo: 90,redFrom: 90, redTo: 100, minorTicks: 10, majorTicks: ['0','10','20','30','40','50','60','70','80','90','100']};\n");
  //  sResponse += F("  var gaugeOptions = {min: 0, max: 280, yellowFrom: 200, yellowTo: 250, redFrom: 250, redTo: 280, minorTicks: 5}; \n");
    sResponse += F("   var gauge; \n");
    sResponse += F("  function drawGauge() { \n");
    sResponse += F("       gaugeData = new google.visualization.DataTable();  \n");
     //   sResponse += F("gaugeData.addColumn('number', 'P. oben \260C');/n");
  //  sResponse += F("gaugeData.addColumn('number', 'P. mitte \260C');/n");
   // sResponse += F("gaugeData.addColumn('number', 'unten');/n");
 //   sResponse += F("gaugeData.addColumn('number', 'Hz VL \260C');/n");
    sResponse += F("       gaugeData.addColumn('number', 'oben');  \n");
    sResponse += F("       gaugeData.addColumn('number', 'mitte');  \n");
    sResponse += F("       gaugeData.addColumn('number', 'unten');  \n");
        sResponse += F("       gaugeData.addColumn('number', 'vorlauf');  \n");
    sResponse += F("       gaugeData.addRows(2);  \n");
    sResponse += F("       gaugeData.setCell(0, 0, ");
    sResponse += pfTemp1[iIndex];
   // sResponse += F(",temp2=");
  //  120);  \n");
      sResponse += F(" ); \n");
    sResponse += F("   gaugeData.setCell(0, 1, ");
        sResponse += pfTemp2[iIndex];
        sResponse += F(" ); \n");
    sResponse += F("   gaugeData.setCell(0, 2, ");
        sResponse += pfTemp3[iIndex];
        sResponse += F(" ); \n");
     sResponse += F("  gaugeData.setCell(0, 3, ");
         sResponse += pfTemp4[iIndex];
         sResponse += F(" ); \n");
    sResponse += F("       gauge = new google.visualization.Gauge(document.getElementById('gauge_div'));  \n");
    sResponse += F("  gauge.draw(gaugeData, gaugeOptions);  \n");
    sResponse += F("  } \n");
 //   sResponse += F(" function changeTemp(dir) {  \n");
  //  sResponse += F("        gaugeData.setValue(0, 0, gaugeData.getValue(0, 0) + dir * 25); \n");
  //  sResponse += F("        gaugeData.setValue(0, 1, gaugeData.getValue(0, 1) + dir * 20); \n");
 //   sResponse += F("         gauge.draw(gaugeData, gaugeOptions);\n");
  //  sResponse += F("  } \n");
    sResponse += F("  </script> \n  </head> \n <body> \n<font color=\"#000000\"><body bgcolor=\"#d0d0f0\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>SMASE<BR><BR><FONT SIZE=+1>Letzte Messung um ");
     sResponse += epoch_to_string(pulTime[iIndex]).c_str();
    sResponse += F(" UTC<BR>\n");
    sResponse += F("<fieldset><legend>Pufferspeicher</legend>");
    sResponse += F("    <div id=\"gauge_div\" style=\"width:140px; height: 560px;\"></div> \n");
        sResponse += F("</fieldset>");
  //  sResponse += F("    <input type=\"button\" value=\"Go Faster\" onclick=\"changeTemp(1)\" /> \n");
  //  sResponse += F("   <input type=\"button\" value=\"Slow down\" onclick=\"changeTemp(-1)\" />  \n");
      sResponse += F(" <div style=\"clear:both;\"></div>");
    sResponse2 = F("<p>Temperaturverlauf - Seiten laden l&auml;nger:<BR>  <a href=\"/lauf\"><button>Vorlauf</button></a>    <a href=\"/grafik\"><button>Grafik</button></a>     <a href=\"/tabelle\"><button>Tabelle</button></a>     <a href=\"/settings\"><button>Settings</button></a></p>");
  
  //  sResponse += F(" </body> \n </html>  \n");
    
    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }
  // Tabelle ////////////////////////////////
  else if (sPath == "/tabelle")
    ////////////////////////////////////
    // format the html page for /tabelle
    ////////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Letzte Messungen im Abstand von ");
    sResponse += ulMeasDelta_ms;
    sResponse += F("ms<BR>");
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
  else if (sPath == "/grafik")
    ///////////////////////////////////
    // format the html page for /grafik
    ///////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeList(&client, false); // get size of list first

    sResponse  = F("<html>\n<head>\n<title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['corechart']}]}\"></script>\n");
    sResponse += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);\nfunction drawChart() {var data = google.visualization.arrayToDataTable([\n['Zeit / UTC', 'Temperatur1', 'Temperatur2', 'Temperatur3', 'Temperatur4'],\n");
    // here the big list will follow later - but let us prepare the end first

    // part 2 of response - after the big list
    sResponse2  = F("]);\nvar options = {title: 'Verlauf',\n");
    sResponse2 += F("curveType:'function',legend:{ position: 'bottom'}};");
    sResponse2 += F("var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));chart.draw(data, options);}\n</script>\n</head>\n");
    sResponse2 += F("<body>\n<font color=\"#000000\"><body bgcolor=\"#d0d0f0\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1><a href=\"/\"><button>Startseite</button></a><BR>");
    sResponse2 += F("<BR>\n<div id=\"curve_chart\" style=\"width: 600px; height: 400px\"></div>");
    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client - delete strings after use to keep mem low
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
    Serial.println("Thinkspeak: " + apiKey);

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

    sResponse  = F("<html><head><title>WLAN Logger f&uuml;r Pufferspeichertemperatur</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>WLAN Logger f&uuml;r Pufferspeichertemperatur</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\"><button>Startseite</button></a><BR><BR>Thingspeak API ist momentan: ");
    sResponse += apiKey;
    sResponse += F("<BR><BR>Das ist die Zeitverschiebung: ");
    sResponse += zeittext;
    sResponse += F("<fieldset><legend>EEPROM Setting</legend>");
    sResponse += F("<p><a href=\"/reset\"><button>RESET</button></a>&nbsp;<a href=\"?pin=FUNCTION2ON\"><button>AUSLESEN</button></a></p>");
    sResponse += F("</fieldset>");
    sResponse += F("<fieldset><legend>Allgemein Setting</legend>");
    sResponse += F("<p>Zeitumstellung <a href=\"?pin=SOMMERZEIT\"><button>SOMMERZEIT</button></a>&nbsp;<a href=\"?pin=WINTERZEIT\"><button>WINTERZEIT</button></a></p>");
    sResponse += F("<form name=\"input\" action=\"\" method=\"get\">THINGSPEAK-API: <input type=\"text\" name=\"$\"><input type=\"submit\" value=\"Submit\"></form>");
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

}
