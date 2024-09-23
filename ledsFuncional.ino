#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <AESLib.h>

#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555

// Definimos los pines para los LEDs
#define yellowLED 25   // LED amarillo
#define whiteLED  26   // LED blanco
#define greenLED  27   // LED verde

Scheduler userScheduler;  // Para manejar las tareas del usuario
painlessMesh mesh;

// Clave de encriptación y vector de inicialización
byte aesKey[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}; // Debe ser la misma que en el DHT
byte iv[16] = {0}; // Vector de inicialización
AESLib aesLib; // Crea una instancia de AESLib

// Variables para el control del parpadeo
bool ledState = false;
unsigned long previousMillis = 0;
const long interval = 250; // Intervalo de parpadeo en milisegundos
int currentError = -1; // Para guardar el error actual
bool errorActive = false; // Indica si hay un error activo

// Prototipos
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();
void nodeTimeAdjustedCallback(int32_t offset);
void handleError(int errorCode);
void handleTemperature(float temp);

// Función que se ejecuta cuando se recibe un mensaje
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Nodo LED: Mensaje recibido de %u msg=%s\n", from, msg.c_str());

  // Comprobar si es un mensaje de error
  if (msg.startsWith("Error:")) {
    int errorCode = msg.substring(6).toInt();
    handleError(errorCode);
    return;
  }

  // Si hay un error activo, ignorar la lectura de temperatura y humedad
  if (errorActive) {
    Serial.println("Error activo, ignorando lectura de temperatura y humedad.");
    return;
  }

  // Intentar parsear el mensaje JSON
  byte decrypted[128];
  int len = aesLib.decrypt((byte*)msg.c_str(), msg.length(), decrypted, aesKey, 128, iv);
  String decryptedString((char*)decrypted, len);
  
  JSONVar jsonData = JSON.parse(decryptedString);
  if (JSON.typeof(jsonData) == "undefined") {
    Serial.println("No es un JSON válido");
    return;
  }

  // Extraer la temperatura y manejarla
  float temp = (double)jsonData["temp"];
  int hum = (int)jsonData["hum"];
  Serial.printf("Temperatura: %.2f, Humedad: %d\n", temp, hum);
  
  // Manejar el estado del LED según la temperatura
  handleTemperature(temp);
}

// Función que maneja los errores y activa LEDs según el código de error
void handleError(int errorCode) {
  Serial.printf("Error detectado: %d\n", errorCode);
  
  // Guardar el error actual
  currentError = errorCode;
  errorActive = true; // Activar estado de error

  // Reiniciar el estado del LED
  digitalWrite(yellowLED, LOW);
  digitalWrite(whiteLED, LOW);
  digitalWrite(greenLED, LOW);
  
  // Control de LEDs según el tipo de error
  switch (errorCode) {
    case 1:
      digitalWrite(yellowLED, HIGH); // Error de lectura
      break;
    case 2:
      digitalWrite(whiteLED, HIGH); // Datos atípicos
      break;
    default:
      digitalWrite(greenLED, HIGH); // Sin errores
      break;
  }
}

// Función que maneja la temperatura y activa los LEDs correspondientes
void handleTemperature(float temp) {
  if (errorActive) {
    // Si hay un error activo, no manejar la temperatura
    return;
  }

  if (temp < 17) {
    digitalWrite(yellowLED, HIGH); // Encender LED amarillo
    digitalWrite(whiteLED, LOW);
    digitalWrite(greenLED, LOW);
  } else if (temp >= 17 && temp <= 25) {
    digitalWrite(yellowLED, LOW);
    digitalWrite(whiteLED, HIGH); // Encender LED blanco
    digitalWrite(greenLED, LOW);
  } else if (temp > 25) {
    digitalWrite(yellowLED, LOW);
    digitalWrite(whiteLED, LOW);
    digitalWrite(greenLED, HIGH); // Encender LED verde
  }
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> Nodo LED: Nueva conexión, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("Cambios en las conexiones");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Tiempo ajustado. Offset = %d\n", offset);
}

void setup() {
  Serial.begin(115200);

  // Configurar los pines de los LEDs como salida
  pinMode(yellowLED, OUTPUT);
  pinMode(whiteLED, OUTPUT);
  pinMode(greenLED, OUTPUT);

  // Apagar todos los LEDs al inicio
  digitalWrite(yellowLED, LOW);
  digitalWrite(whiteLED, LOW);
  digitalWrite(greenLED, LOW);

  // Inicializar la red mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

void loop() {
  // Actualizar la malla
  mesh.update();

  // Parpadeo del LED en función del error actual
  if (currentError != -1) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      ledState = !ledState; // Cambiar el estado del LED

      // Encender o apagar el LED según el error
      if (currentError == 1) {
        digitalWrite(yellowLED, ledState); // Titilar LED amarillo
      } else if (currentError == 2) {
        digitalWrite(whiteLED, ledState); // Titilar LED blanco
      }
    }
  } else {
    // Si no hay error, restablecer el estado de error
    errorActive = false; 
  }
}
