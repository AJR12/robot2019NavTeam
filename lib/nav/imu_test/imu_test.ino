#include "Arduino.h"
#include "imu.h"
Imu imu;

void setup(void)
{
  Serial.begin(9600);
  imu.start();
}

void loop(void)
{
float phi = imu.getPhi();
Serial.print("phi = ");
Serial.println(phi);
}


