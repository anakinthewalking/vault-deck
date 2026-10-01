#include <Arduino.h>
#include <TFT_eSPI.h>          
#include <DHT.h>             
#include <WiFi.h>              
#include <HTTPClient.h>        
#include <ArduinoJson.h>     
#include <WiFiUdp.h>           
#include <WiFiClientSecure.h>  
#include <SpotifyArduino.h>    

#include "secrets.h"    

const char* pc_ip = PC_IP;
const int udpPort = UDP_PORT;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const String apiKey = OPENWEATHER_API_KEY;
String apiUrl = String(OPENWEATHER_URL_BASE) + apiKey;

char clientId[] = SPOTIFY_CLIENT_ID;
char clientSecret[] = SPOTIFY_CLIENT_SECRET;
char refreshToken[] = SPOTIFY_REFRESH_TOKEN;

WiFiUDP udp;
unsigned long lastTelemetryTime = 0;

WiFiClientSecure clientSecure;  
SpotifyArduino spotify(clientSecure, clientId, clientSecret, refreshToken);  

String currentTrack = "Waiting API...";
String currentArtist = "For Spotify";
long trackProgress = 0;
long trackDuration = 1000;
bool isPlaying = false;
unsigned long lastApiSyncTime = 0; 
unsigned long lastSpotifyCheck = 0;
const unsigned long spotifyDelay = 5000; 

#define BTN_UP    13
#define BTN_DOWN  33
#define BTN_LEFT  14
#define BTN_RIGHT 27
#define BTN_OK    26
#define MIC_PIN   35
#define POT_PIN   32
#define LED_PIN   25
#define DHTPIN    21
#define LDR_PIN   34
#define DHTTYPE   DHT22

TFT_eSPI tft = TFT_eSPI();
DHT dht(DHTPIN, DHTTYPE);

bool isOfflineMode = false; 
const int NUM_TABS = 7;
int currentTab = 0;
String tabNames[NUM_TABS] = {"STAT", "ENV ", "WAVE", "RDO ", "DIAG", "LGHT", "GAME"};

bool lastLeftState = HIGH;
bool lastRightState = HIGH;
bool lastOkState = HIGH; 
static bool lastUpState = HIGH;
static bool lastDownState = HIGH;


int waveX = 10;       
int lastWaveY = 130;  
int lastPercent = -1;
int gaugeCenterX = 160; 
int gaugeCenterY = 210; 
float smoothedPot = 0.0; 

float outTemp = 0.0;
int outHumidity = 0;
bool dataFetched = false;
unsigned long lastApiTime = 0;

// --- [GAME] ORBIT DEFENSE VARIABLES v2 ---
int shipLane = 1; 
int laneY[3] = {90, 140, 190};
float obsX = 320;
float lastObsX = 320; 
int obsLane = 1;
int gameScore = 0;
int highScore = 0; 
bool gameOver = false;
unsigned long lastGameFrame = 0;
float baseSpeed = 6.0; 
bool isGameInitialized = false;

// --- Network and Drawing Functions ---
bool tryConnectWiFi() {
  tft.fillRect(10, 80, 300, 50, TFT_BLACK); 
  tft.setTextSize(1);
  tft.setCursor(10, 90);
  tft.print("Connecting to Network...");
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
    delay(500);
    tft.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    tft.fillRect(10, 80, 300, 50, TFT_BLACK);
    tft.setCursor(10, 90);
    tft.print("Connection Successful!");
    
    udp.begin(udpPort);
    clientSecure.setInsecure();

    delay(1000);
    return true;
  } else {
    tft.fillRect(10, 80, 300, 50, TFT_BLACK);
    tft.setCursor(10, 90);
    tft.print("Connection Failed!");
    delay(1500);
    return false;
  }
}

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiUrl);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      deserializeJson(doc, payload);
      
      outTemp = doc["main"]["temp"];
      outHumidity = doc["main"]["humidity"];
      dataFetched = true;
    }
    http.end();
  }
}

// Spotify Callback
void updateSpotifyData(CurrentlyPlaying currentlyPlaying) {
  currentTrack = currentlyPlaying.trackName;
  currentArtist = currentlyPlaying.artists[0].artistName;

  isPlaying = currentlyPlaying.isPlaying;
  trackProgress = currentlyPlaying.progressMs;
  trackDuration = currentlyPlaying.durationMs;
  if(trackDuration == 0) trackDuration = 1000; 

  lastApiSyncTime = millis();
}

