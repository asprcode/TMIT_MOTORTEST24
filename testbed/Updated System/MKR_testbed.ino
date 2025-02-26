#include <SD.h>
#include <stdarg.h>
// GPIO definitions
#define LOAD          A3
#define PRESSURE      A5
#define N_ENABLE      7
#define ACK           6
#define FILE_NAME     "trialrun.txt"

// State Machine Definition
typedef enum {
  STANDBY,
  ACQUIRE,
  TAKE_UNO,
  FINISH,
  FAILURE
} STATE;
STATE currentState = STANDBY;

// Object Instantiations
File logFile;
String response;

// Variable Definitions
bool start = false;
int pressureRead, loadCellRead;
unsigned char* pressureArray;
unsigned char* loadCellArray;
// int timer = 0, count = 0;
const int maxSize = 15000;  // Total buffer size for each array
int pressureIndex = 0, pressureLogs = 0, pressureDelay;     // Tracks data size for each array
int loadCellIndex = 0, loadCellLogs = 0, loadCellDelay;

// Prototype Function Definitions
void dataInit() {
  analogReadResolution(12);

  // Allocate memory for the arrays
  pressureArray = (unsigned char*)malloc(maxSize * sizeof(unsigned char));
  loadCellArray = (unsigned char*)malloc(maxSize * sizeof(unsigned char));

  // Check for successful memory allocation
  if (!pressureArray || !loadCellArray) {
    Serial.println("Memory allocation failed!");
    while (1);  // Halt execution if memory allocation fails
  }

  // Initialize arrays with null terminators
  pressureArray[0] = '\0';
  loadCellArray[0] = '\0';
}

void appendData(unsigned char* buffer, int* index, int* log, int value, int* delay) {
  // Estimate space needed to store the integer and a space
  int currTime = millis();
  int neededSpace = snprintf(NULL, 0, "%d,%d\n", currTime - *delay, value);
  
  // Ensure enough space to add new data
  if (*index + neededSpace >= maxSize - 1) {
    Serial.println("Buffer overflow detected!");
    return;
  }

  // Append formatted data to the buffer
  snprintf((char*)(buffer + *index), maxSize - *index, "%d,%d\n", currTime - *delay, value);
  *index += neededSpace;
  *log += 1;
  *delay = currTime;
}

void getData() {
  // Read analog values
  pressureRead = analogRead(A3);
  loadCellRead = analogRead(A5);

  // Append data to buffers
  appendData(pressureArray, &pressureIndex, &pressureLogs, pressureRead, &pressureDelay);
  appendData(loadCellArray, &loadCellIndex, &loadCellLogs, loadCellRead, &loadCellDelay);
  // count++;

  // if (millis() - timer >= 1000) {
  //   Serial.println(count);
  //   count = 0;
  //   timer = millis();
  // }

  // Print and reset every second
  if (pressureLogs >= 1800) {
    Serial.println("Pressure Data:");
    Serial.println((char*)pressureArray);
    Serial.println("Load Cell Data:");
    Serial.println((char*)loadCellArray);
    
    logData(2, pressureArray, loadCellArray);
    Serial.println(pressureIndex);
    Serial.println(loadCellIndex);
    // Reset buffers for new data
    pressureIndex = 0;
    loadCellIndex = 0;
    pressureLogs = 0;
    loadCellLogs = 0;
    pressureArray[0] = '\0';  // Reset to empty string
    loadCellArray[0] = '\0';
  }
}

int logData(int numArgs, ...) {
  logFile = SD.open(FILE_NAME, FILE_WRITE);
  
  if (logFile) {
    va_list args;
    va_start(args, numArgs);
    
    for (int i = 0; i < numArgs; i++) {
      char* data = va_arg(args, char*);
      logFile.println(data);
      // Serial.println(data);
    }
    
    va_end(args);
    logFile.close();
    return 1;
  } else {
    Serial.println("Error opening log file");
    Serial1.print("ERROR CODE: 2");
    recover();
    return 0;
  }
}

void unoData() {
  if (Serial1.available()) {
    String recv = Serial1.readStringUntil('\n');
    logData(1, recv);
  }
}

void recover() {
  Serial1.println((char*)pressureArray);
  Serial1.println((char*)loadCellArray);
}
void stateTransition () {
  switch (currentState) {
    case STANDBY:
      if (digitalRead(N_ENABLE) == LOW) {
        int confirm = logData(1, "testlog");
        if (confirm) {
          digitalWrite(ACK, LOW);
          currentState = ACQUIRE;
        } else {
          currentState = FAILURE;
          Serial1.println("ERROR CODE: 1");
        }
      }
      break;
    case ACQUIRE:
      if (digitalRead(N_ENABLE) == HIGH) {
        digitalWrite(ACK, HIGH);
        currentState = TAKE_UNO;
      }
      break;
    case TAKE_UNO:
      if (digitalRead(N_ENABLE) == LOW) {
        digitalWrite(ACK, LOW);
        currentState = FINISH;
      }
      break;
  }
}

void stateFunctions () {
  switch (currentState) {
    case STANDBY:
      break;
    case ACQUIRE:
      getData();
      break;
    case TAKE_UNO:
      unoData();
      break;
    case FINISH:
      while(1);
      break;
  }
}

void setup() {
  Serial.begin(9600);
  while(!Serial);
  delay(5000);
  Serial.println("Serial Comm. Initialised.");

  // Pin Initialization
  pinMode(LOAD, INPUT);
  pinMode(PRESSURE, INPUT);
  pinMode(N_ENABLE, INPUT);
  pinMode(ACK, OUTPUT);
  digitalWrite(ACK, HIGH);

  dataInit();

  //SD Card setup
  if (!SD.begin()) {
    Serial.println("SD card initialisation failed.");
    return;
  }
  logFile = SD.open(FILE_NAME, FILE_WRITE);
  if (!logFile) {
    Serial.println("Couldn't open log file");
    return;
  } 
  else {
    Serial.println("Logging to SD card...\n");
  }

  Serial.println("TESTBED SETUP COMPLETE.");
}

void loop() {
  stateTransition();
  stateFunctions();
}