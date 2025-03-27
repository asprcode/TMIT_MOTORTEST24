#include <Adafruit_ZeroDMA.h>
#include <SdFat.h>
#include <SPI.h>

#define ADC_PIN_1 A4
#define ADC_PIN_2 A5
#define SAMPLE_BLOCK_LENGTH 256
#define LOG_INTERVAL 1000  // Log data every 1 second
#define FILENAME "dataclam.txt"

Adafruit_ZeroDMA ADC_DMA;
DmacDescriptor *dmac_descriptor_1;
DmacDescriptor *dmac_descriptor_2;

uint16_t adc_buffer[SAMPLE_BLOCK_LENGTH * 2];
uint16_t adc_sample_block_1[SAMPLE_BLOCK_LENGTH];
uint16_t adc_sample_block_2[SAMPLE_BLOCK_LENGTH];

volatile bool filling_first_half = true;
volatile uint16_t *active_adc_buffer;
volatile bool adc_buffer_filled = false;
volatile bool reading_A4 = true;

// SD card variables - using built-in SD on MKR Zero
SdFat sd;
SdFile dataFile;
unsigned long lastLogTime = 0;

typedef enum STATE {
  STANDBY,
  ACQUIRE,
  FINISH,
  FAILURE
};
STATE currentState = STANDBY;

// ---------- State Transition ----------
void stateTransition() {
  switch (currentState) {
    case STANDBY:
      if (digitalRead(N_ENABLE) == LOW) {
        if (dataFile.isOpen()) {
          digitalWrite(ACK, LOW);
          currentState = ACQUIRE;
        } else {
          Serial.println("Failed to open log file.");
          currentState = FAILURE;
        }
      }
      break;

    case ACQUIRE:
      if (digitalRead(N_ENABLE) == HIGH) {
        digitalWrite(ACK, HIGH);
        currentState = FINISH;
      }
      break;
  }
}

// void stateTransition() {
//   if (Serial.available()) {
//     String in = Serial.readStringUntil('\n');
//     if (in == "ACQUIRE")  currentState = ACQUIRE;
//     if (in == "DONE")     currentState = FINISH;
//   }
// }

void stateFunctions() {
  switch (currentState) {
    case ACQUIRE:
      static unsigned long startTime = 0;
      static unsigned long endTime = 0;
      static bool logThisBlock = false;
      
      if (adc_buffer_filled) {
          endTime = millis(); // Record end time
          logThisBlock = (endTime - lastLogTime >= LOG_INTERVAL);
          
          // Write data to SD card if it's time to log
          if (logThisBlock && dataFile.isOpen()) {
              if (filling_first_half) {
                  // Just filled second half, so log adc_sample_block_2
                  log_data(adc_sample_block_2, false);
              } else {
                  // Just filled first half, so log adc_sample_block_1
                  log_data(adc_sample_block_1, true);
              }
              lastLogTime = endTime;
          }
          
          adc_buffer_filled = false;

          unsigned long elapsedTime = endTime - startTime; // Calculate elapsed time
          if (elapsedTime > 0) {
              float dataRate = (2.0 * SAMPLE_BLOCK_LENGTH * 1000.0) / elapsedTime; // Calculate data rate
              Serial.print("Data Rate: ");
              Serial.print(dataRate);
              Serial.println(" data points/second");
          }
          startTime = millis(); // Record new start time
      }
      break;
    case FINISH:
      while(1);
      break;
  }
}
static void ADCsync() {
    while (ADC->STATUS.bit.SYNCBUSY == 1);
}

void dma_callback(Adafruit_ZeroDMA *dma) {
    (void)dma;
    if (filling_first_half) {
        // Copy data to first sample block for processing/logging
        memcpy(adc_sample_block_1, (const void*)&adc_buffer[0], SAMPLE_BLOCK_LENGTH * sizeof(uint16_t));
        
        active_adc_buffer = &adc_buffer[SAMPLE_BLOCK_LENGTH];
        filling_first_half = false;
    } else {
        // Copy data to second sample block for processing/logging
        memcpy(adc_sample_block_2, (const void*)&adc_buffer[SAMPLE_BLOCK_LENGTH], SAMPLE_BLOCK_LENGTH * sizeof(uint16_t));
        
        active_adc_buffer = &adc_buffer[0];
        filling_first_half = true;
    }
    adc_buffer_filled = true;

    if (reading_A4) {
        ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[ADC_PIN_2].ulADCChannelNumber;
        reading_A4 = false;
    } else {
        ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[ADC_PIN_1].ulADCChannelNumber;
        reading_A4 = true;
    }
    ADCsync();
}