void checkSpotify() {
  if (WiFi.status() == WL_CONNECTED && !isOfflineMode) {
    if (millis() - lastSpotifyCheck > spotifyDelay) {
      
      spotify.getCurrentlyPlaying(updateSpotifyData, "TR");
      lastSpotifyCheck = millis();
    }
  }
}

void drawHeader() {
  tft.fillRect(0, 0, 320, 25, TFT_BLACK); 
  tft.drawLine(0, 26, 320, 26, TFT_GREEN); 
  
  int tabWidth = 320 / NUM_TABS; 
  tft.setTextSize(1);
  for (int i = 0; i < NUM_TABS; i++) {
    int xPos = i * tabWidth + 5;
    if (i == currentTab) {
      tft.fillRect(i * tabWidth, 0, tabWidth, 25, TFT_GREEN);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
    } else {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
    }
    tft.setCursor(xPos, 8);
    tft.print(tabNames[i]);
  }
}

void drawContent() {
  tft.fillRect(0, 28, 320, 212, TFT_BLACK); 
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  
  switch (currentTab) {
    case 0: 
      tft.setTextSize(2); tft.setCursor(10, 35); tft.println("SYSTEM OVERVIEW"); 
      tft.drawFastHLine(10, 55, 300, TFT_GREEN);
      tft.setTextSize(1);
      tft.setCursor(10, 75);  tft.print("NETWORK IP:");
      tft.setCursor(10, 115); tft.print("MEMORY (RAM):");
      tft.setCursor(10, 155); tft.print("UPTIME:");
      tft.setCursor(10, 195); tft.print("POWER SOURCE:");
      break;
    case 1: 
      tft.setTextSize(2); tft.setCursor(10, 35); tft.println("ENVIRONMENT RADAR"); 
      tft.drawFastHLine(10, 55, 300, TFT_GREEN); 
      tft.setTextSize(1);
      tft.setCursor(10, 70); tft.print("[OUTDOOR : BESIKTAS]");
      tft.setCursor(10, 145); tft.print("[INDOOR : LOCAL]");
      break;
    case 2: 
      tft.setTextSize(2); tft.setCursor(10, 35); tft.println("ACOUSTIC RADAR"); 
      tft.drawRect(8, 55, 304, 155, TFT_GREEN);
      waveX = 10;
      break;
    case 3: 
      tft.setTextSize(2); tft.setCursor(10, 35); tft.println("RADIO TERMINAL"); 
      tft.drawFastHLine(10, 55, 300, TFT_GREEN);
      tft.drawRect(10, 70, 80, 50, TFT_GREEN);
      tft.setTextSize(1);
      tft.setCursor(100, 75); tft.print("NOW PLAYING:");
      tft.setTextSize(2);
      tft.setCursor(100, 95); tft.print("Awaiting API..."); 
      tft.setTextSize(1);
      tft.setCursor(100, 115); tft.print("Artist Name"); 
      tft.drawRect(10, 140, 300, 15, TFT_GREEN);
      tft.setCursor(10, 160); tft.print("0:00");
      tft.setCursor(280, 160); tft.print("0:00");
      tft.drawFastHLine(10, 190, 300, TFT_GREEN);
      tft.setCursor(30, 205); tft.print("[UP/DN] NEXT   [OK] PLAY/PAUSE");
      break;
    case 4: 
      tft.setTextSize(2); tft.setCursor(10, 35); tft.println("MATLAB TELEMETRY"); 
      tft.drawFastHLine(10, 55, 300, TFT_GREEN);
      tft.setTextSize(1);
      tft.setCursor(10, 80); tft.print("STATUS: UDP BROADCASTING...");
      tft.setCursor(10, 110); tft.print("TARGET IP : "); tft.print(pc_ip);
      tft.setCursor(10, 130); tft.print("TARGET PORT : "); tft.print(udpPort);
      tft.setCursor(10, 170); tft.print("PACKET DATA:");
      tft.setCursor(10, 190); tft.print("[MIC] [POT] [TEMP] [LDR]");
      break;
    case 5: 
      tft.setTextSize(2); tft.setCursor(10, 35); tft.println("TACTICAL FLASHLIGHT"); 
      for(int i = 0; i <= 100; i += 10) {
        float angle = (180 - i * 1.8) * PI / 180.0; 
        int x1 = gaugeCenterX + cos(angle) * 80; int y1 = gaugeCenterY - sin(angle) * 80;
        int x2 = gaugeCenterX + cos(angle) * 100; int y2 = gaugeCenterY - sin(angle) * 100;
        tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
      }
      tft.setTextSize(1); tft.setCursor(40, 215); tft.print("0%"); tft.setCursor(250, 215); tft.print("100%");
      lastPercent = -1; 
      break;
    case 6: 
      isGameInitialized = false; 
      break;
  }
}

