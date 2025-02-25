#include <SD.h>

int rawValue;
float pressure;
File logFile;

// 
void setup() 
{
      Serial.begin(9600);
      // analogReadResolution(10);
      // if(!SD.begin())
      //   Serial.println("SD card not initialised.");
      // else
        // Serial.println("SD card initilaised.");

      // File logfile;
}

void loop() 
{
  //0.5 to 4.5 - 0.1 to 100
      analogReadResolution(12);
      rawValue = analogRead(A3);
      float volt = rawValue/4095.0 * 3.3;
      pressure = (250/4.5)*100000.0*(((rawValue)/4095.0)*3.3-0.5);
      Serial.println(pressure);
      Serial.println(rawValue);
      Serial.println(volt);
      Serial.println();
      // logFile = SD.open("dataLogs.txt", FILE_WRITE);
      // if (logFile) 
      // {
      //   logFile.println(rawValue);
      //   // Serial.println("File initialized. ");
      // }
      // else 
      // {
      //   Serial.println("Didnt open file.");
      // }
      // Serial.println("\n");
      // delay(1000);
}
