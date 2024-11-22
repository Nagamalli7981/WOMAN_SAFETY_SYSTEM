/*1.gps tracker by using NE0-6M-0-001 GPS MODULE,SIM800L and Esp32.
  2.HTTPS POST request by using SIM800L.
  3.Attached with 0.96 inch OLED dislay and it can supports I2C communication protocol*/



#define TINY_GSM_MODEM_SIM800    // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024  //set bufeer


//gsm configuration
#define RXD2 16
#define TXD2 17

//gps configuration
#define RXD1 4
#define TXD1 2


//serial monitor
#define SerialMon Serial
#define SerialAT Serial2
#define TINY_GSM_DEBUG SerialMon

//indicatorse
#define redled 25
#define greenled 33
#define SMS_Button 32 



//gprs credintials
const char apn[] = "airtelgprs.com";  // Your APN
const char gprs_user[] = "";          // User
const char gprs_pass[] = "";          // Password
const char simPIN[] = "";


//api credentials
const char server[] = "api.templeadventures.com";
const char resource[] = "/api/gpsDevices/updatecurrentlocation";
const int port = 443;
unsigned long timeout;


String imei;
String Latitude, Longitude;
String requestBody;


//library files
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <TimeLib.h>
#include <AceButton.h>
#include "SSLClient.h"
#include "ca_cert.h"
#include <Wire.h>



using namespace ace_button;

//for emargency purpose
String message = "It's an Emergency. I'm at this location ";
String mobile_number = "+916301285589";
String message_with_data;



//sms button configuration
ButtonConfig config1;
AceButton sms_button(&config1);
void handleEvent_sms(AceButton*, uint8_t, uint8_t);

TinyGPSPlus gps;


#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif


//transport layer credentials
//Layers stack
TinyGsm sim_modem(SerialAT);
TinyGsmClient gsm_transpor_layer(modem, 0);
SSLClient secure_presentation_layer(&gsm_transpor_layer);
HttpClient client = HttpClient(secure_presentation_layer, server, port);


//for OLED screen
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1  // Reset pin
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define time_offset +0530

char ISO8601[] = "2000-00-00T00:00:00.000Z"; //year-month-date T hour:min:sec:micsecZ.ISO1608

byte last_second, Second, Minute, Hour, Day, Month;
int Year;


void setup() {                              
  pinMode(redled, OUTPUT);
  pinMode(greenled, OUTPUT);
  pinMode(SMS_Button, INPUT);


  SerialMon.begin(115200);
  delay(100);

  //for gsm module
  SerialAT.begin(115200, SERIAL_8N1, RXD2, TXD2);
  //for gps module
  Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);


  secure_presentation_layer.setCACert(root_ca);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  digitalWrite(redled, HIGH);
  digitalWrite(greenled, HIGH);
  delay(1000);
  digitalWrite(redled, LOW);
  digitalWrite(greenled, LOW);
  delay(1000);
  
  config1.setEventHandler(handleEvent_sms);
  sms_button.init(SMS_Button);

}

void loop() {


  //for setup the modem

  SerialMon.print("Initializing modem...");
  if (!sim_modem.init()) {
    SerialMon.print(" fail... restarting modem...");
    digitalWrite(redled, HIGH);
    //setupModem();
    // Restart takes quite some time
    // Use modem.init() if you don't need the complete restart
    if (!sim_modem.restart()) {
      digitalWrite(redled, HIGH);
      SerialMon.println(" fail... even after restart");
      return;
    }
  }


  SerialMon.println(" OK");
  // General information
  digitalWrite(redled, LOW);
  digitalWrite(greenled, HIGH);

  String name = sim_modem.getModemName();
  Serial.println("Modem Name: " + name);
  String modem_info = sim_modem.getModemInfo();
  Serial.println("Modem Info: " + modem_info);
  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && sim_modem.getSimStatus() != 3) {
    sim_modem.simUnlock(simPIN);
  }

  // Wait for network availability
  SerialMon.print("Waiting for network...");
  if (!sim_modem.waitForNetwork(240000L)) {

    SerialMon.println(" fail");
    digitalWrite(greenled, LOW);
    digitalWrite(redled, HIGH);
    delay(10000);
    return;
  }
  SerialMon.println(" OK");

  digitalWrite(redled, LOW);
  digitalWrite(greenled, HIGH);
  // Connect to the GPRS network
  SerialMon.print("Connecting to network...");
  if (!sim_modem.isNetworkConnected()) {
    SerialMon.println(" fail");
    digitalWrite(greenled, LOW);
    digitalWrite(redled, HIGH);
    delay(10000);
    return;
  }
  SerialMon.println(" OK");
  digitalWrite(redled, LOW);
  digitalWrite(greenled, HIGH);


  // Connect to APN
  SerialMon.print(F("Connecting to APN: "));
  SerialMon.print(apn);
  if (!sim_modem.gprsConnect(apn, gprs_user, gprs_pass)) {
    SerialMon.println(" fail");
    digitalWrite(greenled, LOW);
    digitalWrite(redled, HIGH);
    delay(10000);
    return;
  }
  SerialMon.println(" OK");
  digitalWrite(redled, LOW);
  digitalWrite(greenled, HIGH);

  // More info..
  Serial.println("");
  String ccid = sim_modem.getSimCCID();
  Serial.println("CCID: " + ccid);
  imei = sim_modem.getIMEI();
  Serial.println("IMEI: " + imei);
  String cop = sim_modem.getOperator();
  Serial.println("Operator: " + cop);
  IPAddress local = sim_modem.localIP();
  Serial.println("Local IP: " + String(local));
  int csq = sim_modem.getSignalQuality();
  Serial.println("Signal quality: " + String(csq));
  gpsdata();
  
  sms_button.check();


}



