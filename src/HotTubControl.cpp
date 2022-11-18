#include "HotTubControl.h"

void setup() {
  Serial.begin(76800);
  bootstrapManager.bootstrapSetup(manageDisconnections, manageHardwareButton, callback);
  bootstrapManager.setMQTTWill(tubStatusTopic.c_str(),"\"offline\"",1,false,true);
  
  // DS18B20 one wire temperature sensor
  sensors.begin();

  Serial.print("tubCommandTopic: ");
  Serial.println(tubCommandTopic);
  Serial.print("tubStatusTopic: ");
  Serial.println(tubStatusTopic);


  pinMode(HEAT,OUTPUT);
  pinMode(PUMPLOW,OUTPUT);
  pinMode(PUMPHIGH,OUTPUT);
  pinMode(TEMPERATURE,INPUT);

  pinMode(PRESSURE,INPUT_PULLUP);
  pinMode(STATUS_PUMPLOW,INPUT_PULLUP);
  pinMode(STATUS_PUMPHIGH,INPUT_PULLUP);
  pinMode(STATUS_HEAT,INPUT_PULLUP);
  pinMode(D4,OUTPUT);

  attachInterrupt(PRESSURE,isrPressureDetected,CHANGE);
  attachInterrupt(STATUS_HEAT,isrHeat,CHANGE);
  attachInterrupt(STATUS_PUMPLOW,isrPumpLow,CHANGE);
  attachInterrupt(STATUS_PUMPHIGH,isrPumpHigh,CHANGE);
}

void loop() {
  currentMillis = millis();

  if (isConfigFileOk){
    // Bootsrap loop() with Wifi, MQTT and OTA functions
    bootstrapManager.bootstrapLoop(manageDisconnections, manageQueueSubscription, manageHardwareButton);

    if(!setupComplete){
      setupComplete = true;
      // Broadcast that we are online
      bootstrapManager.publish(tubStatusTopic.c_str(), "\"online\"", false);

    }
  }

  // read the temp sensor + actual relay status from optocoupler and broadcast on mqtt
  statusDebounceLoop();
  statusUpdateLoop();
  updateLED(); // D4 LED flashes when reconnecting, and stays on when connected

  // Pressure detected, so update lastPressure time
  if(statusPressure) lastPressureMillis = currentMillis;

  // PUMP ON
  // if last pump run more than target millis ago (run the pump every 1 hour for example)
  // or if manually commanded to turn on when setting a new target temperature
  if(currentMillis - lastPressureMillis > pumpInterval || newTargetTemp){
    digitalWrite(PUMPLOW,HIGH);
    lastPumpStartMillis = currentMillis;
    newTargetTemp = false;
    // statusPumpLow = true; // rely on optocoupler reading for now
  }

  // PUMP OFF
  // if pump on AND was running for at least minRunTime AND heat off AND heat off for pumpPrePostRunDuration
  if(digitalRead(PUMPLOW) == HIGH && currentMillis - lastPumpStartMillis > pumpMinRunTime && digitalRead(HEAT) == LOW && currentMillis - lastHeatOffMillis > pumpPrePostRunDuration){
    digitalWrite(PUMPLOW,LOW);
    // statusPumpLow = false; // rely on optocoupler reading for now
  }


  // HEAT ON
  // if pressure AND pump has been running for at least preRunMillis AND temp is lower than cut in target
  if(statusPressure && currentMillis - lastPumpStartMillis > pumpPrePostRunDuration && actualTemperature < targetTemperature - tempCutOffset){
    digitalWrite(HEAT,HIGH);
  }

  // HEAT OFF
  // if no pressure OR actual is hotter than cut off target)
  if(!statusPressure || actualTemperature > targetTemperature + tempCutOffset){
    if(digitalRead(HEAT) == HIGH){
      lastHeatOffMillis = currentMillis;
    }
    digitalWrite(HEAT,LOW);
  }

}


void updateLED(){
  static unsigned int lastLED = 0;

  if(WiFi.status() == WL_CONNECTED){
    // turn led on to indicate wifi connected
    digitalWrite(D4,LOW);
    setupComplete = true;
  }else{
    if(currentMillis - lastLED < 500){
      digitalWrite(D4,HIGH);
    }else{
      digitalWrite(D4,LOW);

      if(currentMillis - lastLED > 1000){
        lastLED = currentMillis;
      }      
    }
  }
}

