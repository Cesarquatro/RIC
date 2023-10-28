/*
 * Docente: Eduardo Paciência Godoy
 * Discente: Cesar Augusto Mendes Cordeiro da Silva
 * Discente: Lícia Takahashi dos Santos
 * Projeto 1 - COMUNICAÇÃO COM GOOGLE SHEETS E TELEGRAM
 * 15/09/2023
 */

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h> // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

//-----------BOTÃO--------------
const int botaoPrg = 0;
const int led = 25;
bool estadoPrg = false;

//-----------WIFI---------------
// Replace with your network credentials
//const char* ssid = "labautomacao";
//const char* password = "labaula2023";
const char* ssid = "RedeWifi";
const char* password = "SenhaWifi";

//-----------IFTTT---------------
// Replace with your unique IFTTT URL resource
const char* resource = "/trigger/bme280_leitura/with/key/CHAVE_WEBHOOKS";

// Maker Webhooks IFTTT
const char* server = "maker.ifttt.com";

//-----------Telegram---------------
// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID "ID_CHAT_BOT"

#define SEALEVELPRESSURE_HPA (1029.9) //Sorocaba

// Initialize Telegram BOT
#define BOTtoken "TOKEN_CHAT_BOT"  // your Bot Token (Get from Botfather)

#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

//Checks for new messages every 1 second.
int botRequestDelay = 1000;
const long planilhaUpdateDelay = 10 * 1000;
unsigned long lastTimeBotRan;
unsigned long lastTimePlanilhaRan = 0;

//-----------Sensor---------------
// BME280 connect to ESP32 I2C (GPIO 21 = SDA, GPIO 22 = SCL)
// BME280 connect to ESP8266 I2C (GPIO 4 = SDA, GPIO 5 = SCL)
Adafruit_BME280 bme;

//-----------Leitura Telegram---------------
// Get BME280 sensor readings and return them as a String variable
String getReadings() {
  float temperature, humidity, sealevel, pressao;
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  sealevel = bme.readAltitude(SEALEVELPRESSURE_HPA);
  pressao = bme.readPressure() / 100.0F;
  String message = "Temperatura: " + String(temperature) + " ºC \n";
  message += "Umi: " + String (humidity) + " % \n";
  message += "Alt: " + String (sealevel) + " m \n";
  message += "Pres: " + String (pressao) + " hPa \n";
  message += "Bot: " + String (estadoPrg) + "\n";
  return message;
}

//Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Bem vinda, Deusa " + from_name + "!\n";
      welcome += "Use o seguinte comando para realizar as leituras.\n\n";
      welcome += "/leituras \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/leituras") {
      String readings = getReadings();
      bot.sendMessage(chat_id, readings, "");
      makeIFTTTRequest1();
      makeIFTTTRequest2();
      makeIFTTTRequest3();
    }
  }
}

void setup() {
  Serial.begin(115200);

#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
  client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
#endif

  // Init BME280 sensor
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
#ifdef ESP32
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  pinMode(botaoPrg, INPUT);
  pinMode(led, OUTPUT);
}

