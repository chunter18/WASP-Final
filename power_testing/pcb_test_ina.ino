#include <Wire.h>
#include <Adafruit_INA219.h>

//grounded both addr pins, gives us addr of 1000000 per the datasheet
uint8_t inaaddr = 0b01000000;
Adafruit_INA219 ina219(inaaddr);
float current_avg = 0; //ma
float power_avg = 0; //mw
float shuntvoltage_avg = 0;
float busvoltage_avg = 0;
float loadvoltage_avg = 0;
int32_t count = 0;
int sec_delay = 0;
int mode1 = 0;
int mode2 = 0;
float shuntvoltage = 0;
float current_mA = 0;

float getAvg(float prev_avg, int x, int n);
void oldloop(void);
void timetest(void);
void calc_vals(int gain, float MaxExpected_I);

void setup(void) 
{
  //mode select pins
  pinMode(10, INPUT);
  pinMode(11, INPUT);
  
  Serial.begin(115200);
  while (!Serial) {
      // will pause Zero, Leonardo, etc until serial console opens
      delay(1);
  }

  uint32_t currentFrequency;
  String rx_str = "";
    
  Serial.println("WASP pcb test code begin\n");
  /*
   * PYTHON SCRIPT WILL HANDLE THIS
  Serial.println("pick a delay (seconds) between printing averages (min is 1 second)");
  Serial.print("enter a numer: ");
  //read in a number 
  String rx_str = "";
  while (Serial.available() == 0)
  {
    delay(1);
  }
  rx_str = Serial.readString();
 
  
  Serial.println("");
  Serial.print("\nWill print averages every ");
  sec_delay = rx_str.toInt();
  Serial.print(sec_delay);
  Serial.println(" seconds.");
  */

  ina219.begin();

  //Serial.println("\npick measurement mode.");
  //Serial.println("1 =  (180 mA, gain = 1)");
  //Serial.println("2 =  (700 mA, gain = 4)");
  //Serial.println("3 =  (1.0 A,  gain = 8)");
  //Serial.println("4 =  (1.45 A, gain = 8)");
  


  //while (Serial.available() == 0)
  //{
  //  delay(1);
  //}
  
  //rx_str = Serial.readString();
  mode1 = digitalRead(10);
  mode2 = digitalRead(11);
  //Serial.println(mode);
  if((mode1 == 0) || (mode2 == 0))
  {
    Serial.println("\nsetting 180mA mode");
    ina219.setCalibration_16V_180mA_WASP();
  }
  else if((mode1 == 0) || (mode2 == 1))
  {
    Serial.println("\nsetting 700mA mode");
    ina219.setCalibration_16V_700mA_WASP();
  }
  else if((mode1 == 1) || (mode2 == 0))
  {
    Serial.println("\nsetting 1A mode");
    ina219.setCalibration_16V_1A_WASP();
  }
  else if((mode1 == 1) || (mode2 == 1))
  {
    Serial.println("\nsetting 1A mode");
    ina219.setCalibration_16V_1_45_A_WASP();
  }
  else
    Serial.println("\nnot possible to get here!");

  Serial.println("Measuring voltage and current with INA219 ...");
  Serial.print("#");
}

void loop(void) 
{
  //float shuntvoltage = 0;
  //float current_mA = 0;


  shuntvoltage = ina219.getShuntVoltage_mV();
  current_mA = ina219.getCurrent_mA();

  //printing as csv
  Serial.print(shuntvoltage);
  Serial.print(",");
  Serial.print(current_mA);
  Serial.println("|"); //stop char for python code
  //no delay, go as fast as possible
}

