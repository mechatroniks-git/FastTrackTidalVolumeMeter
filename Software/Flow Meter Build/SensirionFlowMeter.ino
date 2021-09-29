/*
tetrabio.org
SensirionFlowMeter
built using Olimex ESP32-POE-ISO https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware
Sensirion SFM3300 https://www.sensirion.com/en/flow-sensors/mass-flow-meters-for-high-precise-measurement-of-gases/proximal-flow-sensors-sfm3300-autoclavable-washable-or-single-use/ 
SSD1306 OLED Generic
*/

#include <Wire.h> // Arduino Built-in
#include <Arduino.h> // Arduino AVR core
#include <HardwareSerial.h> // Arduino AVR core
#include <Time.h> // PaulStoffregen - Time
#include <SSD1306Wire.h> // https://github.com/ThingPulse/esp8266-oled-ssd1306
#include "esp32config.h"  // This project
HardwareSerial hwSerial(1); //for debugging, uses USB serial for data output

SSD1306Wire display(0x3c, esp32I2CSDA, esp32I2CSCL);  // i2c # 1

extern TwoWire Wire1; // i2c #2

const int skipCount = 20; //update rate in mS
const int averageCount = 50; //sample averaging time in ms
double maxSFlowRate = 0;
String sFlowRate;
  
void setupOled(){
  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Tetra Bio Flow Meter");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 45, "22May2021");
  display.display();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void setupSerial() {
  Serial.begin(115200);
  hwSerial.begin(115200, SERIAL_8N1, esp32Uart2Rx, esp32Uart2Tx);
}

void setupFlowSensor() {
  pinMode(SDA_2,INPUT_PULLUP); // SDA
  pinMode(SCL_2,INPUT_PULLUP); // SCL
  Wire1.begin(SDA_2, SCL_2, 100000); //begin(int sdaPin, int sclPin, uint32_t frequency)
  delay(500);
  Wire1.beginTransmission(byte(0x40)); // transmit to device #064 (0x40)
  Wire1.write(byte(0x10));      //
  Wire1.write(byte(0x00));      //
  Wire1.endTransmission(); 
  delay(5);
  Wire1.requestFrom(0x40, 3); //
  int a = Wire1.read(); // first received byte stored here
  int b = Wire1.read(); // second received byte stored here
  int c = Wire1.read(); // third received byte stored here
  Wire1.endTransmission();
  hwSerial.print(a);
  hwSerial.print(b);
  hwSerial.println(c);
  delay(5);
  Wire1.requestFrom(0x40, 3); //
  a = Wire1.read(); // first received byte stored here
  b = Wire1.read(); // second received byte stored here
  c = Wire1.read(); // third received byte stored here
  Wire1.endTransmission();
  hwSerial.print(a);
  hwSerial.print(b);
  hwSerial.println(c);
  delay(5);
}

//CRC
#define POLYNOMIAL 0x131 //P(x)=x^8+x^5+x^4+1 = 100110001

uint8_t crc8(uint8_t data[], uint8_t nbrOfBytes)  {
  uint8_t crc = 0;
  uint8_t byteCtr;
  //calculates 8-Bit checksum with given polynomial
  for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr) { 
    crc ^= (data[byteCtr]);
    for (uint8_t bit = 8; bit > 0; --bit) { 
      if (crc & 0x80) {
        crc = (crc << 1) ^ POLYNOMIAL;
      }
      else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
} 

double getFlow() {
  uint8_t iicData[3];
  uint8_t mycrc;
  uint16_t measuredValue;
  uint16_t offsetFlow = 32768; // Offset for the sensor
  double Flow;
  double scaleFactorFlow = 120.0; // Scale factor for Air and N2 is 120.0
  double calibrationOffset = 0.0;    // device 1
  //double calibrationOffset = 0.07; // device 0
  Wire1.requestFrom(0x40, 3); // read 3 bytes from device with address 0x40
  iicData[0] = Wire1.read(); // first received byte stored here. The variable "uint16_t" can hold 2 bytes, this will be relevant later
  iicData[1] = Wire1.read(); // second received byte stored here
  measuredValue = (iicData[0] * 0x100) + iicData[1];
  iicData[2] = Wire1.read(); // crc value stored here
  if ( (iicData[0] == 0xff) && (iicData[1] == 0xff) && (iicData[2] = 0xff) ) {
    // invalid data
    return(0);
  }
  else {
    mycrc = crc8(iicData, 2);  // calc CRC
    if (mycrc != iicData[2]) { // check if the calculated and the received CRC byte matches
      hwSerial.print("CRC error | ");
      hwSerial.print(iicData[0], HEX);
      hwSerial.print(" | ");
      hwSerial.print(iicData[1], HEX);
      hwSerial.print(" | ");
      hwSerial.print(iicData[2], HEX);
      hwSerial.print(" > ");
      hwSerial.println(mycrc, HEX);    
      Flow = 0.00;
    }
    else {
      //Flow = -(((double)(measuredValue - offsetFlow) / scaleFactorFlow) + calibrationOffset); // flow in liters/min
      Flow = (((double)(measuredValue - offsetFlow) / scaleFactorFlow) + calibrationOffset); // flow in liters/min
    }
    return(Flow);
  }
}

void updateDisplay() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "f= " + sFlowRate);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 25, "Max = " + String(maxSFlowRate) +" LPM");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,45,"v-22May2021");
  display.display();
}

void updatePlot(double flow) {
  Serial.print("SLM:");
  Serial.println(flow); // print the calculated flow to the serial interface
}

void setup() {
  setupSerial();
  Serial.println("Serial Setup");
  hwSerial.println("Serial Setup");
  setupOled();
  Serial.println("OLED Setup");
  hwSerial.println("OLED Setup");
  setupFlowSensor();  
  Serial.println("FlowSensor Setup");
  hwSerial.println("FlowSensor Setup");
}

void loop() {
  int skip = 0; 
  double flow = 0.0;
  double flowRunningSum = 0.0;
  double flowAverage = 0.0;
  Serial.println("Starting Main");
  while (true) {
    flow = getFlow();
    flowRunningSum = flowRunningSum + flow - flowAverage;
    flowAverage = flowRunningSum / averageCount;
    maxSFlowRate = max(maxSFlowRate,flowAverage);
    if (skip++ > skipCount) {
      sFlowRate = String(flowAverage,0.1) + " " + String(flow,0);
      updateDisplay();
      updatePlot(flowAverage);
      skip = 0;
    }
    delay(1);
  }
}
