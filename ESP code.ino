#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Wifi :D
const char* ssid = "xx";
const char* password = "xx";

// OpenWeatherMap API
const char* apiKey = "secret";

// kaupungit
const char* locations[][2] = {
  {"Oulu", "FI"},
  {"Linnanmaa", "FI"},
  {"Tampere", "FI"}
};
int currentLocation = 0;

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button settings + pins
const int buttonPin = 0; // GPIO10 for the pushbutton
const int resetButtonPin = 2; // GPIO2 for the reset button
bool buttonPressed = false;
bool showWeather = true;

// HC-SR04P pins
#define TRIG_PIN 14 // GPIO14
#define ECHO_PIN 12 // GPIO12

// Display timeout,MS
unsigned long lastMovementTime = 0;
const unsigned long displayTimeout = 40000;

// Display state tracking
bool displayIsOn = true;

WiFiClient client;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000); // UTC+3 > UTC 2 7200

// Auto updates
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 1000;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(resetButtonPin, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, 4, 5)) { // SDA GPIO4, SCL GPIO5
    Serial.println("OLED display failed to initialize!");
    for (;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  connectToWiFi();
  timeClient.begin();
  fetchWeatherData();
}

void loop() {
  float distance = measureDistance();

  if (distance > 0 && distance <= 10.0) {
    Serial.println("Movement detected!");
    wakeDisplay();  // asd
  }

  // Display timeout
  if (millis() - lastMovementTime > displayTimeout && displayIsOn) {
    Serial.println("Display timeout reached. Turning off display.");
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    displayIsOn = false;
  }

  // Handle button press
  if (digitalRead(buttonPin) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      
      if (!displayIsOn) {
        Serial.println("Waking display from button press!");
        wakeDisplay();  // wake screen if off
      } else {
        Serial.println("Button pressed while display is on");
        // cycle screens
        if (showWeather) {
          currentLocation = (currentLocation + 1) % (sizeof(locations) / sizeof(locations[0]));
          if (currentLocation == 0) {
            showWeather = false;
            fetchTimeData();
          } else {
            fetchWeatherData();
          }
        } else {
          showWeather = true;
          fetchWeatherData();
        }
      }
      delay(300); // debounce
    }
  } else {
    buttonPressed = false;
  }

  if (digitalRead(resetButtonPin) == LOW) {
    Serial.println("Reset button pressed. Restarting ESP...");
    ESP.restart();
    delay(300);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectToWiFi();
  }

  if (millis() - lastUpdateTime >= updateInterval) {
    lastUpdateTime = millis();
    if (!showWeather) {
      fetchTimeData();
    }
  }
}

// function to handle waking up screen
void wakeDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.display();
  lastMovementTime = millis();
  displayIsOn = true;
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 50000);
  if (duration == 0) {
    Serial.println("Error: No echo received.");
    return -1;
  }

  float distance = duration * 0.034 / 2;
  if (distance < 2 || distance > 400) {
    Serial.println("Invalid distance.");
    return -1;
  }

  Serial.print("Distance: ");
  Serial.println(distance);
  return distance;
}

void connectToWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Yhistyy wifiin XDD");
  display.display();

  WiFi.begin(ssid, password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    attempt++;
    if (attempt > 20) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Failed XDD");
      display.display();
      delay(5000);
      ESP.restart();
    }
  }
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Yhistetty XDD");
  display.display();
  delay(2000);
  display.clearDisplay();
}

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    String city = locations[currentLocation][0];
    String country = locations[currentLocation][1];
    String weatherURL = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + country + "&appid=" + String(apiKey) + "&units=metric";

    HTTPClient http;
    http.begin(client, weatherURL);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      // Extract weather data
      float temperatureC = doc["main"]["temp"];
      float temperatureF = temperatureC * 9.0 / 5.0 + 32.0;
      float temperatureK = temperatureC + 273.15;
      String weatherDescription = doc["weather"][0]["description"];

      //oled data
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println(city + ":");
      display.setCursor(0, 20);
      display.println(String(temperatureC) + " C");
      display.setCursor(0, 30);
      display.println(String(temperatureF) + " F");
      display.setCursor(0, 40);
      display.println(String(temperatureK) + " K");
      display.setCursor(0, 50);
      display.println(weatherDescription);
      display.display();
    }
    http.end();
  } else {
    connectToWiFi();
  }
}

void fetchTimeData() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  String formattedTime = getFormattedTimeWithoutSeconds(timeClient.getFormattedTime());
  String formattedDate = getFormattedDate(epochTime);
  int weekNumber = getWeekNumber(epochTime);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Kellonaika:");
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(formattedTime);
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.println(formattedDate);
  display.setCursor(0, 50);
  display.println("Viikko " + String(weekNumber));
  display.display();
}

String getFormattedTimeWithoutSeconds(String formattedTime) {
  return formattedTime.substring(0, 5);
}

String getFormattedDate(unsigned long epochTime) {
  time_t rawTime = epochTime;
  struct tm *timeInfo = localtime(&rawTime);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d.%m.%Y", timeInfo);
  return String(buffer);
}

int getWeekNumber(unsigned long epochTime) {
  time_t rawTime = epochTime;
  struct tm *timeInfo = localtime(&rawTime);
  char buffer[3];
  strftime(buffer, sizeof(buffer), "%V", timeInfo);
  return atoi(buffer);
}