void statusUpdateLoop(){
  static long unsigned int lastStatusMillis = 0;
  static bool lastStatusPressure = false;
  static bool lastStatusPumpLow = false;
  static bool lastStatusPumpHigh = false;
  static bool lastStatusHeat = false;
  
  if(millis() > (lastStatusMillis + statusInterval) ){
    updateTemperatureLoop();

    lastStatusMillis = millis();
    Serial.println("Current Status:");
    
    Serial.print(" Temperature: ");
    Serial.println(actualTemperature);
        
    Serial.print(" Pressure: ");
    Serial.println(statusPressure);
    
    Serial.print(" Pump Low|High: ");
    Serial.print(statusPumpLow);
    Serial.print("|");
    Serial.println(statusPumpHigh);

    Serial.print(" Heat: ");
    Serial.println(statusHeat);
    
    Serial.println("Targets:");
    Serial.print(" Temperature: ");
    Serial.println(targetTemperature);

    Serial.print(" Heat: ");
    Serial.println(digitalRead(HEAT));

    Serial.print(" Pump Low|High: ");
    Serial.print(digitalRead(PUMPLOW));
    Serial.print("|");
    Serial.println(digitalRead(PUMPHIGH));
    Serial.println("===========");

    bootstrapManager.publish(String(tubStatusTopic + "/temperature").c_str(), String(actualTemperature).c_str(), false);
    // bootstrapManager.publish(String(tubStatusTopic + "/pressure").c_str(), String(statusPressure).c_str(), false);
    // bootstrapManager.publish(String(tubStatusTopic + "/pump_low").c_str(), String(statusPumpLow).c_str(), false);
    // bootstrapManager.publish(String(tubStatusTopic + "/pump_high").c_str(), String(statusPumpHigh).c_str(), false);
    // bootstrapManager.publish(String(tubStatusTopic + "/heat").c_str(), String(statusHeat).c_str(), false);

    bootstrapManager.publish(String(tubTargetTopic + "/temperature").c_str(), String(targetTemperature).c_str(), false);
    bootstrapManager.publish(String(tubTargetTopic + "/pump_low").c_str(), String(digitalRead(PUMPLOW)).c_str(), false);
    bootstrapManager.publish(String(tubTargetTopic + "/pump_high").c_str(), String(digitalRead(PUMPHIGH)).c_str(), false);
    bootstrapManager.publish(String(tubTargetTopic + "/heat").c_str(), String(digitalRead(HEAT)).c_str(), false);

  }

  if(lastStatusPressure != statusPressure){
    Serial.print("Pressure Status Change to: ");
    Serial.println(statusPressure);
    bootstrapManager.publish(String(tubStatusTopic + "/pressure").c_str(), String(statusPressure).c_str(), false);
  }

  if(lastStatusPumpLow != statusPumpLow){
    Serial.print("Pump Low Status Change to: ");
    Serial.println(statusPumpLow);
    bootstrapManager.publish(String(tubStatusTopic + "/pump_low").c_str(), String(statusPumpLow).c_str(), false);
  }

  if(lastStatusPumpHigh != statusPumpHigh){
    Serial.print("Pump High Status Change to: ");
    Serial.println(statusPumpHigh);
    bootstrapManager.publish(String(tubStatusTopic + "/pump_high").c_str(), String(statusPumpHigh).c_str(), false);
  }

  if(lastStatusHeat != statusHeat){
    Serial.print("Heat Status Change to: ");
    Serial.println(statusHeat);
    bootstrapManager.publish(String(tubStatusTopic + "/heat").c_str(), String(statusHeat).c_str(), false);
  }

  lastStatusPressure = statusPressure;
  lastStatusPumpLow = statusPumpLow;
  lastStatusPumpHigh = statusPumpHigh;
  lastStatusHeat = statusHeat;
}

void IRAM_ATTR isrPressureDetected(){
  lastStatusPressureMillis = currentMillis;
}

void IRAM_ATTR isrPumpLow(){
  lastStatusPumpLowMillis = currentMillis;
}

void IRAM_ATTR isrPumpHigh(){
  lastStatusPumpHighMillis = currentMillis;
}

void IRAM_ATTR isrHeat(){
  lastStatusHeatMillis = currentMillis;
}

void statusDebounceLoop(){
  if(currentMillis - lastStatusPressureMillis > 500){
    statusPressure = !digitalRead(PRESSURE);
  }else if(500 - (currentMillis - lastStatusPressureMillis) > 450){
    Serial.print("deboucing pressure for: ");
    Serial.println(450 - (currentMillis - lastStatusPressureMillis));
  }

  if(currentMillis - lastStatusHeatMillis > 500){
    statusHeat = digitalRead(STATUS_HEAT);
  }

  if(currentMillis - lastStatusPumpLowMillis > 500){
    statusPumpLow = digitalRead(STATUS_PUMPLOW);
  }

  if(currentMillis - lastStatusPumpHighMillis > 500){
    statusPumpHigh = digitalRead(STATUS_PUMPHIGH);
  }
}

void updateTemperatureLoop(){
  // static unsigned int lastMillis = 0;
  // unsigned int currentMillis = millis();

  // if(currentMillis - lastMillis > 5000){
    sensors.requestTemperatures();
    float tempReading = sensors.getTempFByIndex(0);
    if(tempReading > -100){
      actualTemperature = tempReading;
    }else{
      actualTemperature = 200; // prevent the heater from running
      Serial.println("Temperature sensor error!");
    }
    // lastMillis = currentMillis;

    // Serial.print("temp reading: ");
    // Serial.println(actualTemperature);
  // }
}

void callback(char *topic, byte *payload, unsigned int length){
  Serial.println("mqtt callback");

  String topicPrefix = tubCommandTopic;
  topicPrefix.remove(topicPrefix.length() - 1,1);

  // for (int i=0;i<length;i++) {
  //   // Serial.print((char)payload[i]);
  //   Serial.println(payload[i], HEX);
  // }


  if (String(topic) == String(topicPrefix + "temperature")){
    char rxVal[length];
    Serial.println("MQTT: set the temperature");

    for(unsigned int i=0; i<length; i++){
      rxVal[i] = payload[i];
    }

    targetTemperature = atof(rxVal);

    // safety to avoid target temperature from being set too high or too low
    if(targetTemperature > tempUpperLimit || targetTemperature < tempLowerLimit){
      targetTemperature = 65;
    }

    Serial.print("Set targetTemperature: ");
    Serial.println(targetTemperature);
    newTargetTemp = true;
  }

}
void manageDisconnections(){
  setupComplete = false;
  digitalWrite(D4,HIGH); // turn LED off
}
void manageQueueSubscription(){
    bootstrapManager.subscribe(tubCommandTopic.c_str());
    Serial.print("Subscribed to ");
    Serial.println(tubCommandTopic);
}
void manageHardwareButton(){

}