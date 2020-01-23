/*
  Otthon automatizáció, első verzió. Wemos D1 Mini forráskód.
  A következő könyvtárak szükségesek a működéshez:
    *ESP8266WiFi.h-Adatok fogadása és küldése, csatlakozás a helyi wi-fi hálózathoz
    *WiFiClientSecure.h-Biztonságos kapcsolódás az API-hoz
    *ArduinoJson.h-Json fájlok kezelése
    *EEPROM.h-A mikrovezérlő eeprom memóriájának kezelése
*/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

/*Változók inicializálása, osztályok példányosítása.*/
unsigned long currentMillis, previousMillis;
const char host[16] = "api.darksky.net";
char latitude[8] = "46.2530", longitude[8] = "20.1414"; //Helyadatok megadása, ennek megadása majd későbbiekben egy másik felületről fog történni
float Temperature,Pressure,Humidity,weatherStationDistance,windSpeed,windGust;
unsigned int windBearing;
const char *weatherDescription;
char api_key[33];
unsigned long unixTime;
byte offset;
char ssid[33]="In Tyler We Trust", pass[33]="spacemonkey";
//char *ssid,*pass;

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("");
  getAK();
  WiFi.mode(WIFI_STA);
  connectToNetwork(ssid, pass);
  delay(1000);
  if(!getWeatherData(Humidity,Pressure,Temperature,weatherStationDistance,unixTime,offset,windSpeed,windGust,windBearing)){
    displayConditions(Temperature,Humidity,Pressure,weatherDescription,weatherStationDistance,windSpeed,windGust,windBearing);
    calculateTime(unixTime,offset);
  }
}

/*API-kulcs tárolása az eeprom memóriában.*/
void storeAK() {
  char temp[33]="";
  EEPROM.begin(512);
  if(strcmp(api_key,"")){
    if(strcmp(EEPROM.get(0, temp),api_key)) {
      EEPROM.put(0, api_key);
      Serial.println("Your API-key has been changed.");
    }
    else {
      Serial.println("API-key match.");
    }
  }
  else {
    Serial.println("API-key null.");
  }
  EEPROM.end();
}
/*API-kulcs kiolvasása.*/
void getAK() {
  EEPROM.begin(512);
  EEPROM.get(0, api_key);
  EEPROM.end();
}
/*Csatlakozás a hálózathoz.*/
void connectToNetwork(const char *ssid, const char *pass) {
  Serial.println("==================");
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  Serial.print("Succesfully connected to: ");
  Serial.println(ssid);
}
/*Unix idő konvertálása standard formátumú időre.*/
void calculateTime(unsigned long ut,byte offset) {
  byte hours=0;
  byte minutes=0;
  unsigned long elapsedDays=ut/86400;
  unsigned long seconds=elapsedDays*86400;
  unsigned long currentYear=1970;
  byte currentMonth=1;
  byte standardMonths[12]= {31,28,31,30,31,30,31,31,30,31,30,31};
  byte leapMonths[12]= {31,29,31,30,31,30,31,31,30,31,30,31};
  while(true) {
    if(elapsedDays<365) {
      elapsedDays++;
      break;
    }
    if(((currentYear%4==0)&&(currentYear%100!=0))||((currentYear%100==0)&&(currentYear%400==0))) {
      elapsedDays-=366;
    }
    else {
      elapsedDays-=365;
    }
    currentYear++;
  } 
  for(int i=0;i<12;i++) {
    if(((currentYear%4==0)&&(currentYear%100!=0))||((currentYear%100==0)&&(currentYear%400==0))) {
      if(elapsedDays<leapMonths[i])
        break;
      elapsedDays-=leapMonths[i];
    }
    else {
      if(elapsedDays<standardMonths[i])
        break;
      elapsedDays-=standardMonths[i];
    }
    currentMonth++;
  }
  seconds=ut-seconds;
  hours=seconds/3600;
  seconds=seconds-hours*3600;
  minutes=seconds/60;
  seconds=seconds-minutes*60;
  hours+=offset;
  Serial.println("-----------------");
  Serial.print("Date of request: ");Serial.print(currentYear);Serial.print(".");Serial.print(currentMonth);Serial.print(".");Serial.print(elapsedDays);Serial.print(". ");
  Serial.print(hours);Serial.print(":");Serial.print(minutes);Serial.print(":");Serial.println(seconds);
}
/*API-hoz való csatlakozás, API-hívás, kapott Json fájl feldolgozása, adatok tárolása.*/
int getWeatherData(float& humidity,float& pressure,float& temperature,float &wsDistance, unsigned long &ut,byte &offset, float &windspeed, float &windgust, unsigned int &windbearing) {
  WiFiClientSecure client;
  String result;
  bool isJson = false;
  const char *exclude = "minutely,hourly,daily,alerts", *units = "si", *language = "en"; //alerts,flags
  const char *fingerprint = "EA C3 0B 36 E8 23 4D 15 12 31 9B CE 08 51 27 AE C1 1D 67 2B";
  client.setFingerprint(fingerprint);
  if (!client.connect(host, 443)) {
    Serial.println("======================================");
    Serial.println("Connection failed.");
    return 1;
  }
  else {
    Serial.println("======================================");
    Serial.println("Successfully connected to Dark Sky!");
  }
  if (client.verify(fingerprint, host)) {
    Serial.println("-----------------");
    Serial.println("Certificate matches.");
  } 
  else {
    client.stop();
    Serial.println("-----------------");
    Serial.println("Certificate doesn't match, terminating connection.");
    return 2;
  }
  client.print("GET ");
  client.print("https://api.darksky.net/forecast/"); client.print(api_key);client.print("/"); client.print(latitude); client.print(","); client.print(longitude);
  client.print("?exclude="); client.print(exclude); client.print("&units="); client.print(units); client.print("&lang="); client.print(language);
  client.print(" HTTP/1.1\r\nHost:"); client.print(host); client.print("\r\nConnection: close\r\n\r\n");

  while (client.connected() && !client.available())
    delay(1);
  while (client.connected() || client.available()) {
    char recievedChar = client.read();
    if (recievedChar == '{' && isJson == false)
      isJson = true;
    if (isJson == true)
    {
      if(recievedChar=='\n')
      {
        result=result+'\0';
        break;
      }
      else
        result = result + recievedChar;
    }
  }
  client.stop();
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, result);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
  }
  if(doc["error"]!=nullptr) {
    unsigned int request_err_code=doc["code"];
    const char* request_err=doc["error"];
    Serial.println("-----------------");
    Serial.println("Your request returned with the following error:");
    Serial.print(request_err_code);Serial.print(", ");Serial.println(request_err);
    return 3;
  }
  else if(doc["flags"]["darksky-unavailable"]!=nullptr) {
    Serial.println("-----------------");
    Serial.println("Your local weatherstation is currently unavailable.");
    return 4;
  }
  else {
    Serial.println("-----------------");
    Serial.println("Local data is available!");
  }
  ut = doc["currently"]["time"];
  temperature = doc["currently"]["apparentTemperature"];
  humidity = doc["currently"]["humidity"];humidity = humidity*100;
  pressure = doc["currently"]["pressure"];
  weatherDescription = doc["currently"]["summary"];
  wsDistance = doc["flags"]["nearest-station"];
  windspeed = doc["currently"]["windSpeed"];
  windgust = doc["currently"]["windGust"];
  windbearing = doc["currently"]["windBearing"];
  offset = doc["offset"];
  return 0;
}

