#include <AsyncEventSource.h>
#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include <AsyncWebSynchronization.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>

#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>

#include "defines.h"
#include "Credentials.h"
#include "dynamicParams.h"

//#include <ESPAsync_WiFiManager_Lite.h>

/*WifiManager*/
#define USE_DYNAMIC_PARAMETERS      false

//bool LOAD_DEFAULT_CONFIG_DATA = true;
//ESP_WM_LITE_Configuration defaultConfig;
ESPAsync_WiFiManager_Lite* ESPAsync_WiFiManager; 

/*NTP*/
const char* ntpServer ="de.pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0; //3600;
struct tm   tmLast;

/*OTA & WEB*/
AsyncWebServer server(80);
const char* AsyncUser = "admin";
const char* AsyncPWD = "!parsival";
AsyncWebSocket ws("/ws");

// env variable for Rest API
bool pumpeState=0;

/*Rest API*/
StaticJsonDocument<250> jsonDocument;
char buffer[250];
void create_json(char *tag, float value, char *unit) {  
  jsonDocument.clear();
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);  
}
 
void add_json_object(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit; 
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "application/json", "{\"message\":\"Not found\"}");
}

void getPumpe() {
  create_json("pumpe",pumpeState,"Toggle");
  //server.send(200, "application/json", buffer);
  server.on("/pumpe", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buffer);  
  });
}

void setup_routing() {
  //server.on("/pumpe", getPumpe);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"message\":\"Welcome\"}");
  });

  server.on("/get-message", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<100> data;
    if (request->hasParam("message")) {
      data["message"] = request->getParam("message")->value();
    } else {
      data["message"] = "No message parameter";
    }
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
  });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/pumpe", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    StaticJsonDocument<200> data;
    //serializeJsonPretty(jsonObj, Serial);
    //Serial.println();
    serializeJsonPretty(json, Serial);
    Serial.println();
    deserializeJson(data,json);
    int newPumpState=data["value"];
    Serial.println("type value unit");
//    Serial.println(data["type"]);
//    Serial.println(data["value"]);
//    Serial.println(data["unit"]);
    pumpeState = newPumpState;
    Serial.printf("PUMPE=%d\n",pumpeState);
    setPumpe(pumpeState);
    
    request->send(200, "application/json", "{}");
  });
  server.addHandler(handler);
  server.onNotFound(notFound);
  getPumpe();
}


/*WIFI*/
const char* ssid = "B600IT";
const char* password = "Ehl95W23";

const char* host = "Druck";

/*test
const int LED_BUILTIN = 2;
*/

/*RELAY*/
const int RELAIS = 26;

/*POTI*/
const int POTI = 36;

/*LCD*/
//const int SDA = 21; //Standard SDA PIN
//const int SCL = 22; //Standard SCL PIN
const int LINE0 = 0; //IP-Info
const int LINE1 = 1; //Min Max Time
const int LINE2 = 2; //Ein/Aus
const int LINE3 = 3; //P akt Prozent on/off ON<Poti%
//const int LINE0 = 2;
//const int LINE1 = 1;
//const int LINE2 = 3;
//const int LINE3 = 0;

const int lcdColumns = 20;
const int lcdRows = 4;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows); 
//char lines[5][40] = {{"11111111111111111111"},{"22222222222222222222"},{"33333333333333333333"},{"44444444444444444444"},{"                    "}};
char lines[4][21] = {{"                    "},{"                    "},{"                    "},{"                    "}};
byte charMIN[] = {
  B10001,
  B10001,
  B11011,
  B11011,
  B01110,
  B01110,
  B00100,
  B00100
};
const byte MINCHAR=byte(1);
byte charMAX[] = {
  B00100,
  B00100,
  B01110,
  B01110,
  B11011,
  B11011,
  B10001,
  B10001
  };
const byte MAXCHAR=byte(2);
byte charDELTA[] = {
  B00000,
  B00000,
  B00100,
  B01010,
  B01010,
  B11111,
  B00000,
  B00000
  };