void timetest(void)
{

  /* raw printout
   * WASP pcb test code begin


setting 180mA mode
Measuring voltage and current with INA219 ...
#shuntv took1560microseconds
raw shuntv took1560microseconds
current took1996microseconds
raw current took2004microseconds
shuntv took1560microseconds
raw shuntv took1568microseconds
current took2008microseconds
raw current took1992microseconds
shuntv took1564microseconds
raw shuntv took1564microseconds
current took2004microseconds
raw current took2004microseconds

   * 
   */
  //float shuntvoltage = 0;
  //float current_mA = 0;


  //shuntvoltage = ina219.getShuntVoltage_mV();
  //current_mA = ina219.getCurrent_mA();

  unsigned long StartTime = micros();
  shuntvoltage = ina219.getShuntVoltage_mV();
  unsigned long CurrentTime = micros();
  unsigned long ElapsedTime = CurrentTime - StartTime;
  Serial.print("shuntv took");
  Serial.print(ElapsedTime);
  Serial.println("microseconds");

  StartTime = micros();
  shuntvoltage = ina219.getShuntVoltage_raw();
  CurrentTime = micros();
  ElapsedTime = CurrentTime - StartTime;
  Serial.print("raw shuntv took");
  Serial.print(ElapsedTime);
  Serial.println("microseconds");

  StartTime = micros();
  current_mA = ina219.getCurrent_mA();
  CurrentTime = micros();
  ElapsedTime = CurrentTime - StartTime;
  Serial.print("current took");
  Serial.print(ElapsedTime);
  Serial.println("microseconds");


  StartTime = micros();
  current_mA = ina219.getCurrent_raw();
  CurrentTime = micros();
  ElapsedTime = CurrentTime - StartTime;
  Serial.print("raw current took");
  Serial.print(ElapsedTime);
  Serial.println("microseconds");

  

  

  //printing as csv
  //Serial.print(shuntvoltage);
  //Serial.print(",");
  //Serial.print(current_mA);
  //Serial.println("|"); //stop char for python code

  delay(100000);
  
}

void oldloop(void)
{
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power_mW = 0;

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);

  //printing as csv
  //busvoltage,shuntvoltage,loadvoltage,current,power
  Serial.print(busvoltage);
  Serial.print(",");
  Serial.print(shuntvoltage);
  Serial.print(",");
  Serial.print(loadvoltage);
  Serial.print(",");
  Serial.print(current_mA);
  Serial.print(",");
  Serial.print(power_mW);
  Serial.println("|"); //stop char for python code

  delay(500); //sample every half second - 2 hz
}


float getAvg(float prev_avg, int x, int n) 
{ 
    return (prev_avg * n + x) / (n + 1); 
}

void calc_vals(int gain, float MaxExpected_I)
{
  //32v 1 A
  int VBUS_MAX = 16; //volts. will never se anything above 16v
  float VSHUNT_MAX = 0; //mv
  switch(gain)
  {
    case 1:
      VSHUNT_MAX = 0.04;
      break;
    case 2:
      VSHUNT_MAX = 0.08;
      break;
    case 4:
      VSHUNT_MAX = 0.16;
      break;
    case 8:
      VSHUNT_MAX = 0.32;
      break;
    default:
      VSHUNT_MAX = 0.04;
      break;
  }
  float RSHUNT = 0.22; //ours is .22, not 0.1

  //determine max possible current
  float MaxPossible_I = VSHUNT_MAX / RSHUNT;
  Serial.print("Max current:   "); Serial.print(MaxPossible_I); Serial.println(" A");

  //float MaxExpected_I = 1.0; //amps
  float MinimumLSB = MaxExpected_I/32767;
  Serial.print("Min LSB:   "); Serial.print(MinimumLSB); Serial.println(" A per bit");
  float MaximumLSB = MaxExpected_I/4096;
  Serial.print("Max LSB:   "); Serial.print(MaximumLSB); Serial.println(" A per bit");
 
  //choose round val close to between min and max but close to min
  float CurrentLSB = roundf(MinimumLSB*1000000.0f)/1000000.0f;

  //compute calibration reg
  float Cal = trunc (0.04096 / (CurrentLSB * RSHUNT));
  Serial.print("Calibration val:   "); Serial.println(Cal);

  int ina219_calValue = int(Cal);

  //calculate power lsb
  float PowerLSB = 20 * CurrentLSB;
  


}

