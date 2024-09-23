#include "painlessMesh.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Arduino_JSON.h>
#include <AESLib.h>

#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555

#define DHTPIN 4     // Pin al que está conectado el sensor DHT
#define DHTTYPE DHT11 // Tipo de sensor: DHT11

DHT dht(DHTPIN, DHTTYPE);

Scheduler userScheduler;
painlessMesh mesh;

int nodeNumber = 1;  // Número del nodo

// Clave de encriptación y vector de inicialización
byte aesKey[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}; // Tu clave aquí
byte iv[16] = {0}; // Tu vector de inicialización
AESLib aesLib; // Crea una instancia de AESLib

void sendMessage();
String getReadings();

Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);

String getReadings() {
  JSONVar jsonReadings;
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (!isnan(temp) && !isnan(hum) && temp >= 0 && temp <= 50 && hum >= 0 && hum <= 100) {
    jsonReadings["node"] = nodeNumber;
    jsonReadings["temp"] = temp;
    jsonReadings["hum"] = hum;

    String jsonString = JSON.stringify(jsonReadings);
    
    // Encriptar el mensaje
    byte encrypted[128];
    int len = aesLib.encrypt((byte*)jsonString.c_str(), jsonString.length(), encrypted, aesKey, 128, iv);
    return String((char*)encrypted, len);
  } else {
    Serial.println("Error en las lecturas del sensor o datos atípicos.");
    
    // Enviar un mensaje de error específico a los LEDs
    JSONVar errorJson;
    errorJson["node"] = nodeNumber;
    errorJson["error"] = "Invalid"; // Puedes usar un código de error específico si lo deseas

    String errorString = JSON.stringify(errorJson);
    
    // Encriptar el mensaje de error
    byte encryptedError[128];
    int len = aesLib.encrypt((byte*)errorString.c_str(), errorString.length(), encryptedError, aesKey, 128, iv);
    return String((char*)encryptedError, len);
  }
}

void sendMessage() {
  String msg = getReadings();
  if (msg.length() > 0) {
    mesh.sendBroadcast(msg);
  }
  taskSendMessage.setInterval(random(TASK_SECOND * 1, TASK_SECOND * 5));
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Nodo DHT: Mensaje recibido de %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> Nodo DHT: Nueva conexión, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("Conexiones cambiadas");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Ajuste de tiempo: %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  mesh.update();
}
