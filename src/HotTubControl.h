#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "BootstrapManager.h"

// D0 GPIO16 - can only be used as a read/write. can't be used as interrupt or one wire. no pullup, has pull down instead
// D1 GPIO5 - no restriction
// D2 GPIO4 - no restriction
// D3 GPIO0 - pulled low = bootloader mode
// D4 GPIO2 - boot fails if pulled low
    // D5 GPIO14 - no restriction
    // D6 GPIO12 - no restriction
    // D7 GPIO13 - no restriction
    // D8 GPIO15 - is pulled to GND internally, can't be used for INPUT_PULLUP (status) or one wire
// RX GPIO3 - HIGH at boot
// TX GPIO1 - boot fails if pulled low

// When relays are off (at the time of ESP bootup) the STATUS pins are pulled LOW
// 

// sensor inputs;
#define PRESSURE D1 // port1 // input dry contact pulled low for pressure switch used to detect water flow (pump is running)
#define STATUS_PUMPLOW D2 // port2 // pulled low by optocoupler when pump low relay energized
#define STATUS_HEAT D5 // port7 // pulled low by optocoupler when heat 2 relay energized
#define STATUS_PUMPHIGH RX // port6 // pulled low by optocoupler when pump high relay is energized
#define TEMPERATURE D6 // Dallas one wire temp sensor

// relay outputs
#define HEAT D8 // to trigger the Heater 1 & 2 relays; green/white & green
#define PUMPLOW D7 // to trigger the Pump Low Speed Relay; orange/white
#define PUMPHIGH D0 //  to trigger the Pump High Speed Relay; orange

bool setupComplete = false;
String tubCommandTopic = "hottub/command/#";
String tubStatusTopic = "hottub/status";
String tubTargetTopic = "hottub/target";

float actualTemperature = 101.1; // set init value to 101, to be overwritten when actual temp is taken
float targetTemperature = 60.2; // default to 60 degrees as desired set temperature
const float tempCutOffset = .75; // this value is subtracted and added to the targetTemp to create a cut in and cut off
const int tempUpperLimit = 105; // high low limits that the target temp must be between
const int tempLowerLimit = 54; // 
const int statusInterval = 5000; // number of millis to wait before sending full status update on mqtt

unsigned long int currentMillis = 0;
unsigned long int lastPressureMillis = 0; // the last time pressure was detected / pump was run
unsigned long int lastPumpStartMillis = 0; // the last time the pump was started
unsigned long int lastHeatOffMillis = 0; // the last time the heat was turned off, used for pump post run
unsigned long int pumpInterval = 3600000; // millis to wait before between pump runs
unsigned long int pumpMinRunTime = 180000; // minimum run time for the pump
unsigned long int pumpPrePostRunDuration = 60000; // number of millis the pump has to run before and AFTER we turn the heat on
bool newTargetTemp = false; // when a new target temp is set, this tells the pump to start

unsigned long int lastStatusPressureMillis = 0;
unsigned long int lastStatusPumpLowMillis = 0;
unsigned long int lastStatusPumpHighMillis = 0;
unsigned long int lastStatusHeatMillis = 0;



bool statusPressure = 0; // is pressure detected or not. if true, pump is running. if false, pump is not running
bool statusPumpLow = 0;
bool statusPumpHigh = 0;
bool statusHeat = 0;

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
void IRAM_ATTR isrHeat();
void statusDebounceLoop();


// DS18B20 Temperature Sensor
OneWire oneWire(TEMPERATURE);
DallasTemperature sensors(&oneWire);
void updateTemperatureLoop();
void updateLED();
// bool isTempBelowTarget();