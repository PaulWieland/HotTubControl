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


  pinMode(HEAT1,OUTPUT);
  pinMode(HEAT2,OUTPUT);
  pinMode(PUMPLOW,OUTPUT);
  pinMode(PUMPHIGH,OUTPUT);
  pinMode(TEMPERATURE,INPUT);

  pinMode(PRESSURE,INPUT_PULLUP);
  pinMode(STATUS_PUMPLOW,INPUT_PULLUP);
  pinMode(STATUS_PUMPHIGH,INPUT_PULLUP);
  pinMode(STATUS_HEAT1,INPUT_PULLUP);

  attachInterrupt(PRESSURE,isrPressureDetected,CHANGE);
  attachInterrupt(STATUS_HEAT1,isrHeat1,CHANGE);
  attachInterrupt(STATUS_PUMPLOW,isrPumpLow,CHANGE);
  attachInterrupt(STATUS_PUMPHIGH,isrPumpHigh,CHANGE);


  digitalWrite(HEAT1,HIGH);
  digitalWrite(HEAT2,LOW);
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
  
  // If the temperature too low, or too much time has passed since last cycle start the low speed pump


  // if pump is running, actual temp lower than target temp, turn heat on
  if(statusPressure && actualTemperature < targetTemperature){
    if(digitalRead(HEAT1) == LOW){
      digitalWrite(HEAT1,HIGH);
      digitalWrite(HEAT2,HIGH);
      Serial.println("Heat On");
    }
  }else if(digitalRead(HEAT1) == HIGH){
    digitalWrite(HEAT1,LOW);
    digitalWrite(HEAT2,LOW);
    Serial.println("Heat Off");
  }
}

void statusUpdateLoop(){
  static long unsigned int lastTempMillis = 0;
  static bool lastStatusPressure = false;
  static bool lastStatusPumpLow = false;
  static bool lastStatusPumpHigh = false;
  static bool lastStatusHeat1 = false;

  if(millis() > (lastTempMillis + statusTempInterval) ){
    updateTemperatureLoop();

    lastTempMillis = millis();
    Serial.print("Current temperature: ");
    Serial.println(actualTemperature);
    Serial.print(" Target temperature: ");
    Serial.println(targetTemperature);
    bootstrapManager.publish(String(tubStatusTopic + "/temperature").c_str(), String(actualTemperature).c_str(), false);
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

  if(lastStatusHeat1 != statusHeat1){
    Serial.print("Heat1 Status Change to: ");
    Serial.println(statusHeat1);
    bootstrapManager.publish(String(tubStatusTopic + "/heat1").c_str(), String(statusHeat1).c_str(), false);
  }

  lastStatusPressure = statusPressure;
  lastStatusPumpLow = statusPumpLow;
  lastStatusPumpHigh = statusPumpHigh;
  lastStatusHeat1 = statusHeat1;
}

void IRAM_ATTR isrPressureDetected(){
  lastStatusPressureMillis = currentMillis;
    // static unsigned long lastInterruptTime = 0;
    // unsigned long currentMillis = millis();

    // Prevent ISR during the first 2 seconds after reboot
    // if(currentMillis < 2000) return;
    
    // if(currentMillis - lastInterruptTime > 5000){
      // statusPressure = !digitalRead(PRESSURE);
    // }

  // lastInterruptTime = currentMillis;
}

// When the input pin changes, read the 
void IRAM_ATTR isrPumpLow(){
  lastStatusPumpLowMillis = currentMillis;
  // statusPumpLow = !digitalRead(STATUS_PUMPLOW);
}

void IRAM_ATTR isrPumpHigh(){
  lastStatusPumpHighMillis = currentMillis;
  // statusPumpHigh = !digitalRead(STATUS_PUMPHIGH);
}

void IRAM_ATTR isrHeat1(){
  lastStatusHeat1Millis = currentMillis;
//  statusHeat1 = !digitalRead(STATUS_HEAT1);
}

void statusDebounceLoop(){
  if(currentMillis - lastStatusPressureMillis > 500){
    statusPressure = !digitalRead(PRESSURE);
  }else if(500 - (currentMillis - lastStatusPressureMillis) > 450){
    Serial.print("deboucing pressure for: ");
    Serial.println(10000 - (currentMillis - lastStatusPressureMillis));
  }

  if(currentMillis - lastStatusHeat1Millis > 500){
    statusHeat1 = !digitalRead(STATUS_HEAT1);
  }

  if(currentMillis - lastStatusPumpLowMillis > 500){
    statusPumpLow = !digitalRead(STATUS_PUMPLOW);
  }

  if(currentMillis - lastStatusPumpHighMillis > 500){
    statusPumpHigh = !digitalRead(STATUS_PUMPHIGH);
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

    for(int i=0; i<length; i++){
      rxVal[i] = payload[i];
    }

    targetTemperature = atof(rxVal);
    Serial.print("Set targetTemperature: ");
    Serial.println(targetTemperature);
  }

}
void manageDisconnections(){

}
void manageQueueSubscription(){
    bootstrapManager.subscribe(tubCommandTopic.c_str());
    Serial.print("Subscribed to ");
    Serial.println(tubCommandTopic);
}
void manageHardwareButton(){

}