void updateEnvironment() {
  tft.setTextSize(2);
  if (isOfflineMode) {
    tft.setCursor(10, 90); tft.print("SYSTEM OFFLINE ");
    tft.setTextSize(1); tft.setCursor(10, 115); tft.print("PRESS [OK] TO CONNECT");
    tft.setTextSize(2);
  } else if (dataFetched) {
    tft.setCursor(10, 90); tft.printf("Temperature: %.1f C  ", outTemp);
    tft.setCursor(10, 115); tft.printf("Humidity: %d %%  ", outHumidity);
  } else {
    tft.setCursor(10, 90); tft.print("Awaiting API Data..."); 
  }

  float inTemp = dht.readTemperature();
  float inHumidity = dht.readHumidity();
  int rawLDR = analogRead(LDR_PIN);
  int lightPercent = map(rawLDR, 0, 4095, 0, 100);
  lightPercent = constrain(lightPercent, 0, 100);

  tft.setCursor(10, 165); tft.printf("Temperature: %.1f C  ", inTemp);
  tft.setCursor(10, 185); tft.printf("Humidity: %.1f %%      ", inHumidity); 
  tft.setCursor(10, 205); tft.printf("Ambient Lgt: %d %%      ", lightPercent);
}

void updateRadio() {
  int eqX = 15; int eqY = 115;
  for (int i = 0; i < 5; i++) {
    int barHeight = random(5, 40);
    tft.fillRect(eqX + (i * 14), 75, 10, 40, TFT_BLACK);
    tft.fillRect(eqX + (i * 14), eqY - barHeight, 10, barHeight, TFT_GREEN);
  }
  
  static String lastTrack = "";
  static String lastArtist = "";
  static bool lastNetState = false;
  bool netConnected = (WiFi.status() == WL_CONNECTED && !isOfflineMode);

  if (netConnected != lastNetState) {
    tft.fillRect(100, 75, 210, 10, TFT_BLACK);
    tft.setTextSize(1); tft.setCursor(100, 75); 
    tft.print(netConnected ? "SPOTIFY CONNECTED" : "OFFLINE MODE...");
    lastNetState = netConnected;
  }

  if (currentTrack != lastTrack) {
    tft.fillRect(100, 95, 210, 20, TFT_BLACK);
    tft.setTextSize(2); tft.setCursor(100, 95); 
    tft.print(currentTrack.substring(0, 14)); 
    lastTrack = currentTrack;
  }

  if (currentArtist != lastArtist) {
    tft.fillRect(100, 115, 210, 10, TFT_BLACK);
    tft.setTextSize(1); tft.setCursor(100, 115); 
    tft.print(currentArtist.substring(0, 20));
    lastArtist = currentArtist;
  }

  // --- CLIENT-SIDE PREDICTION ---
  long currentSimulatedProgress = trackProgress;
  
  if (isPlaying && trackDuration > 0) {
    currentSimulatedProgress += (millis() - lastApiSyncTime);
  }
  
  if (currentSimulatedProgress > trackDuration) currentSimulatedProgress = trackDuration;

  
  static int lastSimulatedSecond = -1;
  int currentSimulatedSecond = currentSimulatedProgress / 1000;

  if (currentSimulatedSecond != lastSimulatedSecond) {
    int barWidth = map(currentSimulatedProgress, 0, trackDuration, 0, 300);
    barWidth = constrain(barWidth, 0, 300);
    
    
    tft.fillRect(10, 140, barWidth, 15, TFT_GREEN); 
    tft.fillRect(10 + barWidth, 140, 300 - barWidth, 15, TFT_BLACK); 

    
    int progSec = (currentSimulatedProgress / 1000) % 60;
    int progMin = (currentSimulatedProgress / 60000);
    int durSec = (trackDuration / 1000) % 60;
    int durMin = (trackDuration / 60000);

    tft.setTextSize(1);
    tft.fillRect(10, 160, 50, 10, TFT_BLACK); 
    tft.setCursor(10, 160);
    tft.printf("%d:%02d", progMin, progSec);

    tft.fillRect(270, 160, 40, 10, TFT_BLACK); 
    tft.setCursor(270, 160);
    tft.printf("%d:%02d", durMin, durSec);

    lastSimulatedSecond = currentSimulatedSecond;
    }
  }

