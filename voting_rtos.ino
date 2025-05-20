#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Wi-Fi credentials
#define WIFI_SSID "IQOOZ5"
#define WIFI_PASSWORD "24681234"

// Firebase credentials
#define API_KEY "AIzaSyDyEx9Q-qSfP6ccn5E5vK0rsatnDKMUBe4"
#define DATABASE_URL "https://my-project-4c469-default-rtdb.firebaseio.com/"
#define USER_EMAIL "kripakrishnakumar333@gmail.com"
#define USER_PASSWORD "Embedded_098*"

// Voter ID button pins
#define VOTER1_PIN 12
#define VOTER2_PIN 13
#define VOTER3_PIN 14
#define VOTER4_PIN 33

// Candidate button pins
#define CANDIDATE_AA_PIN 25
#define CANDIDATE_BB_PIN 26
#define CANDIDATE_CC_PIN 27

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

FirebaseData fbdoVerify;
FirebaseData fbdoCast;
FirebaseAuth auth;
FirebaseConfig config;

String voterID = "";
bool idEntered = false;
bool voteRecorded = false;
bool readyToVote = false;

void printToDisplay(String message);

void setup() {
  Serial.begin(115200);

  // Initialize display before WiFi
  Wire.begin(21, 22); // SDA, SCL
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED initialization failed");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("System Booting...");
  display.display();
  delay(1000);
  display.clearDisplay();

  // Button setup
  pinMode(VOTER1_PIN, INPUT_PULLUP);
  pinMode(VOTER2_PIN, INPUT_PULLUP);
  pinMode(VOTER3_PIN, INPUT_PULLUP);
  pinMode(VOTER4_PIN, INPUT_PULLUP);
  pinMode(CANDIDATE_AA_PIN, INPUT_PULLUP);
  pinMode(CANDIDATE_BB_PIN, INPUT_PULLUP);
  pinMode(CANDIDATE_CC_PIN, INPUT_PULLUP);

  // Wi-Fi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected with IP: " + WiFi.localIP().toString());

  // Time config (India Time)
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time...");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(100);
    now = time(nullptr);
  }
  Serial.println("Time synced!");

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  Serial.println("System Ready. Waiting for voter...");
  printToDisplay("System Ready. Waiting for voter...");

  // RTOS tasks
  xTaskCreatePinnedToCore(verifyVoterTask, "VerifyVoterTask", 8192, &fbdoVerify, 1, NULL, 1);
  xTaskCreatePinnedToCore(castVoteTask, "CastVoteTask", 8192, &fbdoCast, 1, NULL, 1);
}

void loop() {
  // Tasks are handled with RTOS
}

void verifyVoterTask(void *parameter) {
  FirebaseData *fbdo = (FirebaseData *)parameter;

  while (true) {
    if (!idEntered) {
      if (digitalRead(VOTER1_PIN) == LOW) voterID = "ID123";
      else if (digitalRead(VOTER2_PIN) == LOW) voterID = "ID234";
      else if (digitalRead(VOTER3_PIN) == LOW) voterID = "ID345";
      else if (digitalRead(VOTER4_PIN) == LOW) voterID = "ID456";
      else {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        continue;
      }

      idEntered = true;
      Serial.println("Voter ID: " + voterID);
      printToDisplay("Voter ID: " + voterID);
      String statusPath = "/" + voterID + "/Status";

      if (Firebase.getBool(*fbdo, statusPath)) {
        bool hasVoted = fbdo->boolData();
        if (hasVoted) {
          Serial.println("This ID has already voted.");
          printToDisplay("Already voted.");
          vTaskDelay(1000 / portTICK_PERIOD_MS);
          idEntered = false;
          printToDisplay("Waiting for voter...");
        } else {
          Serial.println("Voter verified.");
          printToDisplay("Verified. Please vote.");
          readyToVote = true;
        }
      } else {
        Serial.println("Invalid ID or error: " + fbdo->errorReason());
        printToDisplay("Invalid ID or Error.");
        idEntered = false;
      }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void castVoteTask(void *parameter) {
  FirebaseData *fbdo = (FirebaseData *)parameter;

  while (true) {
    if (idEntered && readyToVote && !voteRecorded) {
      String candidate = "";

      if (digitalRead(CANDIDATE_AA_PIN) == LOW) candidate = "AA";
      else if (digitalRead(CANDIDATE_BB_PIN) == LOW) candidate = "BB";
      else if (digitalRead(CANDIDATE_CC_PIN) == LOW) candidate = "CC";
      else {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        continue;
      }

      // Mark voted
      String statusPath = "/" + voterID + "/Status";
      if (Firebase.setBool(*fbdo, statusPath, true)) {
        // Get existing vote count
        String voteCountPath = "/voteCount/" + candidate;
        int count = 0;
        if (Firebase.getInt(*fbdo, voteCountPath)) {
          count = fbdo->intData();
        }
        Firebase.setInt(*fbdo, voteCountPath, count + 1);

        // Timestamp
        time_t now = time(nullptr);
        struct tm *timeinfo = localtime(&now);
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

        // Log
        FirebaseJson voteJson;
        voteJson.set("votedFor", candidate);
        voteJson.set("timestamp", timeStr);
        //voteJson.set("timestamp_unix", (unsigned long long)now * 1000);

        Firebase.pushJSON(*fbdo, "/votes", voteJson);

        Serial.println("Vote recorded.");
        printToDisplay("Vote recorded.");
        delay(1000);
      } else {
        Serial.println("Vote failed: " + fbdo->errorReason());
        printToDisplay("Vote failed.");
      }

      voteRecorded = true;
      delay(500);
      idEntered = false;
      readyToVote = false;
      voteRecorded = false;
      voterID = "";
      printToDisplay("Waiting for voter...");
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void printToDisplay(String message) {
  Serial.println(message);
  display.clearDisplay();
  display.setCursor(0, 0);

  int len = message.length();
  for (int i = 0; i < len; i += 20) {
    display.println(message.substring(i, i + 20));
  }

  display.display();
}
