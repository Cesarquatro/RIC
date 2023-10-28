#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <AsyncElegantOTA.h> //ota linha1

//const char* ssid = "labautomacao";
//const char* password = "labaula2023";
const char* ssid = "Cesarquatro";
const char* password = "semsenha";

float temperature = 0.0;
float humidity = 0.0;
float altitude = 0.0;
float pressure = 0.0;

#define SEALEVELPRESSURE_HPA (1029.9) // Pressão atmosférica ao nível do mar para Sorocaba
#define I2C_SDA 21            // Definindo portas I2C
#define I2C_SCL 22            // Definindo portas I2C
TwoWire I2CBME = TwoWire(1);  // Definindo portas I2C entre OLED e BME280, OLED usa por padrão Wire(0) e BME280 vai usar Wire(1)
Adafruit_BME280 bme; // I2C

bool ledState = 0;
bool botaoState = 0;
const int ledPin = 25;
const int botaoPrg = 0;
int estadoPrg = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2{
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  .content {
    padding: 30px;
    max-width: 600px;
    margin: 0 auto;
  }
  .card {
    background-color: #F8F7F9;;
    box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    padding-top:10px;
    padding-bottom:20px;
  }
  .button {
    padding: 15px 50px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #0f8b8d;
    border: none;
    border-radius: 5px;
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
    -webkit-tap-highlight-color: rgba(0,0,0,0);
   }
   /*.button:hover {background-color: #0f8b8d}*/
   .button:active {
     background-color: #0f8b8d;
     box-shadow: 2 2px #CDCDCD;
     transform: translateY(2px);
   }
   .state {
     font-size: 1.5rem;
     color:#8c8c8c;
     font-weight: bold;
   }
  </style>
<title>ESP Web Server</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
</head>
<body>
  <div class="topnav">
    <h1>ESP WebSocket Server</h1>
  </div>
  <div class="content">
    <div class="card">
      <h2>Output - GPIO 2</h2>
      <p class="state">state: <span id="state">%STATE%</span></p>
      <p><button id="button" class="button">Toggle</button></p>
    </div>
  </div>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    var state;
    if (event.data == "1"){
      state = "ON";
    }
    else{
      state = "OFF";
    }
    document.getElementById('state').innerHTML = state;
  }
  function onLoad(event) {
    initWebSocket();
    initButton();
  }
  function initButton() {
    document.getElementById('button').addEventListener('click', toggle);
  }
  function toggle(){
    websocket.send('toggle');
  }
</script>
</body>
</html>
)rawliteral";

// Função para notificar todos os clientes WebSocket com os dados do sensor.
void notifyClients() {
  if (digitalRead(botaoPrg) == LOW) {
    estadoPrg = 1;
    Serial.print("NOTIFY_Prg = ");
    Serial.println(estadoPrg);
  }
  else{
    estadoPrg = 0;
    Serial.print("NOTIFY_Prg = ");
    Serial.println(estadoPrg);
  }
  
  String json;
  json = String(ledState) + ",";
  json += String(temperature, 1) + ",";
  json += String(humidity, 1) + ",";
  json += String(altitude, 1) + ",";
  json += String((pressure/100), 1) + ",";
  json += String(estadoPrg) + ",";
  ws.textAll(json);
  Serial.println(json);
}

// Função para lidar com mensagens WebSocket.
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    // Se a mensagem for "toggle", alterna o estado do LED.
    if (strcmp((char*)data, "toggle") == 0) {
      ledState = !ledState;
    }
  }
}

// Função para lidar com eventos WebSocket.
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Inicializa o WebSocket.
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if (ledState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  return String();
}

// Configuração inicial.
void setup(){
  pinMode(ledPin, OUTPUT);

  //led
  for(int i = 0; i < 10; i++){
    digitalWrite(ledPin, 1);
    delay(100);
    digitalWrite(ledPin, 0);
    delay(100);
   }  
  //led
  
  Serial.begin(115200);

  // Conectar ao Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }

  // Imprimir o endereço IP no monitor serial
  Serial.print("Endereço IP do WebSocket Server: ");
  Serial.println(WiFi.localIP());

  // Iniciar WebSocket
  initWebSocket();

  //Ota linha3
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

   // Iniciar o servidor
  server.begin();

  I2CBME.begin(I2C_SDA, I2C_SCL, 100000); // Definindo portas I2C
  bool status; 
  // Iniciar o sensor BME280
  //status = bme.begin(0x76); 
  status = bme.begin(0x76, &I2CBME);      // Definindo portas I2C
  if (!status) {
    Serial.println("Sensor BME280 - Não OK");
    delay(1000);
    while (1); // Se BME não disponível, não funciona!
  }
  else {
    Serial.println("Sensor BME280 - OK");
    delay(1000);
  }
  
  pinMode(botaoPrg, INPUT);
  // Iniciar o ElegantOTA
  AsyncElegantOTA.begin(&server);  //ota linha2
  // Iniciar o servidor
  server.begin();
}

// Loop principal
void loop() {
  // Notificar clientes WebSocket
 
  notifyClients();
  ws.cleanupClients();

  digitalWrite(ledPin, ledState);

  // Ler dados do sensor BME280
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure();
  altitude = bme.readAltitude(float SEALEVELPRESSURE_HPA);

  delay(1000);
}