void updateSystemStats() {
  tft.setTextSize(2);
  tft.setCursor(10, 90);
  if (WiFi.status() == WL_CONNECTED) {
    tft.print(WiFi.localIP().toString());
  } else {
    tft.print("OFFLINE_LOCAL");
  }

  uint32_t freeHeap = ESP.getFreeHeap() / 1024; 
  tft.setCursor(10, 130);
  tft.printf("%d KB FREE   ", freeHeap); 

  unsigned long currentMillis = millis();
  unsigned long seconds = currentMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  seconds %= 60;
  minutes %= 60;
  
  tft.fillRect(10, 170, 150, 20, TFT_BLACK); 
  tft.setCursor(10, 170);
  tft.printf("%02d:%02d:%02d", hours, minutes, seconds);

  tft.setCursor(10, 210);
  tft.printf("LiPo");
}

void drawWaveform() {
  int rawMic = analogRead(MIC_PIN);
  int mappedY = map(rawMic, 1750, 2050, 208, 57); mappedY = constrain(mappedY, 57, 208); 
  tft.drawFastVLine(waveX + 1, 56, 153, TFT_BLACK); tft.drawFastVLine(waveX + 2, 56, 153, TFT_BLACK); 
  tft.drawLine(waveX - 1, lastWaveY, waveX, mappedY, TFT_GREEN); lastWaveY = mappedY; waveX++;
  if (waveX > 310) { waveX = 10; tft.drawFastVLine(waveX, 56, 153, TFT_BLACK); }
}

void updateLightGauge() {
  int rawPot = analogRead(POT_PIN); smoothedPot = (smoothedPot * 0.92) + (rawPot * 0.08);
  int percent = map((int)smoothedPot, 0, 4095, 0, 100); percent = constrain(percent, 0, 100);
  float fraction = percent / 100.0; int pwmValue = pow(fraction, 3.0) * 255; analogWrite(LED_PIN, pwmValue);
  if (percent != lastPercent) {
    if (lastPercent != -1) {
      float oldAngle = (180 - lastPercent * 1.8) * PI / 180.0;
      tft.drawLine(gaugeCenterX, gaugeCenterY, gaugeCenterX + cos(oldAngle) * 90, gaugeCenterY - sin(oldAngle) * 90, TFT_BLACK);
      tft.fillCircle(gaugeCenterX, gaugeCenterY, 5, TFT_BLACK); 
    }
    float newAngle = (180 - percent * 1.8) * PI / 180.0;
    tft.drawLine(gaugeCenterX, gaugeCenterY, gaugeCenterX + cos(newAngle) * 90, gaugeCenterY - sin(newAngle) * 90, TFT_GREEN);
    tft.fillCircle(gaugeCenterX, gaugeCenterY, 5, TFT_GREEN); 
    tft.fillRect(130, 120, 60, 30, TFT_BLACK); tft.setTextSize(3); tft.setTextColor(TFT_GREEN, TFT_BLACK);
    if(percent < 10) tft.setCursor(150, 125); else if(percent < 100) tft.setCursor(140, 125); else tft.setCursor(130, 125);
    tft.print(percent); lastPercent = percent; 
  }
}

