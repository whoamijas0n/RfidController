#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// Definición de pines según tu diagrama
#define RST_PIN 15
#define SS_PIN 5
#define LED_VERDE 2
#define LED_ROJO 4
#define BUZZER 13

// Configuración OLED
#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Credenciales WiFi
const char* ssid = "XXXXX";
const char* password = "XXXXXX";
const char* serverIP = "192.168.1.XXX";  
const int serverPort = 5181;

// Configuración NTP para hora
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600;  // GMT-6 para El Salvador
const int daylightOffset_sec = 0;

// Objetos
MFRC522 mfrc522(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variables de tiempo
unsigned long ultimaActualizacion = 0;
bool animacionActiva = false;

// ==================== DECLARACIONES DE FUNCIONES ====================

// Funciones WiFi
void conectarWiFi();
void mostrarConectandoWiFi();
void mostrarWiFiConectado();
void mostrarErrorWiFi();

// Funciones OLED mejoradas
void mostrarPantallaReloj();
void mostrarLeyendoTarjeta();
void mostrarProcesando();
void mostrarAccesoPermitido();
void mostrarAccesoDenegado();
void mostrarErrorFichaje();
void dibujarIconoWiFi(int x, int y, bool conectado);
void dibujarIconoRFID(int x, int y);
void dibujarCheck(int x, int y);
void dibujarX(int x, int y);
void animacionCargando(int ciclos);

// Funciones sonido mejoradas
void sonidoAceptacion();
void sonidoDenegado();
void sonidoError();
void sonidoConexionWiFi();
void sonidoEsperaRFID();

// Funciones RFID
String leerUID();
bool verificarTarjeta(String uid);
bool registrarFichaje(String uid);
void enviarACaptura(String uid);
void procesarTarjeta(String uid);

// Funciones Telegram
bool notificarTelegram(String uid, String tipo, String nombreEmpleado = "");
bool testConexionTelegram();

void setup() {
  Serial.begin(115200);
  
  // Configurar pines
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  
  // Inicializar SPI para RFID
  SPI.begin();
  mfrc522.PCD_Init();
  
  // Inicializar OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error al inicializar OLED");
    for(;;);
  }
  
  Serial.println("OLED inicializado correctamente");
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  
  // Conectar WiFi con estética mejorada
  conectarWiFi();
  
  // Configurar servidor NTP para hora
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);
  
  // Test inicial de Telegram
  Serial.println("Realizando test inicial de Telegram...");
  if (testConexionTelegram()) {
    Serial.println("Test Telegram exitoso");
  } else {
    Serial.println("Test Telegram falló");
  }
  
  // Mostrar pantalla de reloj inicial
  mostrarPantallaReloj();
}

void loop() {
  // Actualizar pantalla de reloj cada 5 segundos
  if (millis() - ultimaActualizacion > 5000 && !animacionActiva) {
    mostrarPantallaReloj();
    ultimaActualizacion = millis();
  }
  
  // Parpadeo sutil LED verde cada 10 segundos (sistema activo)
  static unsigned long ultimoParpadeo = 0;
  if (millis() - ultimoParpadeo > 10000) {
    digitalWrite(LED_VERDE, HIGH);
    delay(50);
    digitalWrite(LED_VERDE, LOW);
    ultimoParpadeo = millis();
  }
  
  // Sonido sutil cada 30 segundos para indicar que está activo
  static unsigned long ultimoSonido = 0;
  if (millis() - ultimoSonido > 30000) {
    sonidoEsperaRFID();
    ultimoSonido = millis();
  }
  
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;
  
  animacionActiva = true;
  String uid = leerUID();
  Serial.println("Tarjeta detectada: " + uid);
  
  mostrarLeyendoTarjeta();
  procesarTarjeta(uid);
  
  mfrc522.PICC_HaltA();
  animacionActiva = false;
  delay(500);  // Reducido de 1000ms a 500ms
  mostrarPantallaReloj();
}

// ==================== FUNCIONES PANTALLA OLED MEJORADAS ====================

