#include <M5Stack.h>

//#include <SPI.h>
#include <Usb.h>
#include <hiduniversal.h>
#include <hidboot.h>
#include <usbhub.h>
//#include <ESP8266WiFi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include "config.h" // Remove if not using
#include "logo.h"

#define TFTW            320     // screen width
#define TFTH            240     // screen height
#define TFTW2           160     // half screen width
#define TFTH2           120     // half screen height

const char* ssid = SSID; //replace this with your wifi  name
const char* password = WIFI_PASSWORD; //replace with your wifi password
const char* mqttServer = MQTT_SERVER; //replace with your mqtt broker IP
const int mqttPort = 1883;
const char* mqttUser = MQTT_USER; //replace with your mqtt user name
const char* mqttPassword = MQTT_PASSWORD; //replace with your mqtt password 
const long utcOffsetInSeconds = 25200; // utc + 7 hour

class KbdRptParser : public KeyboardReportParser
{
    void PrintKey(uint8_t mod, uint8_t key);
  protected:
    virtual void OnKeyDown  (uint8_t mod, uint8_t key);
    virtual void OnKeyPressed(uint8_t key);
};

USB     Usb;
USBHub     Hub(&Usb);
HIDUniversal Hid(&Usb);
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    Keyboard(&Usb);
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

String scan;
String barcode;
bool new_barcode;
int status = WL_IDLE_STATUS;
KbdRptParser Prs;




void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  uint8_t c = OemToAscii(mod, key);

  if (c)
    OnKeyPressed(c);
}

void KbdRptParser::OnKeyPressed(uint8_t key)
{

  if(key!=13) {
    scan += ((char) key);
  }
  else {
    new_barcode = true;
    barcode = scan;
    // Serial.println(scan);
    scan = ((String) "");
  }
}

void setup(){

  // Initialize the M5Stack object
  M5.begin();
  //M5.Power.begin();
  M5.Lcd.setSwapBytes(true);
  WiFi.hostname("M5Stack_scanner");
  WiFi.begin(ssid, password);

  M5.Lcd.pushImage(0, 0, imageWidth, imageHeight, logo);
  // Connect to Wifi 
  M5.Lcd.setCursor(0,100);
  while (WiFi.status() != WL_CONNECTED)
  {
  delay(500);
  M5.Lcd.print(".");
  }
  M5.Lcd.setTextSize(2);
  M5.Lcd.println(""); 
  M5.Lcd.println("Connected to WiFi"); 
  
  timeClient.begin();
  M5.Lcd.println("Time client started"); 

  
  client.setServer(mqttServer, 1883 ); //default port for mqtt is 1883
  M5.Lcd.println("MQTT Started");
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Barcode 2 MQTT");
 
  delay(1000);
  
  if (Usb.Init() == -1) {
   M5.Lcd.setTextSize(2);
   M5.Lcd.println("USB Host Init Error");
   M5.Lcd.println("Will reboot in 2 Sec");
   delay(2000);
   ESP.restart();
   } 
  else { 
   M5.Lcd.setCursor( TFTW2 - (9*9), TFTH2 - 6);
   M5.Lcd.setTextSize(3); 
   M5.Lcd.println("Ready");
  }

 Hid.SetReportParser(0, (HIDReportParser*)&Prs);
 delay(200);
}

// the loop routine runs over and over again forever
void loop() {
 Usb.Task();
 //Serial.println(wifiClient.localIP());
  if ( !client.connected() )
 {
  reconnect();
 }

  if (new_barcode)
 {
  
  MQTTPOST();
  
 }
 
}

void MQTTPOST()
{
// update time
timeClient.update();
unsigned long epochTime = timeClient.getEpochTime();
struct tm *ptm = gmtime ((time_t *)&epochTime);
int currentYear = ptm->tm_year+1900;
int currentMonth = ptm->tm_mon+1;
int monthDay = ptm->tm_mday;
String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);

String IP_address = wifiClient.localIP().toString();

//payload formation begins here
String time = '"' + currentDate + " " + timeClient.getFormattedTime()+ '"';


// String barcode = barcode;

M5.Lcd.setBrightness(100);
String payload ="{";
payload +="\"time\":"; 
payload +=time;
payload +=",";
payload +="\"Barcode\":";
payload += barcode;
payload +=",";
payload +="\"Broker IP\":";
payload += wifiClient.remoteIP().toString();
payload +=",";
payload +="\"IP Address\":";
payload += wifiClient.localIP().toString();
payload +=",";
payload +="\"RSSI dBm\":";
payload += WiFi.RSSI();
payload +="";
payload +="}";

char attributes[1000];
payload.toCharArray( attributes, 1000 );
client.publish("Test Barcode/M5Stack_scanner", attributes); //topic="test" MQTT data post command.
Serial.println( attributes );
M5.Lcd.fillScreen(TFT_BLACK);
M5.Lcd.setTextSize(2); 
M5.Lcd.pushImage(0, 0, imageWidth, imageHeight, logo);
M5.Lcd.setCursor(0,100);
M5.Lcd.println("Time:");
M5.Lcd.println(time);
M5.Lcd.println("Barcode:");
M5.Lcd.println(barcode);
new_barcode = false;
barcode = ((String) "");
}

//this function helps you reconnect wifi as well as broker if connection gets disconnected.
void reconnect() 
{
while (!client.connected()) {
status = WiFi.status();
if ( status != WL_CONNECTED) {
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
M5.Lcd.setTextSize(1); 
M5.Lcd.print(".");
}
M5.Lcd.fillScreen(TFT_BLACK);
M5.Lcd.setTextSize(2); 
M5.Lcd.setCursor(10,10); 
M5.Lcd.println("Connected to AP");
}
M5.Lcd.setTextSize(1); 
M5.Lcd.println("Connecting to Broker...");
M5.Lcd.println(mqttServer);

if ( client.connect("ESP8266 Device", mqttUser, mqttPassword) )
{
M5.Lcd.fillScreen(TFT_BLACK);
M5.Lcd.setTextSize(2); 
M5.Lcd.setCursor(10,10);  
M5.Lcd.println("[Ready]" );
M5.Lcd.setBrightness(0);
}
else {
M5.Lcd.setTextSize(1);   
M5.Lcd.println( " : retrying in 5 seconds]" );
delay( 5000 );
}
}
}