void sendTelemetry() {
  if (millis() - lastTelemetryTime > 100) {
    if (WiFi.status() == WL_CONNECTED && !isOfflineMode) {
      
      // 1. SOFTWARE GAIN
      int micMax = 0;
      int micMin = 4095;
      for(int i = 0; i < 50; i++) { 
        int sample = analogRead(MIC_PIN);
        if(sample > micMax) micMax = sample;
        if(sample < micMin) micMin = sample;
        delayMicroseconds(200); 
      }
      int micAmplitude = micMax - micMin; 
      
      micAmplitude = micAmplitude * 15; 
      if (micAmplitude > 4095) micAmplitude = 4095; 

      // 2. READ OTHER SENSORS
      int rawPot = analogRead(POT_PIN);
      float temp = dht.readTemperature();
      
      // 3. AC FLICKER NOISE REDUCTION
      analogRead(LDR_PIN); 
      delay(2); 
      int rawLDR = analogRead(LDR_PIN);
      
      static float smoothedLDR = 0;
      if (smoothedLDR == 0) smoothedLDR = rawLDR; 
      smoothedLDR = (smoothedLDR * 0.85) + (rawLDR * 0.15); // Mathematical Filter
      
      // 4. UDP PACKAGE THROWN AT MATLAB
      String payload = String(micAmplitude) + "," + String(rawPot) + "," + String(temp) + "," + String((int)smoothedLDR);
      udp.beginPacket(pc_ip, udpPort);
      udp.print(payload);
      udp.endPacket();
    }
    lastTelemetryTime = millis();
  }
}

void resetGame() {
  shipLane = 1;
  obsX = 320;
  lastObsX = 320;
  obsLane = random(0, 3);
  gameScore = 0;
  baseSpeed = 6.0; 
  gameOver = false;
  
  tft.fillRect(0, 28, 320, 212, TFT_BLACK); 
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2); 
  tft.setCursor(10, 35); 
  tft.println("ORBIT DEFENSE"); 
  tft.drawFastHLine(10, 55, 300, TFT_GREEN);
  tft.drawFastHLine(10, 220, 300, TFT_GREEN);
  for(int i = 10; i < 310; i += 20) {
    tft.drawFastHLine(i, 115, 8, TFT_GREEN);
    tft.drawFastHLine(i, 165, 8, TFT_GREEN);
  }
}

void updateGame() {
  if (!isGameInitialized) { resetGame(); isGameInitialized = true; }

  if (gameOver) {
    if (gameScore > highScore) highScore = gameScore; 
    tft.setTextSize(3); tft.setCursor(70, 110); tft.print("GAME OVER");
    tft.setTextSize(1); tft.setCursor(85, 150); tft.print("PRESS [OK] TO RESTART");
    if (digitalRead(BTN_OK) == LOW) { resetGame(); delay(200); }
    return;
  }

  static bool lastUpGame = HIGH;
  static bool lastDownGame = HIGH;
  bool currUp = digitalRead(BTN_UP);
  bool currDown = digitalRead(BTN_DOWN);
  bool currOk = digitalRead(BTN_OK);

  if (currUp == LOW && lastUpGame == HIGH) {
    if (shipLane > 0) { tft.fillTriangle(20, laneY[shipLane]-10, 20, laneY[shipLane]+10, 40, laneY[shipLane], TFT_BLACK); shipLane--; }
  }
  if (currDown == LOW && lastDownGame == HIGH) {
    if (shipLane < 2) { tft.fillTriangle(20, laneY[shipLane]-10, 20, laneY[shipLane]+10, 40, laneY[shipLane], TFT_BLACK); shipLane++; }
  }
  lastUpGame = currUp; lastDownGame = currDown;

  float currentSpeed = baseSpeed;
  if (currOk == LOW) {
    currentSpeed = baseSpeed * 1.6; 
    static int boostTick = 0;
    if (boostTick++ % 10 == 0) gameScore += 1; 
  }

  if (millis() - lastGameFrame > 20) { 
    lastGameFrame = millis();
    float tailWidth = lastObsX - obsX + 2; 
    tft.fillRect(obsX + 16, laneY[obsLane] - 8, tailWidth, 16, TFT_BLACK);
    lastObsX = obsX; obsX -= currentSpeed;

    if (obsX <= 40 && obsX + 16 >= 20 && shipLane == obsLane) { gameOver = true; return; }

    if (obsX < 0) {
      tft.fillRect(0, laneY[obsLane] - 8, 20, 16, TFT_BLACK); 
      obsX = 320; lastObsX = 320; obsLane = random(0, 3); gameScore += 15;
      if (baseSpeed < 18.0) baseSpeed += 0.15; 
    }

    tft.fillTriangle(20, laneY[shipLane]-10, 20, laneY[shipLane]+10, 40, laneY[shipLane], TFT_GREEN);
    tft.fillRect(obsX, laneY[obsLane] - 8, 16, 16, TFT_GREEN);
    tft.fillRect(190, 35, 120, 15, TFT_BLACK); 
    tft.setTextSize(1); tft.setCursor(195, 38); tft.print("HI: "); tft.print(highScore);
    tft.setCursor(255, 38); tft.print("SC: "); tft.print(gameScore);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP); pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT); 
  smoothedPot = analogRead(POT_PIN); dht.begin();
  tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextSize(2);
  tft.setCursor(10, 20); tft.println("VAULT-OS BIOS v1.0");
  tft.drawFastHLine(10, 45, 300, TFT_GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 70); tft.println("NETWORK (WI-FI) SELECTION:");
  tft.setCursor(10, 110); tft.println("[LEFT BUTTON]  -> OFFLINE MODE (Fast Boot)");
  tft.setCursor(10, 140); tft.println("[RIGHT BUTTON] -> ONLINE MODE  (API & Wi-Fi)");
  tft.setCursor(10, 190); tft.println("Awaiting Selection_");

  while (true) {
    if (digitalRead(BTN_LEFT) == LOW) {
      isOfflineMode = true; 
      tft.fillRect(10, 180, 300, 30, TFT_BLACK); tft.setCursor(10, 190); tft.println("OFFLINE MODE SELECTED. Booting...");
      delay(1000); break; 
    }
    if (digitalRead(BTN_RIGHT) == LOW) {
      isOfflineMode = false; 
      tft.fillRect(10, 180, 300, 30, TFT_BLACK); tft.setCursor(10, 190); tft.println("ONLINE MODE SELECTED. Connecting...");
      break; 
    }
    delay(50); 
  }

  if (!isOfflineMode) {
    if(tryConnectWiFi()) {
      fetchWeatherData();
    } else {
      isOfflineMode = true; 
    }
  }

  drawHeader();
  drawContent();
}

