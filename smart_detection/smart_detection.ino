#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// MANDATORY: Enable modules for Firebase
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h> 

// --- 1. Configuration ---
#define WIFI_SSID     "UW MPSK"
#define WIFI_PASSWORD "jVAn7m45bYAtKAd6"
#define API_KEY       "AIzaSyDR5M44M45YkQcwNUUTyx4M8K7VTsNdvA0"
#define DATABASE_URL  "https://jwesp32-project-default-rtdb.firebaseio.com/"
#define USER_EMAIL    "murphywei2000@gmail.com"
#define USER_PASSWORD "Jingjing1121"

const int TRIG_PIN = 5; 
const int ECHO_PIN = 6;
const int THRESHOLD = 50; // Detection threshold in cm

// --- 2. Global Objects ---
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;
AsyncResult dbResult;

float getDistance() {
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    return duration * 0.034 / 2;
}

void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // --- WORKING STAGE START ---
    Serial.println("\n[Working Stage] Waking up for sensor check...");
    float distance = getDistance();
    
    // Only activate WiFi (high power) if an object is detected
    if (distance > 0 && distance < THRESHOLD) {
        Serial.println("Object Detected! Initializing WiFi + Firebase...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        while (WiFi.status() != WL_CONNECTED) { delay(500); }
        
        ssl_client.setInsecure();
        initializeApp(aClient, app, getAuth(user_auth));
        app.getApp<RealtimeDatabase>(Database);
        Database.url(DATABASE_URL);
        
        Database.set<float>(aClient, "/lab5/security_alert", distance, dbResult);
        delay(2000); // Hold the high-power state briefly for the screenshot
    }

    // --- DEEP SLEEP STAGE START ---
    Serial.println("[Deep Sleep] Entering low-power mode for 60s...");
    Serial.flush();
    // 将休眠时间改为 5 秒，这样你就能在 1 分钟内看到很多次“唤醒-休眠”的循环
    esp_sleep_enable_timer_wakeup(5 * 1000000ULL);
    esp_deep_sleep_start(); 
}

void loop() {} // Not used in Deep Sleep flow