void mostrarPantallaReloj() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    // Si no hay hora, mostrar pantalla de inicio simple
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 15);
    display.println("SISTEMA");
    display.setCursor(20, 35);
    display.println("FICHAJE");
    display.setTextSize(1);
    display.setCursor(20, 52);
    display.println("Acercar tarjeta");
    display.display();
    return;
  }
  
  display.clearDisplay();
  
  // Dibujar borde decorativo superior
  display.drawLine(0, 8, 127, 8, SSD1306_WHITE);
  
  // Icono RFID en esquina superior izquierda
  dibujarIconoRFID(2, 0);
  
  // Icono WiFi en esquina superior derecha
  dibujarIconoWiFi(108, 0, WiFi.status() == WL_CONNECTED);
  
  // Hora grande y centrada
  display.setTextSize(3);
  char horaStr[6];
  strftime(horaStr, 6, "%H:%M", &timeinfo);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(horaStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 18);
  display.print(horaStr);
  
  // Fecha pequeña centrada
  display.setTextSize(1);
  char fechaStr[20];
  strftime(fechaStr, 20, "%d/%m/%Y", &timeinfo);
  display.getTextBounds(fechaStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 45);
  display.print(fechaStr);
  
  // Indicador parpadeante
  if ((millis() / 500) % 2 == 0) {
    display.fillCircle(64, 58, 2, SSD1306_WHITE);
  }
  
  // Texto inferior
  display.setTextSize(1);
  display.setCursor(15, 55);
  display.print("Acercar tarjeta");
  
  display.display();
}

void mostrarLeyendoTarjeta() {
  display.clearDisplay();
  
  // Parpadeo rápido ambos LEDs (tarjeta detectada)
  for(int i = 0; i < 2; i++) {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_ROJO, HIGH);
    delay(80);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, LOW);
    delay(80);
  }
  
  // Icono RFID grande centrado
  dibujarIconoRFID(54, 10);
  
  // Texto
  display.setTextSize(2);
  display.setCursor(25, 30);
  display.println("LEYENDO");
  
  // Animación de carga con LEDs alternados
  for(int c = 0; c < 3; c++) {
    // Alternar LEDs durante la lectura
    if(c % 2 == 0) {
      digitalWrite(LED_VERDE, HIGH);
    } else {
      digitalWrite(LED_ROJO, HIGH);
    }
    
    for(int i = 0; i < 8; i++) {
      int x = 44 + (i * 5);
      if(i <= c * 3) {
        display.fillRect(x, 50, 3, 8, SSD1306_WHITE);
      }
    }
    display.display();
    delay(150);
    
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, LOW);
  }
}

void mostrarProcesando() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(5, 15);
  display.println("PROCESANDO");
  
  // Animación de puntos con LEDs alternados
  for(int i = 0; i < 4; i++) {
    // Alternar LEDs durante procesamiento
    if(i % 2 == 0) {
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_ROJO, LOW);
    } else {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_ROJO, HIGH);
    }
    
    display.fillRect(30, 40, 70, 10, SSD1306_BLACK);
    display.setTextSize(2);
    display.setCursor(40, 40);
    for(int j = 0; j <= i; j++) {
      display.print(".");
    }
    display.display();
    tone(BUZZER, 1500 + (i * 100), 80);
    delay(350);
  }
  
  // Apagar LEDs
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);
}

void mostrarAccesoPermitido() {
  display.clearDisplay();
  
  // Check grande centrado
  dibujarCheck(44, 5);
  
  // Texto
  display.setTextSize(2);
  display.setCursor(15, 35);
  display.println("PERMITIDO");
  
  // Borde verde (simulado con líneas)
  display.drawRect(5, 5, 118, 54, SSD1306_WHITE);
  display.drawRect(6, 6, 116, 52, SSD1306_WHITE);
  
  display.display();
  
  // Efecto LED verde pulsante más dramático
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_VERDE, HIGH);
    delay(200);
    digitalWrite(LED_VERDE, LOW);
    delay(100);
  }
  
  // LED verde fijo durante 800ms
  digitalWrite(LED_VERDE, HIGH);
  delay(800);
  digitalWrite(LED_VERDE, LOW);
}

