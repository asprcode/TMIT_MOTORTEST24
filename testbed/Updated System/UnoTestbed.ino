#include <SPI.h>
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>

#define SCK 13
#define MISO 12
#define MOSI 11
#define CS1 10
#define CS2 4

#define RYLR_RX 6
#define RYLR_TX 7
SoftwareSerial RYLR(RYLR_RX, RYLR_TX);
String data, transmit, response;
 
#define D4184A 3
#define D4184B 2

#define ACK A5
#define N_ENABLE A3

AltSoftSerial ucComm;

int temp1Read, temp2Read;
int timer = 0;
const int maxRows = 10;
const int maxCols = 75;
unsigned char* tempArray[maxRows];
int tempIndex[maxRows] = {0};
int tempLogs[maxRows] = {0};
int currentRow = 0;
int tempDelay;

typedef enum State {SAFE, ARMED, LAUNCHED, DONE, FAILSAFE};
State currentState = SAFE;

void sendState(String currState) 
{
  
  String transmit = "AT+SEND=0," + String(currState.length()) + "," + currState + "\r\n";
  RYLR.print(transmit);
  delay(10);
}

void receiveData() {
  if (RYLR.available()) {
    Serial.println("rylr available");
    response = RYLR.readStringUntil('\n');
    response = parseRYLR(response);
    Serial.println("DATA RECEIVED: " + response);
  }
}

String parseRYLR(String input) 
{
  int start = input.indexOf(',') + 1;
  start = input.indexOf(',', start) + 1;
  int end = input.indexOf(',', start);
  String parsed = input.substring(start, end);
  parsed.trim();
  return parsed;  
}

int SPIData(int CS) 
{
  digitalWrite(CS, LOW);
  delay(10);
  int received = SPI.transfer(0);
  digitalWrite(CS, HIGH);
  return received;
}

void appendData(int value) 
{
  if (currentRow >= maxRows) 
  {
    Serial.println("Max data storage reached!");
    return;
  }
  
  int currTime = millis();
  int neededSpace = snprintf(NULL, 0, "%d,%d\n", currTime - tempDelay, value);

  if (tempIndex[currentRow] + neededSpace >= maxCols - 1) 
  {
    Serial.println("Row buffer full, moving to next row.");
    currentRow++;
    if (currentRow >= maxRows)
    {
      Serial.println("All buffers full!");
      return;
    }
    tempIndex[currentRow] = 0;
    tempLogs[currentRow] = 0;
  }

  snprintf((char*)(tempArray[currentRow] + tempIndex[currentRow]), maxCols - tempIndex[currentRow], "%d,%d\n", currTime - tempDelay, value);
  tempIndex[currentRow] += neededSpace;
  tempLogs[currentRow] += 1;
  tempDelay = currTime;
}


void dataInit() 
{
  for (int i = 0; i < maxRows; i++) 
  {
    tempArray[i] = (unsigned char*)malloc(maxCols * sizeof(unsigned char));
    if (!tempArray[i]) 
    {
      Serial.println("Memory allocation failed :(");
      while (1);
    }
    tempArray[i][0] = '\0';
  }
}

void getData() 
{
  temp1Read = SPIData(CS1);
  temp2Read = SPIData(CS2);
  // temp1Read = 37.3; // placeholder
  appendData(temp1Read);
  appendData(temp2Read);
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
    // digitalWrite(N_ENABLE, LOW);
    // while(!(ucComm.available()||digitalRead(ACK)==LOW));
    // if(digitalRead(ACK)==LOW)
    // {
    //   Serial.println("CURRENT STATE: LAUNCHED");
      currentState = LAUNCHED;
      sendState("TESTBED STATE: LAUNCHED");
      Serial.println("CURRENT STATE: LAUNCHED");
      //send data to mkr to store on SD
      return;
    // }
    // else if (ucComm.available())
    // {
    //   Serial.println("MKRZero Data Logging Failed");  
    //   // currentState = FAILSAFE;
    //   sendState("Going to FAILSAFE");
    //   String recv = ucComm.readString();
    //   Serial.println(recv);
    // }
  }
  if (receive == "DONE" && currentState == LAUNCHED) 
  {
    // digitalWrite(N_ENABLE, HIGH);
    
    Serial.println("'DONE' received. Sending data request to MKR Zero...");
    sendState("TESTBED STATE: DONE");
    Serial.println("Sent testbed state done to groundside");

    // if(digitalRead(ACK)==HIGH)
    // {
    //   Serial.println("DONE");
    //   currentState = DONE;
    // }
    // else
    // {
    //   Serial.println("MKRZero didnt respond to DONE");  
    //   currentState = DONE;
    //   sendState("FAILEND");
    // }
    
    return;
  }
}

void sendCollectedData() 
{
  Serial.println("Transmitting collected data...");
  
  for (int row = 0; row <= currentRow; row++)
  {
    for (int i = 0; i < tempIndex[row]; i += 200) 
    {  
      String packet = String((char*)tempArray[row]).substring(i, i + 200);
      ucComm.println(packet);
      delay(50);
    }
  }

  Serial.println("Data transmission complete!");
  currentRow = 0;
  for (int i = 0; i < maxRows; i++) 
  {
    tempIndex[i] = 0;
    tempLogs[i] = 0;
  }

  RYLR.println("Approach Testbed");
}

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
      delay(250);
      break;

    case DONE:
      delay(500);  
      sendCollectedData();
      break;

    case FAILSAFE:
      if (ucComm.available()) {
        String recv = ucComm.readString();
        RYLR.println(recv);
        Serial.println(recv);
      }
      break;
  }
}

void setup() 
{
  Serial.println("hellos");
  Serial.begin(9600);
  RYLR.begin(57600);
  ucComm.begin(19200);
  SPI.begin();
  
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(N_ENABLE, OUTPUT);
  pinMode(ACK, INPUT);

  digitalWrite(CS1, HIGH);
  digitalWrite(CS2, HIGH);

  digitalWrite(N_ENABLE, HIGH);

  dataInit();
  Serial.println("Setup Complete.");
}

void loop() 
{
  if (RYLR.available()) 
  {
    String input = RYLR.readStringUntil('\n');
    input = parseRYLR(input);
    checkInput(input);
  }

  // receiveData();
  // performOperations();
}