const byte DELTACHAR=byte(3);

/*PUSHBUTTON*/
const int PUSHBUTTON = 35;

/*DruckSENSOR*/
const int PRESSURE = 34;
int cPressure = 0;
const double PSI2BAR = 0.0689476;
const int MAXADC = 4095; // höchter wert, wenn 
int ADCNull = 0; // normaler Luftdruck 14,696 PSI
int ADCMaximal = 0;
double ADCDelta = 1.0; // der Wert von ADCDelta = ADCMaximal-ADCNull entspricht 100%
unsigned long SecondsBetweenOnOff = 0;
unsigned long cMillis = 0;
unsigned long LastMillis = 0;
unsigned long LastOnOff=0;
int getPoti=0;
int getValue=0;


// Pumpe ON/OFF
int PotiProzent=0;
bool PUMPE=0;

void lcdDisplay() {
  lcdLine(0, lines[LINE0]);
  lcdLine(1, lines[LINE1]);
  lcdLine(2, lines[LINE2]);
  lcdLine(3, lines[LINE3]);
  //Serial.printf("[%s]\n[%s]\n[%s]\n[%s]\n",lines[LINE0],lines[LINE1],lines[LINE2],lines[LINE3]);
}

void lcdLine(int idx,char *out) {
  byte len;
  if(((String )out).length()>lcdColumns) {
    out[lcdColumns]=0;
  }
  /*
  for(int i=0;i<((String )out).length()-1;i++) {
    if (out[i]<32) {
      out[i]=' ';
    }
  }*/
  lcd.setCursor(0, idx);
  len=lcd.printf("%s",out);
}

void setPumpe(bool state) {
    Serial.printf("%ld %ld\n",cMillis/1000,LastOnOff/1000);
    if(SecondsBetweenOnOff*1000>cMillis-LastOnOff) {
      Serial.println("wait");
      return;
    } else { 
      Serial.println("toogle");
      LastOnOff = cMillis;
    }
    pumpeState=state;
    getPumpe();
    if(pumpeState) {
      sprintf(lines[LINE2],"Pumpe: An                    ");
      digitalWrite(RELAIS,HIGH);
      lcd.backlight();
    } else {
      sprintf(lines[LINE2],"Pumpe: Aus                    ");
      digitalWrite(RELAIS,LOW);
      lcd.noBacklight();
      ADCMaximal=0;
      ADCNull=0;
      if(PotiProzent==17) {
        ESPAsync_WiFiManager->clearConfigData();
        sprintf(lines[LINE0],"WLAN: ->192.168.4.1                    ");
        delay(500);
        ESP.restart();
      }
    }
    //delay(1000);
    //Serial.printf("%d\n",pumpeState);
}

void printLocalTime() {
  struct tm timeinfo;
  time_t myTime;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    sprintf(&lines[LINE1][12],"--:--:--");
    return;
  }
  if (memcmp(&timeinfo,&tmLast,sizeof(timeinfo))!=0) {
    //Serial.printf("%02d:%02d:%02d\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    tmLast=timeinfo;
    /*
    lcd.setCursor(18, 0);
    lcd.printf("%02d",timeinfo.tm_hour);
    lcd.setCursor(18, 1);
    lcd.printf("%02d",timeinfo.tm_min);
    lcd.setCursor(18, 2);
    lcd.printf("%02d",timeinfo.tm_sec);
    */
    //lcd.setCursor(12, 1);
    //Serial.printf("%02d:%02d:%02d\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    //lcd.printf("%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    sprintf(&lines[LINE1][12],"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    //Serial.printf("%s\n",lines[1]);
  }
  /*
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");
  Serial.printf("Sommerzeit=%d\n",timeinfo.tm_isdst);
  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
  myTime=mktime(&timeinfo);
  struct tm ts2; 
   localtime_r(&myTime, &ts2);
  Serial.print("Time obtained in struct from myTime is: ");
  Serial.println(&ts2, "%A, %B %d %Y %H:%M:%S");
  */
}