void mostrarAccesoDenegado() {
  display.clearDisplay();
  
  // X grande centrada
  dibujarX(44, 5);
  
  // Texto
  display.setTextSize(2);
  display.setCursor(20, 35);
  display.println("DENEGADO");
  
  // Efecto de parpadeo pantalla + LED rojo
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_ROJO, HIGH);
    display.drawRect(5, 5, 118, 54, SSD1306_WHITE);
    display.drawRect(6, 6, 116, 52, SSD1306_WHITE);
    display.display();
    delay(180);
    
    digitalWrite(LED_ROJO, LOW);
    display.fillRect(5, 5, 118, 54, SSD1306_BLACK);
    display.setTextSize(2);
    display.setCursor(20, 35);
    display.println("DENEGADO");
    dibujarX(44, 5);
    display.display();
    delay(180);
  }
  
  // LED rojo fijo durante 600ms
  digitalWrite(LED_ROJO, HIGH);
  display.drawRect(5, 5, 118, 54, SSD1306_WHITE);
  display.drawRect(6, 6, 116, 52, SSD1306_WHITE);
  display.display();
  delay(600);
  digitalWrite(LED_ROJO, LOW);
}

void mostrarErrorFichaje() {
  display.clearDisplay();
  
  // Símbolo de advertencia
  display.fillTriangle(64, 10, 50, 35, 78, 35, SSD1306_WHITE);
  display.fillTriangle(64, 15, 55, 32, 73, 32, SSD1306_BLACK);
  display.fillCircle(64, 26, 2, SSD1306_WHITE);
  display.fillRect(62, 18, 4, 6, SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setCursor(20, 42);
  display.println("ERROR FICHAJE");
  display.setCursor(15, 54);
  display.println("Intente de nuevo");
  
  display.display();
  
  // Parpadeo alternado rápido de ambos LEDs
  for(int i = 0; i < 5; i++) {
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    delay(120);
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    delay(120);
  }
  
  // LED rojo fijo por 700ms
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, HIGH);
  delay(700);
  digitalWrite(LED_ROJO, LOW);
}

void mostrarConectandoWiFi() {
  display.clearDisplay();
  
  // Icono WiFi desconectado animado
  for(int i = 0; i < 10; i++) {
    display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
    
    // Alternar LEDs durante conexión
    if(i % 2 == 0) {
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_ROJO, LOW);
    } else {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_ROJO, HIGH);
    }
    
    // WiFi animado
    int offset = (i % 3) * 15;
    dibujarIconoWiFi(54 + offset - 15, 5, false);
    
    display.setTextSize(2);
    display.setCursor(10, 30);
    display.println("CONECTANDO");
    
    // Barra de progreso
    int progreso = (i * 12);
    display.drawRect(14, 50, 100, 8, SSD1306_WHITE);
    display.fillRect(16, 52, progreso, 4, SSD1306_WHITE);
    
    display.display();
    
    tone(BUZZER, 2000 + (i * 50), 50);
    delay(200);
  }
  
  // Apagar LEDs al terminar animación
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);
}

void mostrarWiFiConectado() {
  display.clearDisplay();
  
  // Check grande
  dibujarCheck(44, 5);
  
  display.setTextSize(2);
  display.setCursor(25, 32);
  display.println("CONECTADO");
  
  display.setTextSize(1);
  String ip = WiFi.localIP().toString();
  display.setCursor((128 - (ip.length() * 6)) / 2, 52);
  display.print(ip);
  
  display.display();
  
  // Animación LED verde de éxito
  for(int i = 0; i < 4; i++) {
    digitalWrite(LED_VERDE, HIGH);
    delay(150);
    digitalWrite(LED_VERDE, LOW);
    delay(150);
  }
  
  sonidoConexionWiFi();
  
  // LED verde fijo por 1 segundo
  digitalWrite(LED_VERDE, HIGH);
  delay(1000);
  digitalWrite(LED_VERDE, LOW);
}

void mostrarErrorWiFi() {
  display.clearDisplay();
  
  // X grande
  dibujarX(44, 5);
  
  display.setTextSize(2);
  display.setCursor(5, 32);
  display.println("SIN CONEXION");
  
  display.setTextSize(1);
  display.setCursor(20, 52);
  display.println("Reiniciando...");
  
  display.display();
  
  // Parpadeo rápido LED rojo de error
  for(int i = 0; i < 6; i++) {
    digitalWrite(LED_ROJO, HIGH);
    tone(BUZZER, 400 - (i * 30), 150);
    delay(200);
    digitalWrite(LED_ROJO, LOW);
    delay(200);
  }
}

// ==================== FUNCIONES DE DIBUJO ====================

