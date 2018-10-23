// interrupt-based analog reading with Arduino's ATMega328p
// to measure pressure sensor values for synchronisation of
// two motorcycle carburators
// https://github.com/yz88/arduino-digital-carb-sync/


volatile int readFlag;      // High when a value is ready to be read
volatile int analogVal_ADC0;     // Value to store analog result from ADC0
volatile int analogVal_ADC1;     // Value to store analog result from ADC1
volatile int analogVal_ADC2;     // Value to store analog result from ADC2
volatile int analogVal_ADC3;     // Value to store analog result from ADC3
volatile int analogVal_ADC4;     // Value to store analog result from ADC4
volatile int analogVal_ADC5;     // Value to store analog result from ADC5s

//#define DEBUG // toogle debug: uncomment-> debug ON

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long TimeStamp = 0;
unsigned long previousTimeStampMin = 0;
unsigned long TimeStampTemp = 0;
unsigned long TimeStampMin = 20000;
unsigned long TimeStampDiff = 0;

unsigned long previousTimeStampGetVoltage = 0;  // will store the last time GetVoltage() was called
const long intervalGetVoltage = 60000;  // interval (60s) after the GetVoltage() is called
bool getVoltageFlag = true;

float RPM;

float PressureLeft = 1023;
float previousPressureLeft = 1023;
float PressureLeftTemp = 1023;
float PressureLeftMin = 1023;
float PressureLeftFiltered = 1023;

float PressureRight = 1023;
float previousPressureRight = 1023;
float PressureRightTemp = 1023;
float PressureRightMin = 1023;
float PressureRightFiltered = 1023;

int DifferenceLeft = 0;
int DifferenceRight = 0;

const int RANGE = 3;

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif


void setup() {
  // Start serial comunication at baud=9600 (default for Nextion display)
  Serial.begin(9600);

  // give the Nextion display some time to start
  delay(500);

  // Set new baud rate for Nextion display to 115200
  Serial.print("baud=115200");
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  // End the serial comunication of baud=9600.
  Serial.end();
  // start serial comunication at baud=115200.
  Serial.begin(115200);

  // initialise Arduino's input pins
  // pinMode() not neccessary because analog pin are INPUT only

  // settign Arduino's registers for freerunning anlaog read
  // clear ADLAR in ADMUX (0x7C) to right-adjust the result
  // ADCL will contain lower 8 bits, ADCH upper 2 (in last two bits)
  cbi(ADMUX, ADLAR);

  // Set REFS1..0 in ADMUX (0x7C) to change reference voltage to the
  // proper source (01)
  sbi(ADMUX, REFS0);
  cbi(ADMUX, REFS1);

  // Set MUX3..0 in ADMUX (0x7C) to read from ADC4 (0100)
  cbi(ADMUX, MUX3);
  sbi(ADMUX, MUX2);
  cbi(ADMUX, MUX1);
  cbi(ADMUX, MUX0);

  // Set ADEN in ADCSRA (0x7A) to enable the ADC.
  sbi(ADCSRA, ADEN);  // enable ADC

  // Set ADATE in ADCSRA (0x7A) to enable auto-triggering.
  sbi(ADCSRA, ADATE); // enable auto trigger

  // Clear ADTS2..0 in ADCSRB (0x7B) to set trigger mode to free running (000).
  // This means that as soon as an ADC has finished, the next will be
  // immediately started.
  cbi(ADCSRB, ADTS2);
  cbi(ADCSRB, ADTS1);
  cbi(ADCSRB, ADTS0);

  // Set ADPS2:0: ADC Prescaler Select Bits to 128 (111) (16000KHz/128 = 125KHz)
  // Above 200KHz 10-bit results are not reliable.
  sbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  // Set ADIE in ADCSRA (0x7A) to enable the ADC interrupt.
  // Without this, the internal interrupt will not trigger.
  sbi(ADCSRA, ADIE);  // enable ADC interrupt when measurement complete

  // Enable global interrupts
  // AVR macro included in <avr/interrupts.h>, which the Arduino IDE
  // supplies by default.
  sei();

  // Kick off the first ADC
  readFlag = 0;

  // Set ADSC: ADC Start Conversion in ADCSRA (0x7A) to start the ADC conversion
  sbi(ADCSRA, ADSC);  // start ADC measurements

  delay(100);

  // etermine difference between senor values
  // get some values and find the lowest value
  for (int i=0; i <= 1000; i++){
    PressureLeft = analogVal_ADC5;
    PressureRight = analogVal_ADC4;
    if (PressureLeft < previousPressureLeft) {
      previousPressureLeft = PressureLeft;  // store the lowest value
    }
    if (PressureRight < previousPressureRight) {
      previousPressureRight = PressureRight;  // store the lowest value
    }
  }

  // compare the lowest values of ADC4 and ADC5, then calculating the difference
  if (previousPressureLeft > previousPressureRight){
    DifferenceRight = previousPressureLeft - previousPressureRight;
  } else {
    DifferenceLeft = previousPressureRight - previousPressureLeft;
  }
}