void setupNTP() {
  configTime(0,0, ntpServer);
  setenv("TZ",TIMEZONE,1);
  tzset();
  printLocalTime();
}

void setupLCD() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(MINCHAR,charMIN);
  lcd.createChar(MAXCHAR,charMAX);
  lcd.createChar(DELTACHAR,charDELTA);
}

void setupRELAIS() {
  pinMode(RELAIS,OUTPUT);
  pumpeState=0;
  SecondsBetweenOnOff=atoi(SECONDSBETWEEN_ONOFF);
  setPumpe(pumpeState);
}

void setupPoti() {
  ///
}


void setupWifi() {
  Serial.print(F("\nStarting ESPAsync_WiFi using "));
  Serial.print(FS_Name);
  Serial.print(F(" on "));
  Serial.println(ARDUINO_BOARD);
  Serial.println(ESP_ASYNC_WIFI_MANAGER_LITE_VERSION);

#if USING_MRD
  Serial.println(ESP_MULTI_RESET_DETECTOR_VERSION);
#else
  Serial.println(ESP_DOUBLE_RESET_DETECTOR_VERSION);
#endif

  ESPAsync_WiFiManager = new ESPAsync_WiFiManager_Lite();
  String AP_SSID = host;
  String AP_PWD  = "configuration";
  // Set customized AP SSID and PWD
  ESPAsync_WiFiManager->setConfigPortal(AP_SSID, AP_PWD);
  ESPAsync_WiFiManager->setConfigPortalChannel(0);
  

#if USING_CUSTOMS_STYLE
  ESPAsync_WiFiManager->setCustomsStyle(NewCustomsStyle);
#endif

#if USING_CUSTOMS_HEAD_ELEMENT
  ESPAsync_WiFiManager->setCustomsHeadElement(PSTR("<style>html{filter: invert(10%);}</style>"));
#endif

#if USING_CORS_FEATURE
  ESPAsync_WiFiManager->setCORSHeader(PSTR("Your Access-Control-Allow-Origin"));
#endif

  // Set customized DHCP HostName
  //Or use default Hostname "ESP_XXXXXX"
  ESPAsync_WiFiManager->begin();
  //ESPAsync_WiFiManager->begin(host);
  //Serial.println("");
  //Serial.print("Connected to ");
  //Serial.println(ESPAsync_WiFiManager->getWiFiSSID(0));
  //Serial.print("IP address: ");
  //Serial.println(ESPAsync_WiFiManager->localIP());
}

void heartBeatPrint()
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED) {
    //Serial.print("H");        // H means connected to WiFi
    sprintf(lines[LINE0],"%s                  ",ESPAsync_WiFiManager->localIP());
  }
  else
  {
    if (ESPAsync_WiFiManager->isConfigMode()) {
      //Serial.print("C");        // C means in Config Mode
      sprintf(lines[LINE0],"%s                  ",ESPAsync_WiFiManager->localIP());
    }
    else {
      Serial.print("F");        // F means not connected to WiFi
      sprintf(lines[LINE0],"IP: ---.---.---.---         ");
    }
  }

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(F(" "));
  }
}

void check_status()
{
  static unsigned long checkstatus_timeout = 0;

  //KH
#define HEARTBEAT_INTERVAL    20000L
  // Print hearbeat every HEARTBEAT_INTERVAL (20) seconds.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = millis() + HEARTBEAT_INTERVAL;
  }
}

void setupOTA() {
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  AsyncElegantOTA.begin(&server); //,AsyncUser,AsyncPWD);
  setup_routing();
  server.begin();
}