void dibujarIconoWiFi(int x, int y, bool conectado) {
  if(conectado) {
    display.fillCircle(x + 10, y + 7, 1, SSD1306_WHITE);
    display.drawCircle(x + 10, y + 7, 3, SSD1306_WHITE);
    display.drawCircle(x + 10, y + 7, 5, SSD1306_WHITE);
    display.drawCircle(x + 10, y + 7, 7, SSD1306_WHITE);
    display.fillRect(x + 10, y + 7, 1, 1, SSD1306_BLACK);
  } else {
    display.drawCircle(x + 10, y + 7, 3, SSD1306_WHITE);
    display.drawCircle(x + 10, y + 7, 5, SSD1306_WHITE);
    display.drawLine(x + 4, y + 1, x + 16, y + 13, SSD1306_WHITE);
  }
}

void dibujarIconoRFID(int x, int y) {
  display.drawRect(x, y, 20, 7, SSD1306_WHITE);
  display.fillRect(x + 2, y + 2, 3, 3, SSD1306_WHITE);
  display.drawLine(x + 8, y + 2, x + 10, y + 2, SSD1306_WHITE);
  display.drawLine(x + 8, y + 4, x + 12, y + 4, SSD1306_WHITE);
  display.drawCircle(x + 15, y + 3, 2, SSD1306_WHITE);
}

void dibujarCheck(int x, int y) {
  // Check mark grande
  display.drawLine(x, y + 15, x + 10, y + 25, SSD1306_WHITE);
  display.drawLine(x + 1, y + 15, x + 11, y + 25, SSD1306_WHITE);
  display.drawLine(x + 2, y + 15, x + 12, y + 25, SSD1306_WHITE);
  
  display.drawLine(x + 10, y + 25, x + 35, y, SSD1306_WHITE);
  display.drawLine(x + 11, y + 25, x + 36, y, SSD1306_WHITE);
  display.drawLine(x + 12, y + 25, x + 37, y, SSD1306_WHITE);
}

void dibujarX(int x, int y) {
  // X grande
  display.drawLine(x, y, x + 35, y + 25, SSD1306_WHITE);
  display.drawLine(x + 1, y, x + 36, y + 25, SSD1306_WHITE);
  display.drawLine(x + 2, y, x + 37, y + 25, SSD1306_WHITE);
  
  display.drawLine(x + 35, y, x, y + 25, SSD1306_WHITE);
  display.drawLine(x + 36, y, x + 1, y + 25, SSD1306_WHITE);
  display.drawLine(x + 37, y, x + 2, y + 25, SSD1306_WHITE);
}

void animacionCargando(int ciclos) {
  for(int c = 0; c < ciclos; c++) {
    for(int i = 0; i < 8; i++) {
      int x = 44 + (i * 5);
      if(i <= c * 3) {
        display.fillRect(x, 50, 3, 8, SSD1306_WHITE);
      }
    }
    display.display();
    delay(150);
  }
}

// ==================== FUNCIONES SONIDO MEJORADAS ====================

void sonidoAceptacion() {
  // Melodía de éxito (tres tonos ascendentes armoniosos)
  int melodia[] = {1047, 1319, 1568}; // Do, Mi, Sol
  for(int i = 0; i < 3; i++) {
    tone(BUZZER, melodia[i], 150);
    delay(180);
  }
  noTone(BUZZER);
}

void sonidoDenegado() {
  // Sonido de error (dos tonos descendentes)
  tone(BUZZER, 800, 250);
  delay(300);
  tone(BUZZER, 400, 400);
  delay(450);
  noTone(BUZZER);
}

void sonidoError() {
  // Sonido de alerta (pulsante)
  for(int i = 0; i < 3; i++) {
    tone(BUZZER, 300, 200);
    delay(250);
  }
  noTone(BUZZER);
}

void sonidoConexionWiFi() {
  // Melodía de conexión exitosa
  tone(BUZZER, 1000, 100);
  delay(120);
  tone(BUZZER, 1500, 100);
  delay(120);
  tone(BUZZER, 2000, 200);
  delay(220);
  noTone(BUZZER);
}

void sonidoEsperaRFID() {
  // Sonido muy sutil de "estoy activo"
  tone(BUZZER, 3000, 30);
  delay(50);
  noTone(BUZZER);
}

// ==================== FUNCIONES TELEGRAM ====================