void loop() {

  if (digitalRead(botaoPrg) == LOW) {
    estadoPrg = !estadoPrg;
    Serial.print("estadoPrg = ");
    Serial.println(estadoPrg);
    if (estadoPrg == true) {
      digitalWrite(led, HIGH);
    }
    else {
      digitalWrite(led, LOW);
    }
  }

  delay(1000); // Adicione um atraso para evitar leituras repetidas muito rápidas

  unsigned long currentTime = millis();
  if ((currentTime - lastTimePlanilhaRan) >= planilhaUpdateDelay) {
    lastTimePlanilhaRan = currentTime;
    //atualiza planilha aqui
    makeIFTTTRequest1();
    makeIFTTTRequest2();
    makeIFTTTRequest3();
  }
  //if (millis() > lastTimePlanilhaRan + planilhaUpdateDelay)  {
  //    //atualiza planilha aqui
  //makeIFTTTRequest1();
  //}
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    //atualiza planilha aqui
    //makeIFTTTRequest1();

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

// Make an HTTP request to the IFTTT web service
void makeIFTTTRequest1() {
  Serial.print("Connecting to ");
  Serial.print(server);

  WiFiClient client;
  int retries = 5;
  while (!!!client.connect(server, 80) && (retries-- > 0)) {
    Serial.print(".");
  }
  Serial.println();
  if (!!!client.connected()) {
    Serial.println("Failed to connect...");
  }

  Serial.print("Request resource: ");
  Serial.println(resource);

  float temperature, humidity, sealevel, pressao;
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  sealevel = bme.readAltitude(SEALEVELPRESSURE_HPA);
  pressao = bme.readPressure() / 100.0F;

  // Temperature in Celsius
  String jsonObject;
  jsonObject = String("{\"value1\":\"") + "T_U";
  jsonObject += String("\",\"value2\":\"") + temperature;
  jsonObject += String("\",\"value3\":\"") + humidity + "\"}";

  client.println(String("POST ") + resource + " HTTP/1.1");
  client.println(String("Host: ") + server);
  client.println("Connection: close\r\nContent-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonObject.length());
  client.println();
  client.println(jsonObject);

  int timeout = 5 * 10; // 5 seconds
  while (!!!client.available() && (timeout-- > 0)) {
    delay(100);
  }
  if (!!!client.available()) {
    Serial.println("No response...");
  }
  while (client.available()) {
    Serial.write(client.read());
  }

  Serial.println("\nclosing connection");
  client.stop();
}
void makeIFTTTRequest2() {
  Serial.print("Connecting to ");
  Serial.print(server);

  WiFiClient client;
  int retries = 5;
  while (!!!client.connect(server, 80) && (retries-- > 0)) {
    Serial.print(".");
  }
  Serial.println();
  if (!!!client.connected()) {
    Serial.println("Failed to connect...");
  }

  Serial.print("Request resource: ");
  Serial.println(resource);

  float temperature, humidity, sealevel, pressao;
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  sealevel = bme.readAltitude(SEALEVELPRESSURE_HPA);
  pressao = bme.readPressure() / 100.0F;

  // Temperature in Celsius
  String jsonObject;
  jsonObject = String("{\"value1\":\"") + "A_P";
  jsonObject += String("\",\"value2\":\"") + sealevel;
  jsonObject += String("\",\"value3\":\"") + pressao + "\"}";

  client.println(String("POST ") + resource + " HTTP/1.1");
  client.println(String("Host: ") + server);
  client.println("Connection: close\r\nContent-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonObject.length());
  client.println();
  client.println(jsonObject);

  int timeout = 5 * 10; // 5 seconds
  while (!!!client.available() && (timeout-- > 0)) {
    delay(100);
  }
  if (!!!client.available()) {
    Serial.println("No response...");
  }
  while (client.available()) {
    Serial.write(client.read());
  }

  Serial.println("\nclosing connection");
  client.stop();
}
void makeIFTTTRequest3() {
  Serial.print("Connecting to ");
  Serial.print(server);

  WiFiClient client;
  int retries = 5;
  while (!!!client.connect(server, 80) && (retries-- > 0)) {
    Serial.print(".");
  }
  Serial.println();
  if (!!!client.connected()) {
    Serial.println("Failed to connect...");
  }

  Serial.print("Request resource: ");
  Serial.println(resource);

  float temperature, humidity, sealevel, pressao;
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  sealevel = bme.readAltitude(SEALEVELPRESSURE_HPA);
  pressao = bme.readPressure() / 100.0F;

  // Temperature in Celsius
  String jsonObject;
  jsonObject = String("{\"value1\":\"") + "B_Nan";
  jsonObject += String("\",\"value2\":\"") + estadoPrg;
  jsonObject += String("\",\"value3\":\"") + "Nan" + "\"}";

  client.println(String("POST ") + resource + " HTTP/1.1");
  client.println(String("Host: ") + server);
  client.println("Connection: close\r\nContent-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonObject.length());
  client.println();
  client.println(jsonObject);

  int timeout = 5 * 10; // 5 seconds
  while (!!!client.available() && (timeout-- > 0)) {
    delay(100);
  }
  if (!!!client.available()) {
    Serial.println("No response...");
  }
  while (client.available()) {
    Serial.write(client.read());
  }

  Serial.println("\nclosing connection");
  client.stop();
}