void setup() {
/*  // put your setup code here, to run once:
*/
  Serial.begin(115200);

  /*
  if (!SPIFFS.begin(true,"/spiffs")) {
    Serial.println("SPIFFS.begin ERROR");
  } else {
    if (SPIFFS.exists("/spiffs/settings.json")) {
      Serial.println("/spiffs/settings.json OK!!!");
      getPumpe();
      File fp=SPIFFS.open("/spiffs/settings.json",FILE_WRITE,true);
      if(!fp) {
        Serial.println("SPIFFS.open ERROR");
      } else {
        fp.print(buffer);
        fp.close();
        // reopen
        fp=SPIFFS.open("/spiffs/settings.json",FILE_READ,false);
        while (fp.available()) {
          Serial.write(fp.read());
        }
        Serial.println();
        fp.close();
      }    } else {
      File fp=SPIFFS.open("/spiffs/settings.json",FILE_WRITE,true);
      if(!fp) {
        Serial.println("SPIFFS.open ERROR");
      } else {
        fp.close();
      }
    }  
  }
  */
  setupLCD();
  setupWifi();
  setupOTA();
  setupNTP();
  setupRELAIS();

  //Serial.printf("TIMEZONE is %s\n",TIMEZONE);

  //pinMode(PRESSURE,INPUT);
  // Idee: Über Loop 1-1000 den aktuellen Wert von ADCNull ermitteln
  /*for(int i=0;i<1000;i++) {
    int tmpVal=analogRead(PRESSURE);
    if(ADCNull<tmpVal) {
      ADCNull=tmpVal;
    }
  }*/
  ADCNull=1;

  sprintf(lines[LINE1]," %04d      ",ADCNull);
  lines[LINE1][0]=MAXCHAR;
  }

void loop() {
  getPoti = analogRead(POTI);
  getValue = analogRead(PRESSURE);
  cMillis = millis();
  PotiProzent = (int)((float)getPoti*100.0/4095.0);
  /*if (LastMillis=0) {
    LastMillis=millis();
  } else {
    LastMillis=millis()-LastMillis;
  }*/
  ESPAsync_WiFiManager->run();
  check_status();

  if(analogRead(PUSHBUTTON)!=0) {
    setPumpe(!pumpeState);
    }
  
  //server.handleClient();

  if(pumpeState) {
    if(getValue>ADCNull) {
      if(cPressure!=getValue) {
        cPressure=getValue;
        if((getValue>ADCMaximal) && (getValue>ADCNull)) {
          ADCMaximal=getValue;
          ADCDelta=ADCMaximal-ADCNull;
        }
        if((getValue<ADCMaximal)) {
          if(ADCNull==0 || (getValue<ADCNull)) {
            ADCNull=getValue;
            ADCDelta=ADCMaximal-ADCNull;
          }
        }
        sprintf(lines[LINE1],"V%04d %04d  ",(int)ADCDelta,(cPressure-ADCNull));
        lines[LINE1][0]=DELTACHAR;
        if (ADCDelta>0) {
          if((((cPressure-ADCNull)*100)/ADCDelta)<PotiProzent) {
            sprintf(lines[LINE3],"P:ON %d%% on<%d%%                  ",(int)(((cPressure-ADCNull)*100)/ADCDelta),PotiProzent);
            digitalWrite(RELAIS,HIGH);
          } else {
            sprintf(lines[LINE3],"P:OFF %d%% on<%d%%                 ",(int)(((cPressure-ADCNull)*100)/ADCDelta),PotiProzent);
            digitalWrite(RELAIS,LOW);
          }
        }
      }
    }
  } else {
    sprintf(lines[LINE3],"P:--- on<%d%%                 ",PotiProzent);
    if((getValue>ADCMaximal) && (getValue>ADCNull)) {
      ADCMaximal=getValue;
      ADCDelta=ADCMaximal-ADCNull;
    }
    if((getValue<ADCMaximal)) {
      if(ADCNull==0 || (getValue<ADCNull)) {
        ADCNull=getValue;
        ADCDelta=ADCMaximal-ADCNull;
      }
    }
    sprintf(lines[LINE1],"-%04d+%04d  ",(int)ADCNull,ADCMaximal);
    lines[LINE1][0]=MINCHAR;
    lines[LINE1][5]=MAXCHAR;
  }

  printLocalTime();

  lcdDisplay();
}