bool notificarTelegram(String uid, String tipo, String nombreEmpleado) {
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/api/telegramnotifications/fichaje-invalido";
  
  Serial.println("ENVIANDO NOTIFICACION TELEGRAM DIRECTA");
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-RFID-Reader");
  http.setTimeout(15000);
  
  String ipReal = WiFi.localIP().toString();
  String json = "";
  
  if (nombreEmpleado != "") {
    json = "{\"codigoRFID\":\"" + uid + "\",\"ip\":\"" + ipReal + "\",\"tipo\":\"" + tipo + "\",\"nombreEmpleado\":\"" + nombreEmpleado + "\"}";
  } else {
    json = "{\"codigoRFID\":\"" + uid + "\",\"ip\":\"" + ipReal + "\",\"tipo\":\"" + tipo + "\"}";
  }
  
  int httpCode = http.POST(json);
  http.end();
  return (httpCode == 200);
}

bool testConexionTelegram() {
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/api/telegramnotifications/test";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-RFID-Reader");
  http.setTimeout(10000);
  
  String ipReal = WiFi.localIP().toString();
  String json = "{\"mensaje\":\"Test desde ESP32 físico\",\"ip\":\"" + ipReal + "\"}";
  
  int httpCode = http.POST(json);
  http.end();
  return (httpCode == 200);
}

// ==================== FUNCIONES RFID ====================

String leerUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void procesarTarjeta(String uid) {
  // Feedback visual y sonoro de detección más dramático
  for(int i = 0; i < 2; i++) {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_ROJO, HIGH);
    tone(BUZZER, 2500, 100);
    delay(120);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, LOW);
    delay(120);
  }
  
  bool tarjetaValida = verificarTarjeta(uid);
  
  if (tarjetaValida) {
    Serial.println("Tarjeta valida, registrando fichaje...");
    mostrarProcesando();
    
    if (registrarFichaje(uid)) {
      Serial.println("Fichaje registrado exitosamente!");
      notificarTelegram(uid, "VALIDO", "Empleado Verificado");
      
      mostrarAccesoPermitido();
      sonidoAceptacion();
      
      delay(1200);  // Reducido de 2000ms a 1200ms
    } else {
      Serial.println("Error al registrar fichaje");
      notificarTelegram(uid, "ERROR", "");
      
      mostrarErrorFichaje();
      sonidoError();
      
      delay(1500);  // Reducido de 2000ms a 1500ms
    }
  } else {
    Serial.println("Tarjeta no valida o no registrada");
    notificarTelegram(uid, "INVALIDO", "");
    
    mostrarAccesoDenegado();
    sonidoDenegado();
    enviarACaptura(uid);
    
    delay(1200);  // Reducido de 2000ms a 1200ms
  }
}

void enviarACaptura(String uid) {
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/api/Rfid/capture/unknown";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-RFID-Reader");
  http.setTimeout(8000);
  
  String json = "{\"codigoRfid\":\"" + uid + "\"}";
  http.POST(json);
  http.end();
}

bool verificarTarjeta(String uid) {
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/api/rfid/verificar/" + uid;
  
  http.begin(url);
  http.addHeader("User-Agent", "ESP32-RFID-Reader");
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String respuesta = http.getString();
    
    if (respuesta.indexOf("\"valida\":true") >= 0) {
      http.end();
      return true;
    } else if (respuesta.indexOf("\"valida\":false") >= 0) {
      http.end();
      return false;
    }
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, respuesta);
    
    if (!error && doc.containsKey("valida")) {
      bool valida = doc["valida"];
      http.end();
      return valida;
    }
  }
  
  http.end();
  return false;
}

bool registrarFichaje(String uid) {
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/api/fichajes/rfid";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-RFID-Reader");
  http.setTimeout(10000);
  
  String ipReal = WiFi.localIP().toString();
  String json = "{\"codigoRFID\":\"" + uid + "\",\"ip\":\"" + ipReal + "\"}";
  
  int httpCode = http.POST(json);
  http.end();
  
  return (httpCode == 200);
}

// ==================== FUNCIONES WIFI ====================

void conectarWiFi() {
  mostrarConectandoWiFi();
  
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 30) {
    delay(500);
    intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    mostrarWiFiConectado();
  } else {
    Serial.println("Error WiFi!");
    mostrarErrorWiFi();
  }
}
