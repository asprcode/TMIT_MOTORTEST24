#include <SD.h>

/*
    this version prints the delay between readings for both arrays as well
*/

// GPIO definitions
#define LOAD          A3
#define PRESSURE      A5

// Object Instantiations
File logFile;
String response;

// Variable Definitions
int pressureRead, loadCellRead;
int timer = 0;
unsigned char* pressureArray;
unsigned char* loadCellArray;

const int maxSize = 6000;  // Total buffer size for each array
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
  int neededSpace = snprintf(NULL, 0, "%d &d ", currTime - *delay, value);
  
  // Ensure enough space to add new data
  if (*index + neededSpace >= maxSize - 1) {
    Serial.println("Buffer overflow detected!");
    return;
  }

  // Append formatted data to the buffer
  snprintf((char*)(buffer + *index), maxSize - *index, " %d %d", currTime - *delay, value);
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

  // Print and reset every second
  if (pressureLogs >= 500) {
    Serial.println("Pressure Data:");
    Serial.println((char*)pressureArray);
    Serial.println("Load Cell Data:");
    Serial.println((char*)loadCellArray);
    
    // logData();

    // Reset buffers for new data
    pressureIndex = 0;
    loadCellIndex = 0;
    pressureLogs = 0;
    loadCellLogs = 0;
    pressureArray[0] = '\0';  // Reset to empty string
    loadCellArray[0] = '\0';
  }
}

void logData() {
  logFile = SD.open("thisdata.txt", FILE_WRITE);
  logFile.println((char*)pressureArray);
  logFile.println((char*)loadCellArray);
  logFile.close();
}

void setup() {
  Serial.begin(9600);
  Serial.println("Serial Comm. Initialised.");

  // Pin Initialization
  pinMode(LOAD, INPUT);
  pinMode(PRESSURE, INPUT);

  dataInit();

  //SD Card setup
  if (!SD.begin()) {
    Serial.println("SD card initialisation failed.");
    return;
  }
  logFile = SD.open("thisdata.txt", FILE_WRITE);
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
  getData();
}