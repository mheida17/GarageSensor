#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <DHT.h>
#include <Config.h>

/***** BOARD DEFINES *****/
#define ONBOARD_LED 2
#define MEASUREMENT_INTERVAL_MS (500)
#define DHT_MEASUREMENT_COUNT 10 // Measure DHT every 5s
#define DD_SWITCH_PIN 39 //Double door input
#define SD_SWITCH_PIN 36 //Single door input
#define GreenLED 18
#define RedLED 16
#define YellowLED 0
#define ClearLED ONBOARD_LED
#define BlueLED 15
#define DHT_INPUT 4
#define DHTTYPE DHT22
DHT dht(DHT_INPUT, DHTTYPE);

/***** LOCAL VALUES *****/
static WiFiClient espClient;
static PubSubClient client(_mqtt_server, 1883, espClient);
static int loop_count = 0;

/***** LOCAL FUNCTIONS *****/
static void setup_wifi(void);
static void setup_ota(void);
static void setup_board(void);
static void update_garage_state(void);
static void update_dht(void);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  if (client.connect(_clientID)) {
    Serial.println("Connected to MQTT Broker!");
  } else {
    Serial.println("Connection to MQTT Broker failed...");
  }
  setup_ota();
  setup_board();
}

void loop() {
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  if (!client.connected()) {
    if (client.connect(_clientID)) {
      Serial.println("Re-Connected to MQTT Broker!");
    } else {
      Serial.println("Still not connected to MQTT Broker...");
      goto cleanup;
    }
  }
  update_garage_state();
  if (DHT_MEASUREMENT_COUNT <= loop_count) {
    update_dht();
    loop_count = 0;
  }
  
cleanup:
  delay(MEASUREMENT_INTERVAL_MS);
  loop_count++;
}

static void update_garage_state(void)
{
  if (digitalRead(DD_SWITCH_PIN)) { // Reads Double Door position
    digitalWrite(GreenLED, HIGH);
    client.publish(_mqtt_topic_main_door, "open");
  }
  else {
    digitalWrite(GreenLED, LOW);
    client.publish(_mqtt_topic_main_door, "closed");
  }
  if (digitalRead(SD_SWITCH_PIN)) {   //Reads Single Door position
    digitalWrite(RedLED, HIGH);
    client.publish(_mqtt_topic_small_door, "open");
  }
  else {
    digitalWrite(RedLED, LOW);
    client.publish(_mqtt_topic_small_door, "closed");
  }
}

static void update_dht(void) {
  float hum = dht.readHumidity();
  float temp = dht.readTemperature(true);
  if (isnan(hum)) {
    Serial.println("Error reading hum");
  }
  if (isnan(temp)) {
    Serial.println("Error reading temp");
  }
  if (isnan(hum) || isnan(temp)) {
    Serial.println("Error reading DHT22");
  } else {
    Serial.println("Temp = " + String(temp));
    Serial.println("Hum = " + String(hum));
    client.publish(_mqtt_topic_temp, String(temp).c_str());
    client.publish(_mqtt_topic_hum, String(hum).c_str());
  }
}

static void setup_wifi(void) {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(_ssid);

  WiFi.begin(_ssid, _password);
  bool ledState = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(ClearLED, ledState); // Also onboard LED
    ledState = !ledState; // Flip ledState
  }
  digitalWrite(ClearLED, HIGH); // WiFi connected

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

static void setup_board(void)
{
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(DD_SWITCH_PIN, INPUT);
  pinMode(SD_SWITCH_PIN, INPUT);
  pinMode(GreenLED, OUTPUT); // Double Garage Door Status
  pinMode(RedLED, OUTPUT); //Single Garage Door Status
  pinMode(YellowLED, OUTPUT); // Change in status
  pinMode(ClearLED, OUTPUT); // Not used maybe status to RPI
  pinMode(BlueLED, OUTPUT); // WIFI connected status
  dht.begin();
}

void setup_ota()
{
  ArduinoOTA.setPassword(_ota_password);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}