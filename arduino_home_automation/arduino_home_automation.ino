/*
  Otthon automatizáció, első verzió. Arduino forráskód.
  A következő könyvtárak szükségesek a működéshez:
    *SimpleDHT.h-hőmérséklet és páratartalom szenzor 
    *LiquidCrystal_I2C.h-Hőmérséklet és páratartalom értékek kijelzése
    *Wire.h-I2C kommunikáció
    *Servo.h-szervómotor vezérlése
    *SR04.h-távolságmérő szenzor
    *Adafruit_CCS811.h-Levegőminőség mérése
*/
#include <SimpleDHT.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Servo.h>
#include "SR04.h"
#include "Adafruit_CCS811.h"

/* Ki- és bemenetek egyszerűbb kezelése makrók segítségével. */

#define BUTTON_LOW HIGH
#define BUTTON_HIGH LOW
#define WINDOW_MOTOR_PIN 9
#define SR04_ECHO 11
#define SR04_TRIG 12
#define DHT_PIN 2
#define MAX_TEMP 40
#define MIN_TEMP 10

/* Példányosítás, kezdőértékek beállítása*/

LiquidCrystal_I2C lcd(0x27,16,2);
SimpleDHT11 dht11;
Servo window;
SR04 sr04 = SR04(SR04_ECHO,SR04_TRIG);
Adafruit_CCS811 ccs;

byte temperature=0, humidity=0, data[40]={0}, desiredTemp=22, outsideTemp=15, tempFault=0;
unsigned long previousMillis=0, currentMillis=0, incPrevDebounceT=0, decPrevDebounceT=0;

int buttonInc = 7, buttonDec = 8;
int incButtonPrev=BUTTON_LOW, decButtonPrev=BUTTON_LOW;
int incButtonState, decButtonState;

unsigned int selectDisplayMode=0, prevDisplayMode=0, displayTurnOffCountdown=0;
bool toggleDisplay=false, powerDisplay=true;
int motorPos=0;