void httpspostreq() {
  if (!modem.isGprsConnected()) {
    DBG("... not connected");
    digitalWrite(greenled, LOW);
    digitalWrite(redled, HIGH);
  }

  else {
    DBG("Connecting to ", server);
    // Make a HTTPS POST request:
    digitalWrite(greenled, LOW);
    digitalWrite(redled, LOW);
    Serial.println("Making POST request securely");
    String contentType = "Content-Type: application/json";
    String postData = requestBody;
    client.post(resource, contentType, postData);
    int status_code = client.responseStatusCode();
    String response = client.responseBody();
    Serial.print("Status code: ");
    Serial.println(status_code);
    Serial.print("Response: ");
    Serial.println(response);
    if (status_code == 200) {
      digitalWrite(redled, LOW);
      digitalWrite(greenled, HIGH);

    } else {
      digitalWrite(greenled, LOW);
      digitalWrite(redled, HIGH);
    }


    display.clearDisplay();
    display.setTextSize(1.5);
    display.setTextColor(WHITE);
    display.setCursor(0, 5);
    display.println("Status code: ");
    display.setTextSize(1);
    display.setCursor(70, 5);
    display.println(status_code);

    display.setTextSize(1.5);
    display.setTextColor(WHITE);
    display.setCursor(0, 25);
    display.println("Responce: ");
    display.setTextSize(1);
    display.setCursor(60, 25);
    display.println(response);
    display.display();
    delay(300);

    client.stop();
  }

  SerialMon.println();
  client.stop();
  SerialMon.println(F("Server disconnected"));
  modem.gprsDisconnect();
  SerialMon.println(F("GPRS disconnected"));
}


void gpsdata() {
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;) {
    while (Serial1.available()) {
      if (gps.encode(Serial1.read())) {
        newData = true;
        break;
      }
    }
  }
  if (true) {
    newData = false;

    Latitude = String(gps.location.lat(), 6);  // Latitude in degrees (double)
    SerialMon.print("latitude: ");
    SerialMon.println(Latitude);
    Longitude = String(gps.location.lng(), 6);
    SerialMon.print("longitude: ");
    SerialMon.println(Longitude);




    if (gps.time.isValid()) {
      Minute = gps.time.minute();
      Second = gps.time.second();
      Hour = gps.time.hour();
    }

    // get date drom GPS module
    if (gps.date.isValid()) {
      Day = gps.date.day();
      Month = gps.date.month();
      Year = gps.date.year();
    }

    if (last_second != gps.time.second())  // if time has changed
    {
      last_second = gps.time.second();

      // set current UTC time
      setTime(Hour, Minute, Second, Day, Month, Year);
      // add the offset to get local time
      adjustTime(time_offset);

      ISO8601[2] = (year() / 10) % 10 + '0';
      ISO8601[3] = year() % 10 + '0';
      ISO8601[5] = month() / 10 + '0';
      ISO8601[6] = month() % 10 + '0';
      ISO8601[8] = day() / 10 + '0';
      ISO8601[9] = day() % 10 + '0';
      ISO8601[11] = hour() / 10 + '0';
      ISO8601[12] = hour() % 10 + '0';
      ISO8601[14] = minute() / 10 + '0';
      ISO8601[15] = minute() % 10 + '0';
      ISO8601[17] = second() / 10 + '0';
      ISO8601[18] = second() % 10 + '0';

      Serial.println(ISO8601);  // print ISO STANDARD TIME STAMP
    }
  }



  display.clearDisplay();
  display.setTextSize(1.5);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.println("LATITUDE: ");
  display.setTextSize(1);
  display.setCursor(60, 5);
  display.println(Latitude);


  display.setTextSize(1.5);
  display.setTextColor(WHITE);
  display.setCursor(0, 25);
  display.println("LONGITUDE: ");
  display.setTextSize(1);
  display.setCursor(65, 25);
  display.println(Longitude);
  display.display();
  delay(300);


  display.setTextSize(1.5);
  display.setTextColor(WHITE);
  display.setCursor(0, 45);
  display.println("TIMESTAMP: ");
  display.setTextSize(1);
  display.setCursor(65, 45);
  display.println(ISO8601);
  display.display();
  delay(300);


  //json formate
  StaticJsonDocument<200> doc;
  doc["lat"] = Latitude;
  doc["long"] = Longitude;
  doc["device_id"] = imei;
  doc["time_stamp"] = ISO8601;
  serializeJson(doc, requestBody);
  SerialMon.println(requestBody);

  httpspostreq();

  requestBody = "";
}

void handleEvent_sms(AceButton* /* button */, uint8_t eventType,
                     uint8_t /* buttonState */) {
  switch (eventType) {
    case AceButton::kEventPressed:
      // Serial.println("kEventPressed");
      message_with_data = message + "Latitude = " + (String)Latitude + "Longitude = " + (String)Longitude;
      modem.sendSMS(mobile_number, message_with_data);
      message_with_data = "";
      break;
    case AceButton::kEventReleased:
      //Serial.println("kEventReleased");
      break;
  }
}