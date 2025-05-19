#include <Arduino_FreeRTOS.h>
#include <semphr.h> // For semaphore and mutex

// Global queue and synchronization objects
QueueHandle_t voteQueue;
SemaphoreHandle_t fingerprintReady;
SemaphoreHandle_t serialMutex;

// Simulated fingerprint result
int fakeFingerprintID = 1;

// Task: Simulate fingerprint scanning
void fingerprintTask(void *a) {
  while (1) {
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Simulate delay
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.println("Fingerprint scanned.");
    xSemaphoreGive(serialMutex);

    // Send fingerprint ID to queue
    xQueueSend(voteQueue, &fakeFingerprintID, portMAX_DELAY);
    
    // Signal that fingerprint is ready
    xSemaphoreGive(fingerprintReady);

    fakeFingerprintID++; // Simulate different IDs
  }
}

// Task: Process vote and prepare data for cloud
void voteTask(void *pvParameters) {
  int receivedID;
  while (1) {
    // Wait until fingerprint ready
    if (xSemaphoreTake(fingerprintReady, portMAX_DELAY) == pdTRUE) {
      if (xQueueReceive(voteQueue, &receivedID, portMAX_DELAY)) {
        xSemaphoreTake(serialMutex, portMAX_DELAY);
        Serial.print("Processing vote for ID: ");
        Serial.println(receivedID);
        xSemaphoreGive(serialMutex);
        
        // Simulate processing...
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    }
  }
}

// Task: Simulate sending to cloud
void cloudTask(void *a) {
  while (1) {
    vTaskDelay(5000 / portTICK_PERIOD_MS); // Simulate cloud interval
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.println("Sending data to cloud...");
    xSemaphoreGive(serialMutex);
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Create queue (holds up to 5 votes)
  voteQueue = xQueueCreate(5, sizeof(int));

  // Create binary semaphore
  fingerprintReady = xSemaphoreCreateBinary();

  // Create mutex
  serialMutex = xSemaphoreCreateMutex();

  // Create tasks
  xTaskCreate(fingerprintTask, "Fingerprint Task", 128, NULL, 3, NULL);
  xTaskCreate(voteTask, "Vote Task", 128, NULL, 2, NULL);
  xTaskCreate(cloudTask, "Cloud Task", 128, NULL, 2, NULL);
}

void loop() {
  // Empty - RTOS handles the tasks
}