/*Kapott értékek kijelzése.*/

void displayConditions(float temperature, float humidity, float pressure, const char* description,float wsDistance,float windspeed,float windgust,unsigned int windbearing) {
  Serial.println("==================");
  Serial.print("Weather description: ");Serial.println(description);
  Serial.println("-----------------");
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println("[*C] ");
  Serial.print("Relative humidity: "); Serial.print(int(humidity)); Serial.println("[%] ");
  Serial.print("Pressure: "); Serial.print(pressure); Serial.println("[hPa]");
  Serial.println("-----------------");
  Serial.print("Wind speed: ");Serial.print(windspeed);Serial.println("[m/s]");
  Serial.print("Wind gust: ");Serial.print(windgust);Serial.println("[m/s]");
  if(windspeed) {
    Serial.print("Wind bearing: ");Serial.print(windbearing);Serial.println("[degrees]");
  }
  Serial.println("-----------------");
  Serial.print("Distance from the nearest weather station: "); Serial.print(wsDistance); Serial.println("[km]");
}
void loop() {
  currentMillis = millis();
  /*Minden két percben történik API-hívás, mert havonta 1000 ingyen API-hívás igénylése lehetséges.*/
  if ((currentMillis - previousMillis >= 120000)) {
      if(!getWeatherData(Humidity,Pressure,Temperature,weatherStationDistance,unixTime,offset,windSpeed,windGust,windBearing)) {
        displayConditions(Temperature,Humidity,Pressure,weatherDescription,weatherStationDistance,windSpeed,windGust,windBearing);
        calculateTime(unixTime,offset);
      }
      previousMillis=currentMillis;
  }
}