// Interrupt service routine for the ADC completion
ISR(ADC_vect){
  // reading voltage values if flag is set
  if(getVoltageFlag) {
    cbi(ADMUX, MUX0);
    cbi(ADMUX, MUX1);
    cbi(ADMUX, MUX2);
    cbi(ADMUX, MUX3); // b0100 0000 -> A0
    analogVal_ADC0 = ADCL | (ADCH << 8);

    sbi(ADMUX, MUX0);
    cbi(ADMUX, MUX1);
    cbi(ADMUX, MUX2);
    cbi(ADMUX, MUX3); // b0100 0001 -> A1
    analogVal_ADC1 = ADCL | (ADCH << 8);

    cbi(ADMUX, MUX0);
    sbi(ADMUX, MUX1);
    cbi(ADMUX, MUX2);
    cbi(ADMUX, MUX3); // b0100 0010 -> A2
    analogVal_ADC2 = ADCL | (ADCH << 8);

    sbi(ADMUX, MUX0);
    sbi(ADMUX, MUX1);
    cbi(ADMUX, MUX2);
    cbi(ADMUX, MUX3); // b0100 0011 -> A3
    analogVal_ADC3 = ADCL | (ADCH << 8);

    getVoltageFlag = false;

    // set ADMUX back to A$
    cbi(ADMUX, MUX0);
    cbi(ADMUX, MUX1);
    sbi(ADMUX, MUX2);
    cbi(ADMUX, MUX3); // b0100 0100 -> A4
  }

  // reading pressure values
  // toggling analog input pin between 4 (A4) and 5 (A5)
  if (ADMUX == 0x44) {  // b0100 0100 -> A4
    // Must read low first
    analogVal_ADC4 = ADCL | (ADCH << 8);
    // Set MUX3..0 in ADMUX (0x7C) to read from ADC4 (0100) to read from ACD5 (0101)
    sbi(ADMUX, MUX0);
  } else if (ADMUX == 0x45){  // b0100 0101 -> A5
    // Must read low first
    analogVal_ADC5 = ADCL | (ADCH << 8);
    // Set MUX3..0 in ADMUX (0x7C) to read from ADC5 (0101) to read from ACD5 (0100)
    cbi(ADMUX, MUX0);
  }
}


int GetVoltage () {
  /*
  ========== Voltage Measurement ==========
  values A0..A3 are read in interrupt routine
  */

  // string for conversion from float to string, because nextion display can not display float number
  char buffer[15] = {0};
  // voltage divider 1M to 270K -> factor 4.703 for analog read on Arduino
  const float VOLTAGE_DIV_FAC = 4.703;
  // voltage correction values for the 4 cell LiFePo (4s) battery, determined by multimeter
  const float VOLTAGE_CORREC_A0 = 2.49;
  const float VOLTAGE_CORREC_A1 = 1.93;
  const float VOLTAGE_CORREC_A2 = 1.39;
  const float VOLTAGE_CORREC_A3 = 0.64;

  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  float Voltage_A0 = analogVal_ADC0 * (5.0 / 1023.0) * VOLTAGE_DIV_FAC - VOLTAGE_CORREC_A0;
  float Voltage_A1 = analogVal_ADC1 * (5.0 / 1023.0) * VOLTAGE_DIV_FAC - VOLTAGE_CORREC_A1;
  float Voltage_A2 = analogVal_ADC2 * (5.0 / 1023.0) * VOLTAGE_DIV_FAC - VOLTAGE_CORREC_A2;
  float Voltage_A3 = analogVal_ADC3 * (5.0 / 1023.0) * VOLTAGE_DIV_FAC - VOLTAGE_CORREC_A3;

  #ifndef DEBUG
  // convert float to string and send buffer to corresponding Nextion text label
  dtostrf(Voltage_A0, 5, 2, buffer);
  Serial.print("tvoltage.txt=\"");
  Serial.print(buffer);
  Serial.print("\"");
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  // voltage cell 1
  dtostrf(Voltage_A0 - Voltage_A1, 5, 2, buffer); // total of 5 chars, 2 of them after the decimal dot
  Serial.print("cell1.txt=\"");
  Serial.print(buffer);
  Serial.print("\"");
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  // voltage cell 2
  dtostrf(Voltage_A1 - Voltage_A2, 5, 2, buffer);
  Serial.print("cell2.txt=\"");
  Serial.print(buffer);
  Serial.print("\"");
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  // voltage cell 3
  dtostrf(Voltage_A2 - Voltage_A3, 5, 2, buffer);
  Serial.print("cell3.txt=\"");
  Serial.print(buffer);
  Serial.print("\"");
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  // voltage cell 4
  dtostrf(Voltage_A3, 5, 2, buffer);
  Serial.print("cell4.txt=\"");
  Serial.print(buffer);
  Serial.print("\"");
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);
  #endif

  #ifdef DEBUG
  Serial.print(Voltage_A0);
  Serial.print(" ");
  Serial.print(Voltage_A0 - Voltage_A1);
  Serial.print(" ");
  Serial.print(Voltage_A1 - Voltage_A2);
  Serial.print(" ");
  Serial.print(Voltage_A2 - Voltage_A3);
  Serial.print(" ");
  Serial.println(Voltage_A3);
  #endif
}