void loop() {
  bool currentLeftState = digitalRead(BTN_LEFT);
  bool currentRightState = digitalRead(BTN_RIGHT);
  bool currentUpState = digitalRead(BTN_UP);
  bool currentDownState = digitalRead(BTN_DOWN);
  bool currentOkState = digitalRead(BTN_OK);
  bool tabChanged = false;

  if (currentLeftState == LOW && lastLeftState == HIGH) {
    currentTab--; if (currentTab < 0) currentTab = NUM_TABS - 1; 
    tabChanged = true; delay(50);
  }
  if (currentRightState == LOW && lastRightState == HIGH) {
    currentTab++; if (currentTab >= NUM_TABS) currentTab = 0; 
    tabChanged = true; delay(50);
  }
  
  if (tabChanged) { drawHeader(); drawContent(); }
  
  if (!isOfflineMode && (millis() - lastApiTime > 600000)) {
    fetchWeatherData(); lastApiTime = millis();
  }

  if (currentTab == 0) {
    updateSystemStats();
    delay(200); 
  } 
  else if (currentTab == 1) {
    if (isOfflineMode && currentOkState == LOW && lastOkState == HIGH) {
      if(tryConnectWiFi()) {
        isOfflineMode = false; 
        fetchWeatherData();    
      }
      drawContent(); 
    }
    updateEnvironment();
    delay(100); 
  }
  else if (currentTab == 2) { drawWaveform(); } 
  else if (currentTab == 3) { 
    updateRadio(); 
    checkSpotify(); 
    
    // SPOTIFY MEDIA CONTROLS
    if (!isOfflineMode && WiFi.status() == WL_CONNECTED) {
      if (currentOkState == LOW && lastOkState == HIGH) {
        if (isPlaying) {
          spotify.pause();
          isPlaying = false; 
        } else {
          spotify.play();
          isPlaying = true;
        }
        currentTrack = "Loading..."; 
      }
      
      if (currentUpState == LOW && lastUpState == HIGH) {
        spotify.previousTrack();
        currentTrack = "Loading..."; 
      }
      if (currentDownState == LOW && lastDownState == HIGH) {
        spotify.nextTrack();
        currentTrack = "Loading..."; 
      }
    }
    delay(150); 
  } else if (currentTab == 4) { 
    sendTelemetry(); 
  } else if (currentTab == 5) { 
    updateLightGauge(); 
  } else if (currentTab == 6) { 
    updateGame(); 
  } else { delay(10); }
  
  lastLeftState = currentLeftState; 
  lastRightState = currentRightState;
  lastUpState = currentUpState;
  lastDownState = currentDownState;
  lastOkState = currentOkState; 
}