void adc_init() {
    ADC->CTRLA.bit.ENABLE = 0;
    ADCsync();
    ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC1;
    ADCsync();
    ADC->AVGCTRL.reg = 0;
    ADC->SAMPCTRL.reg = 2;
    ADCsync();
    ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV32 | ADC_CTRLB_FREERUN | ADC_CTRLB_RESSEL_12BIT;
    ADCsync();
    ADC->CTRLA.bit.ENABLE = 1;
    ADCsync();
    ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[ADC_PIN_1].ulADCChannelNumber; //Start with A4
    ADCsync();
}

void dma_init() {
    ADC_DMA.allocate();
    ADC_DMA.setTrigger(ADC_DMAC_ID_RESRDY);
    ADC_DMA.setAction(DMA_TRIGGER_ACTON_BEAT);
    dmac_descriptor_1 = ADC_DMA.addDescriptor(
        (void *)(&ADC->RESULT.reg),
        adc_buffer,
        SAMPLE_BLOCK_LENGTH,
        DMA_BEAT_SIZE_HWORD,
        false,
        true);
    dmac_descriptor_1->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

    dmac_descriptor_2 = ADC_DMA.addDescriptor(
        (void *)(&ADC->RESULT.reg),
        adc_buffer + SAMPLE_BLOCK_LENGTH,
        SAMPLE_BLOCK_LENGTH,
        DMA_BEAT_SIZE_HWORD,
        false,
        true);
    dmac_descriptor_2->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

    ADC_DMA.loop(true);
    ADC_DMA.setCallback(dma_callback);
}

// Initialize SD card for MKR Zero
bool sd_init() {
    // MKR Zero has built-in SD card support with SDCARD_SS_PIN
    Serial.println("Initializing SD card...");
    
    // Try multiple initialization methods for compatibility
    if (!sd.begin(SDCARD_SS_PIN, SD_SCK_MHZ(4))) {
        // Try the older initialization if the first method fails
        if (!sd.begin(SDCARD_SS_PIN)) {
            Serial.println("SD card initialization failed!");
            return false;
        }
    }
    
    Serial.println("SD card initialized successfully.");
    
    // Open the fixed filename "datadogs.txt"
    if (sd.exists(FILENAME)) {
        // If file exists, open it for appending
        if (!dataFile.open(FILENAME, O_WRITE | O_APPEND)) {
            Serial.println("Could not open existing file for appending!");
            return false;
        }
        Serial.println("Appending to existing file: " FILENAME);
    } else {
        // Create new file
        if (!dataFile.open(FILENAME, O_WRITE | O_CREAT)) {
            Serial.println("Could not create file!");
            return false;
        }
        // Write header row for new file
        dataFile.println("Time,A4,A5");
        Serial.println("Created new file: " FILENAME);
    }
    
    dataFile.sync();
    return true;
}

// Close the data file safely
void close_file() {
    if (dataFile.isOpen()) {
        dataFile.close();
        Serial.println("File closed successfully");
    }
}

// Log data to SD card
void log_data(uint16_t* data_block, bool is_first_half) {
    if (!dataFile.isOpen()) {
        Serial.println("Error: File not open for writing!");
        return;
    }
    
    unsigned long timestamp = millis();
    
    for (int i = 0; i < SAMPLE_BLOCK_LENGTH; i++) {
        dataFile.print(timestamp + i);
        dataFile.print(",");
        dataFile.print(data_block[i]);
        dataFile.print(",");
        
        // We don't have simultaneous samples of both pins, so we'll just repeat the value
        // In a real application, you might want to handle this differently
        dataFile.println(data_block[i]);
    }
    
    // Sync after writing a block to ensure data is written to card
    dataFile.sync();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000); // Wait for Serial port to connect or timeout after 5s
    
    Serial.println("DMA ADC with SD card logging for MKR Zero");
    delay(1000); // Give SD card time to initialize properly
    
    // Initialize SD card
    if (!sd_init()) {
        Serial.println("SD initialization failed. Continuing without logging.");
    }
    
    adc_init();
    dma_init();
    ADC_DMA.startJob();
    
    Serial.println("Sampling started!");
}

void loop() {
  stateTransition();
  stateFunctions();
}