void loop(){
  // calibrate pressure sensors to the same level
  PressureLeft = analogVal_ADC5 + DifferenceLeft;
  PressureRight = analogVal_ADC4 + DifferenceRight;

  // filter out noise, the measured value The input signals vary in value by one up and down.
  if (PressureLeft > (PressureLeftFiltered + RANGE) || PressureLeft < (PressureLeftFiltered - RANGE)) {
    PressureLeftFiltered = PressureLeft;
  }
  if (PressureRight > (PressureRightFiltered + RANGE) || PressureRight < (PressureRightFiltered - RANGE)) {
    PressureRightFiltered = PressureRight;
  }

  TimeStamp = millis();

  // check for min values
  // The pressure may flutter at the top dead center of the engine.
  // To exclude this range, the measurement only starts at a value below 780.
  if (PressureLeftFiltered < previousPressureLeft && PressureLeftFiltered < 780) {
    PressureLeftTemp = PressureLeftFiltered;
    previousTimeStampMin = TimeStampMin;
    TimeStampTemp = TimeStamp;
  } else {
    PressureLeftMin = PressureLeftTemp;
    TimeStampMin = TimeStampTemp;
    TimeStampDiff = TimeStampMin - previousTimeStampMin;
    // In a four-stroke engine that uses a camshaft, each valve is opened every second
    // rotation of the crankshaft. According to this the camshaft runs with half speed
    // of the crankshaft. Because the time difference is derived from the camshaft (Intake to
    // Intake), we have to multiply by 2 to get the speed of the engine.
    // RPM = (1 / TimeStampDiff) * MSecPerSec * SecPerMin * 2
    //     = (1 / TimeStampDiff) * 1000 * 60 * 2
    //     = (1/ TimeStampDiff) * 120000
    RPM = 120000 / TimeStampDiff;
  }

  if (PressureRightFiltered < previousPressureRight && PressureRightFiltered < 780) {
    PressureRightTemp = PressureRightFiltered;
  } else {
    PressureRightMin = PressureRightTemp;
  }

  // in normal mode (not DEBUG) send results to Nextion display
  #ifndef DEBUG
  Serial.print("j0.val=");
  // Nextion display progressbar accepts values between 0 - 100
  // Measured values are in the range from 850 down to 400, therefore
  // the values have to be normalized
  Serial.print((int)((PressureLeftMin-1)/(1024-1)*100));
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  Serial.print("j1.val=");
  Serial.print((int)((PressureRightMin-1)/(1024-1)*100));
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  Serial.print("n0.val=");
  Serial.print((int)RPM);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);
  #endif

  // in DEBUG mode print results to serial
  #ifdef DEBUG
  Serial.print((int)((PressureLeftMin-1)/(1024-1)*100));
  Serial.print(" ");
  Serial.print((int)((PressureRightMin-1)/(1024-1)*100));
  Serial.print(" ");
  Serial.println((int)RPM);
  #endif

  previousPressureLeft = PressureLeftFiltered;
  previousPressureRight = PressureRightFiltered;

  // check interval and get battery voltage
  if (TimeStamp - previousTimeStampGetVoltage >= intervalGetVoltage || previousTimeStampGetVoltage == 0) {
    previousTimeStampGetVoltage = TimeStamp;
    getVoltageFlag = true;
    GetVoltage();
  }
}
