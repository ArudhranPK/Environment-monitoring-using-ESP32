#include <main.h>

// Function declarations
bool readEnvironment(float *temperature, float *pressure, float *humidity);
void connectAWS();
void publishMessage();

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  
  Serial.println("Starting ESP32...");
  
  dht.begin();
  
  //  Assign different pins rather than using default
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!bmp.begin(0x76)) {  // 0x76, 0x77
    Serial.println("Could not find BMP280 sensor!");
    while (1);
  }
  
  Serial.println("BMP280 sensor initialized");

  //  Connect to WiFi and AWS
  connectAWS();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Getting time from NTP...");
}

void loop() {
  client.loop(); // Keep MQTT connection alive
  
  if (!client.connected()) {
    Serial.println("MQTT disconnected. Reconnecting...");
    while (!client.connect("ESP32_Client")) {
      Serial.print(".");
      delay(1000);
    }
    Serial.println("Reconnected!");
  }
  
  if (readEnvironment(&temperature, &pressure, &humidity)) {
    Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" °C");
    Serial.print("Pressure: "); Serial.print(pressure); Serial.println(" hPa");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
    
    publishMessage();
  } else {
    Serial.println("Error reading sensors.");
  }

  delay(2000); // Publish every 5 seconds
}

// Function definitions
bool readEnvironment(float *temperature, float *pressure, float *humidity) {

  // Read BMP280
  float bmpTemp = bmp.readTemperature();
  float bmpPressure = bmp.readPressure() / 100.0F;  // Convert to hPa
  
  // Read DHT11
  float dhtTemp = dht.readTemperature();
  float dhtHum = dht.readHumidity();

  // Check for NaN values
  if (isnan(bmpTemp) || isnan(bmpPressure) || isnan(dhtTemp) || isnan(dhtHum)) {
    Serial.println("Sensor read error!");
    return false;
  }

  if (getLocalTime(&timeinfo)) {
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    Serial.print("IST: ");
    Serial.println(timeString);
  } else {
    Serial.println("Failed to get time");
  }

  *temperature = bmpTemp;
  *pressure = bmpPressure;
  *humidity = dhtHum;

  return true;
}

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  //  AWS MQTT client setup
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
  
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  
  Serial.println("Connecting to AWS IoT...");
  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }
  
  if (client.connected()) {
    Serial.println("\nAWS IoT Connected!");
  }
}

void publishMessage()
{
  JsonDocument doc;
  doc["timestamp"] = timeString;
  doc["temperature"] = temperature;
  doc["pressure"] = pressure;
  doc["humidity"] = humidity;

  char jsonBuffer[1024];
  serializeJson(doc, jsonBuffer);
  
  Serial.print("Publishing: ");
  Serial.println(jsonBuffer);
  
  if (client.publish(Publish_TOPIC, jsonBuffer)) {
    Serial.println("Message published successfully");
  } else {
    Serial.println("Message publish failed");
  }
}