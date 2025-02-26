#include <max6675.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>

#define SCK 13
#define SO 12
#define CS1 11
#define CS2 10

#define RYLR_RX 7
#define RYLR_TX 5
SoftwareSerial RYLR(RYLR_RX, RYLR_TX);
 
#define D4184A 3
#define D4184B 2

#define ACK A5
#define N_ENABLE A3

AltSoftSerial ucComm;

int temp1Read, temp2Read;
// int timer = 0;
// const int maxRows = 25;
// const int maxCols = 2;
// unsigned char* tempArray[maxRows];
// int tempIndex[maxRows] = {0};
// int tempLogs[maxRows] = {0};
// int currentRow = 0;
// int tempDelay;

typedef enum State {SAFE, ARMED, LAUNCHED, DONE, FAILURE};
State currentState = SAFE;

// Create an instance of the MAX6675 class with the specified pins
MAX6675 thermocouple1(SCK, CS1, SO);
MAX6675 thermocouple2(SCK, CS2, SO);

void sendState(String currState) 
{
  const int maxChunkSize = 150;
  int startIndex = 0;
  int endIndex = 0;

  while (startIndex < currState.length()) {
    // Find the next newline character after maxChunkSize
    endIndex = currState.indexOf('\n', startIndex + maxChunkSize);
    
    // If no newline found, use the end of the string
    if (endIndex == -1) {
      endIndex = currState.length();
    }

    // Extract the chunk to send
    String chunk = currState.substring(startIndex, endIndex);
    
    // Prepare and send the transmission
    String transmit = "AT+SEND=0," + String(chunk.length()) + "," + chunk + "\r\n";
    RYLR.print(transmit);
    
    // Wait for transmission to complete
    delay(10);

    // Move to the next chunk
    startIndex = endIndex + 1;
  }
}

// void appendData(int value) 
// {
//   if (currentRow >= maxRows) 
//   {
//     Serial.println("Max data storage reached!");
//     return;
//   }
  
//   int currTime = millis();
//   int neededSpace = snprintf(NULL, 0, "%d,%d\n", currTime - tempDelay, value);

//   if (tempIndex[currentRow] + neededSpace >= maxCols - 1) 
//   {
//     Serial.println("Row buffer full, moving to next row.");
//     currentRow++;
//     if (currentRow >= maxRows)
//     {
//       Serial.println("All buffers full!");
//       return;
//     }
//     tempIndex[currentRow] = 0;
//     tempLogs[currentRow] = 0;
//   }

//   snprintf((char*)(tempArray[currentRow] + tempIndex[currentRow]), maxCols - tempIndex[currentRow], "%d,%d\n", currTime - tempDelay, value);
//   tempIndex[currentRow] += neededSpace;
//   tempLogs[currentRow] += 1;
//   tempDelay = currTime;
// }


// void dataInit() 
// {
//   for (int i = 0; i < maxRows; i++) 
//   {
//     tempArray[i] = (unsigned char*)malloc(maxCols * sizeof(unsigned char));
//     if (!tempArray[i]) 
//     {
//       Serial.println("Memory allocation failed :(");
//       while (1);
//     }
//     tempArray[i][0] = '\0';
//   }
// }

void getData() 
{
  temp1Read = thermocouple1.readCelsius();
  temp2Read = thermocouple2.readCelsius();
  Serial.print("Temperature: ");
  Serial.print(temp1Read);
  Serial.print(" , ");
  Serial.println(temp2Read);
  String mess = String(temp1Read) + "," + String(temp2Read);
  sendState(mess);
}

void checkInput(String receive) 
{
  receive.trim();
  
  if (receive == "ARM" && currentState == SAFE) 
  {
    Serial.println("CURRENT STATE: ARMED");
    currentState = ARMED;
    sendState("TESTBED STATE: ARMED");
    return;
  }
  if (receive == "DISARM" && currentState == ARMED) 
  {
    Serial.println("CURRENT STATE: SAFE");
    currentState = SAFE;
    sendState("TESTBED STATE: SAFE");
    return;
  }
  if (receive == "LAUNCH" && currentState == ARMED) 
  {
    digitalWrite(N_ENABLE, LOW);
    while(!(ucComm.available()||digitalRead(ACK)==LOW));
    if(digitalRead(ACK)==LOW)
    {
      Serial.println("CURRENT STATE: LAUNCHED");
      currentState = LAUNCHED;
      sendState("TESTBED STATE: LAUNCHED");
      return;
    }
    else
    {
      Serial.println("MKRZero didnt respond to launch signal");  
      currentState = FAILURE;
      sendState("ERR=1; TESTBED STATE: FAILURE");
    }
  }
  if (receive == "DONE" && currentState == LAUNCHED) 
  {
    digitalWrite(N_ENABLE, HIGH);
    Serial.println("'DONE' received. Halting MKR Zero.");
    int delayTime = millis();
    while(digitalRead(ACK)==LOW || millis()-delayTime <= 5000);
    if(digitalRead(ACK)==HIGH)
    {
      Serial.println("DONE");
      currentState = DONE;
      sendState("TESTBED STATE: DONE");
      // delay(500);
    }
    else
    {
      Serial.println("MKRZero did not respond to DONE");  
      currentState = DONE;
      sendState("ERR=2; TESTBED STATE: DONE");
    }
    
    return;
  }
}

// void sendCollectedData() 
// {
//   Serial.println("Transmitting collected data...");
  
//   for (int row = 0; row <= currentRow; row++)
//   {
//     for (int i = 0; i < tempIndex[row]; i += 200) 
//     {  
//       String packet = String((char*)tempArray[row]).substring(i, i + 200);
//       ucComm.println(packet);
//       delay(50);
//     }
//   }

//   Serial.println("Data transmission complete!");
//   currentRow = 0;
//   for (int i = 0; i < maxRows; i++) 
//   {
//     tempIndex[i] = 0;
//     tempLogs[i] = 0;
//   }

//   RYLR.println("Approach Testbed");
// }

void performOperations() 
{
  switch (currentState) 
  {
    case SAFE:
      break;

    case ARMED:
      break;

    case LAUNCHED:
      // Serial.println("D4184s LATCHED");
      digitalWrite(D4184A, HIGH);
      digitalWrite(D4184B, HIGH);

      getData();

      if (ucComm.available()) {
        String backup = ucComm.readString();
        sendState(backup);
      }
      delay(250);
      break;

    case DONE:
      digitalWrite(D4184A, LOW);
      digitalWrite(D4184B, LOW);
      while(1);  
      // sendCollectedData();
      break;

    case FAILURE:
      break;
  }
}

void setup() 
{
  Serial.begin(9600);
  RYLR.begin(57600);
  ucComm.begin(19200);
  SPI.begin();
  
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(N_ENABLE, OUTPUT);
  pinMode(ACK, INPUT);

  digitalWrite(N_ENABLE, HIGH);

  // dataInit();
  Serial.println("Setup Complete.");
}

void loop() 
{
  if (Serial.available()) 
  {
    String input = Serial.readStringUntil('\n');
    checkInput(input);
  }

  if (RYLR.available()) 
  {
    String input = RYLR.readStringUntil('\n');
    checkInput(input);
  }

  performOperations();
}