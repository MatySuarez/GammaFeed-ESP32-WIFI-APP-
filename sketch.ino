#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include "HX711.h"

// Configuración de red WiFi
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// Configuración de Firebase
#define FIREBASE_URL "https://firestore.googleapis.com/v1/projects/gammafeed-93110/databases/(default)/documents"
#define FIREBASE_API_KEY "AIzaSyAgmUh1qk15MQZHv6FgDQ37qcuLpsEhITI"

// Pines y configuración del servo
#define SERVO_PIN 16
Servo servoMotor;

// Pines de la balanza
#define LOADCELL_DOUT_PIN 14
#define LOADCELL_SCK_PIN 12
HX711 scale;

// Variables globales
HTTPClient http;
float targetWeight = 0.0;  // Peso objetivo
float lastWeight = 0.0;    // Último peso registrado
bool motorActive = false;  // Estado del motor
bool comidaProcesada = false; // Estado de comida procesada
const float calibrationFactor = 0.401; // Factor de calibración ajustado
const float tolerance = 1.0;           // Tolerancia para detener el motor
String userUID = "ccdWTOHmkhYmuELD3nfBgPxSZJ03"; // UID del usuario y mascota (idénticos en este caso)
std::vector<String> comidasProcesadas;

void setup() {
  Serial.begin(115200);

  // Configurar WiFi
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\n¡Conexión WiFi establecida!");
  delay(2000);

  // Saludo de bienvenida
  Serial.println("Bienvenido a GammaFeed. Preparando el sistema...");
  delay(2000);

  // Configurar servo y balanza
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(0);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibrationFactor);
  scale.tare();
  Serial.println("Balanza configurada y calibrada.");
  delay(2000);
}

void consultarFirebase() {
  if (comidaProcesada) {
    return;  // Evitar procesar múltiples comidas al mismo tiempo
  }

  // Ruta completa en Firebase
  String url = String(FIREBASE_URL) + "/usuarios/" + userUID + "/mascotas/" + userUID + "/comidas?key=" + String(FIREBASE_API_KEY);
  Serial.print("Buscando en la base de datos");

  // Mostrar animación de puntos durante la búsqueda
  for (int i = 0; i < 30; i++) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();

    if (httpResponseCode == 200) {
      procesarComidas(payload);
    } else {
      Serial.println("Error al consultar datos: Código " + String(httpResponseCode));
    }
  } else {
    Serial.println("Error de conexión con Firebase: Código " + String(httpResponseCode));
  }

  http.end();
}

void procesarComidas(String json) {
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.println("Error al analizar JSON: " + String(error.c_str()));
    return;
  }

  if (!doc.containsKey("documents")) {
    Serial.println("No hay comidas registradas actualmente.");
    return;
  }

  JsonArray documents = doc["documents"].as<JsonArray>();
  bool nuevaComida = false;

  for (JsonObject comida : documents) {
    String cantidadStr = comida["fields"]["cantidad"]["integerValue"];
    float cantidad = cantidadStr.toFloat();
    String documentName = comida["name"].as<String>();

    if (std::find(comidasProcesadas.begin(), comidasProcesadas.end(), documentName) != comidasProcesadas.end()) {
      continue;
    }

    if (cantidad > 0) {
      Serial.println("Comida detectada: " + String(cantidad) + " gramos.");
      targetWeight = cantidad;
      comidasProcesadas.push_back(documentName);
      activarMotor();
      comidaProcesada = true;
      nuevaComida = true;
      break;
    }
  }

  if (!nuevaComida) {
    Serial.println("No hay nuevas comidas para procesar.");
  }
}

void loop() {
  if (!motorActive) {
    consultarFirebase();
  } else {
    controlarMotor();
  }

  delay(2000);  // Mayor tiempo entre iteraciones
}

void activarMotor() {
  motorActive = true;
  Serial.print("Dispensando alimento");

  // Animación de puntos durante la dispensación
  for (int i = 0; i < 30; i++) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
}

void controlarMotor() {
  float currentWeight = scale.get_units();

  if (abs(currentWeight - lastWeight) > tolerance) {
    lastWeight = currentWeight;
    Serial.println("Peso actual: " + String(currentWeight, 2) + " gramos");
  }

  if (currentWeight >= targetWeight - tolerance) {
    Serial.println("Peso alcanzado: " + String(currentWeight, 2) + " gramos");
    detenerMotor();
    return;
  }

  servoMotor.write(90);
  delay(300);  // Más tiempo entre movimientos del servo
  servoMotor.write(0);
  delay(300);
}

void detenerMotor() {
  motorActive = false;
  servoMotor.write(0);
  comidaProcesada = false;
  Serial.println("Motor detenido. Alimento dispensado con éxito.");
  Serial.println("Listo para buscar nuevas comidas.");
}