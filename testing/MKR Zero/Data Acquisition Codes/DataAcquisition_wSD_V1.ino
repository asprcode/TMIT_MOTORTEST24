#include <SD.h>

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
int pressureCount = 0, loadCellCount = 0;

const int maxSize = 6000;  // Total buffer size for each array
int pressureIndex = 0;     // Tracks data size for each array
int loadCellIndex = 0;

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

void appendData(unsigned char* buffer, int* index, int value) {
  // Estimate space needed to store the integer and a space
  int neededSpace = snprintf(NULL, 0, "%d ", value);
  
  // Ensure enough space to add new data
  if (*index + neededSpace >= maxSize - 1) {
    Serial.println("Buffer overflow detected!");
    return;
  }

  // Append formatted data to the buffer
  snprintf((char*)(buffer + *index), maxSize - *index, "%d ", value);
  *index += neededSpace;

  if (buffer == loadCellArray) loadCellCount++;
  if (buffer == pressureArray) pressureCount++;
}

void getData() {
  // Read analog values
  pressureRead = analogRead(A3);
  loadCellRead = analogRead(A5);

  // Append data to buffers
  appendData(pressureArray, &pressureIndex, pressureRead);
  appendData(loadCellArray, &loadCellIndex, loadCellRead);

  // Print and reset every second
  if (millis() - timer >= 1000) {
    timer = millis();
    Serial.println("Pressure Data:");
    Serial.println((char*)pressureArray);
    Serial.print("Pressure Readings: ");
    Serial.println(pressureCount);
    Serial.println("Load Cell Data:");
    Serial.println((char*)loadCellArray);
    Serial.print("Load Cell Readings: ");
    Serial.println(loadCellCount);
    
    logData();

    // Reset buffers for new data
    pressureIndex = 0;
    loadCellIndex = 0;
    pressureArray[0] = '\0';  // Reset to empty string
    loadCellArray[0] = '\0';

    pressureCount = 0;
    loadCellCount = 0;
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
