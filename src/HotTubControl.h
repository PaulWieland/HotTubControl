#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "BootstrapManager.h"

// sensor inputs; D0 can only be used as a read/write. can't be used as interrupt or one wire
#define STATUS_HEAT1 D0 // pulled low by optocoupler when heat 1 relay energized
#define STATUS_PUMPLOW D1 // pulled low by optocoupler when pump low relay energized
#define STATUS_PUMPHIGH D0 // pulled low by optocoupler when pump high relay energized
#define TEMPERATURE D3 // Dallas one wire temp sensor
#define PRESSURE D4 // input dry contact for pressure switch used to detect water flow (pump is running)
// #define STATUS_HEAT2 // not enough GPIOs pulled low by optocoupler when heat 2 relay energized

// relay outputs
#define HEAT1 D5 // output A to trigger the Heater 1 relay
#define HEAT2 D6 // output B to trigger the Heater 2 relay
#define PUMPLOW D7 // output C to trigger the Pump Low Speed Relay
#define PUMPHIGH D8 // output D to trigger the Pump High Speed Relay

bool setupComplete = false;
String tubCommandTopic = "hottub/command/#";
String tubStatusTopic = "hottub/status";

float actualTemperature = 101.1; // set init value to 101, to be overwritten when actual temp is taken
float targetTemperature = 101.1; // default to 101 degrees as desired set temperature
const float tempCutIn = 1.1; // number of degrees to start / stop heating from
const int statusTempInterval = 5000; // number of millis to wait before sending temp update on mqtt

unsigned long int lastHeatMillis = 0;
bool statusPressure = 0; // is pressure detected or not. if true, pump is running. if false, pump is not running
bool statusPumpLow = 0;
bool statusPumpHigh = 0;
bool statusHeat1 = 0;

void statusUpdateLoop(); // run in main loop to send mqtt messages when the value of STATUS PUMP/HEAT changes

/****************** BOOTSTRAP MANAGER ******************/
BootstrapManager bootstrapManager;

/********************************** BOOTSTRAP FUNCTION DECLARATION *****************************************/
void callback(char *topic, byte *payload, unsigned int length);
void manageDisconnections();
void manageQueueSubscription();
void manageHardwareButton();


/********************************** INTERRUPT SERVICE ROUTINES ***********************************/
void IRAM_ATTR isrPressureDetected();
void IRAM_ATTR isrPumpLow();
void IRAM_ATTR isrPumpHigh();
void IRAM_ATTR isrHeat1();


// DS18B20 Temperature Sensor
OneWire oneWire(TEMPERATURE);
DallasTemperature sensors(&oneWire);
void updateTemperatureLoop();