void setup(){
  /*Kívánt hőmérséklet változtató gombok és képernyő beállítása, szervómotor alaphelyzetbe hozása*/
  Serial.begin(9600);
  pinMode(buttonInc, INPUT_PULLUP);  
  pinMode(buttonDec, INPUT_PULLUP);
  lcd.init();
  lcd.clear();
  lcd.backlight();
  TempDisplay(0);
  window.attach(WINDOW_MOTOR_PIN);
  window.write(motorPos);
  /* Levegőminőség mérő szenzor inicializálása. */
  if(!ccs.begin(0x5A)){
    Serial.println("Failed to start air quality sensor! Please check your wiring.");
    while(1);
  }
  while(!ccs.available());
  double temp = ccs.calculateTemperature();
  ccs.setTempOffset(temp-25);
  
}
/*Hőmérséklet kijelző kezelő függvény*/
void TempDisplay(unsigned int val){
  selectDisplayMode=val;
  if(prevDisplayMode!=selectDisplayMode)
    lcd.clear();
  if(selectDisplayMode==0){
    lcd.setCursor(0,0);lcd.print("Temp:"+String(temperature)+"*C");
    lcd.setCursor(0,1);lcd.print("Hum:"+String(humidity)+"%");
  }
  else if(selectDisplayMode==1){
    lcd.setCursor(0,0);lcd.print("Temp:"+String(temperature)+"*C");
    lcd.setCursor(0,1);lcd.print("Des. temp:"+String(desiredTemp)+"*C");
    toggleDisplay=true;
  }
  else if(selectDisplayMode==2){
    lcd.setCursor(0,0);lcd.print("Temp. sensor");lcd.setCursor(0,1);lcd.print("error");
    toggleDisplay=true;
  }
  prevDisplayMode=selectDisplayMode;
}
/*Hőmérséklet kijelző ki- és bekapcsolása*/
void LcdTurnOn(){
  lcd.display();
  lcd.backlight();
  powerDisplay=true;
}
void LcdTurnOff(){
  lcd.noDisplay();
  lcd.noBacklight();
  powerDisplay=false;
}
/*Kívánt hőmérséklet növelése és csökkentése. A gombok szoftveresen pergésmentesítettek*/
void ControlDesiredTemp(){
  int incButtonReading = (digitalRead(buttonInc));
  int decButtonReading = (digitalRead(buttonDec));
  if(incButtonReading!=incButtonPrev){
    incPrevDebounceT=millis();
  }
  else if(decButtonReading!=decButtonPrev){
    decPrevDebounceT=millis();
  }
  
  if(((currentMillis-incPrevDebounceT)>100)||((currentMillis-decPrevDebounceT)>100)){
    if(incButtonReading!=incButtonState){
      incButtonState=incButtonReading;
      if(incButtonState==BUTTON_HIGH){
        if(desiredTemp<MAX_TEMP)
          desiredTemp++;
        TempDisplay(1);
      }
    }
    else if(decButtonReading!=decButtonState){
      decButtonState=decButtonReading;
      if(decButtonState==BUTTON_HIGH){
        if(desiredTemp>MIN_TEMP)
          desiredTemp--;
        TempDisplay(1);
      }
    }
  }
  incButtonPrev=incButtonReading;
  decButtonPrev=decButtonReading;
}
/*Hőmérséklet és páratartalom leolvasása a DHT11 szenzorról.*/
void GetTemp(){
  if(dht11.read(DHT_PIN, &temperature, &humidity, data)){
    if(tempFault<5)
      tempFault++;
    else if(tempFault>=5&&powerDisplay==true){
      TempDisplay(2);
    }
    return;
  }
  else{
    tempFault=0;
    Serial.print("Temperature: ");Serial.print(temperature);Serial.print("*C ");
    Serial.print("Humidity: ");Serial.print(humidity);Serial.println("%");
  }
}
/*Képernyő automatikus ki- és bekapcsolása távolság alapján.*/
void ControlLcdLight(){
    long sr04Distance=sr04.Distance();
    if((sr04Distance<=100||sr04Distance>=1000)&&powerDisplay==false){
      displayTurnOffCountdown=0;
      LcdTurnOn();
    }
    else if((sr04Distance>=100&&sr04Distance<=1000)&&powerDisplay==true){
      displayTurnOffCountdown++;
      if(displayTurnOffCountdown>=10)
        LcdTurnOff();
    }
}
void loop(){
  /*Hőmérséklet értékek beolvasása és kijelzése 4 másodpercenként*/
  currentMillis = millis();
  ControlDesiredTemp();
  if(currentMillis - previousMillis >= 4000){
    previousMillis = currentMillis;
    ControlLcdLight();
    GetTemp();
    if(powerDisplay==true){
      if(toggleDisplay==false)
        TempDisplay(0);
      else if(toggleDisplay==true)
        toggleDisplay=false;
    }
    /*Levegőminőség mérése és kijelzése "Serial Monitor" funkcióval*/
    if(ccs.available()){
      double temp = ccs.calculateTemperature();
      if(!ccs.readData()){
        Serial.println("CO2: "+String(ccs.geteCO2())+"ppm, TVOC: "+String(ccs.getTVOC())+"ppb");
      }
      else{
        Serial.println("Air quality sensor error.");
        ccs.readData();
      }   
    }
    Serial.println("===================================");
  }
  /*Motor-lényegében az ablak- pozícionálása, jelenleg félkész állapotban, a vezérlés finomításra szorul, illetve a külső hőmérsékletet biztosító forrással sincs összekötve a rendszer.*/
  if((temperature>=desiredTemp+2)&&temperature>outsideTemp){
    if(motorPos<178){
      motorPos=178;
      window.write(motorPos);
    }
  }
  else if((temperature<=desiredTemp-2)||temperature<outsideTemp){
    if(motorPos>2){
      motorPos=2;
      window.write(motorPos);
    }
  }
}
