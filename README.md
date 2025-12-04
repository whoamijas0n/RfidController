# RfidController
## Firmware del Sistema de Asistencia RFID con ESP32

Este repositorio aloja el código fuente para un sistema de control de asistencia y acceso basado en IoT utilizando el microcontrolador ESP32. El sistema está diseñado para identificar usuarios mediante etiquetas RFID (protocolo MIFARE), validar credenciales contra una API RESTful remota y proporcionar retroalimentación visual y auditiva inmediata a través de una pantalla OLED, indicadores LED y un zumbador piezoeléctrico.

La arquitectura sigue un modelo Cliente-Servidor donde el ESP32 actúa como un cliente ligero gestionando los periféricos de hardware y la comunicación de red, mientras que la lógica de negocio y la persistencia de datos son administradas por un backend centralizado.

## Arquitectura de Hardware

* Microcontrolador: ESP32 DevKit V1

* Módulo RFID: RC522 (Interfaz SPI)

* Pantalla: OLED 0.91" (Controlador SSD1306, Interfaz I2C)

### Indicadores:

* LED Verde (Acceso Permitido/Conexión Exitosa) 

* LED Rojo (Acceso Denegado/Error)

* Audio: Zumbador (Buzzer) Activo

### Estabilización de Energía: 

* Condensador electrolítico de 100µF (crítico para la estabilidad del RC522)

## Diagrama del Circuito

Esquema de conexión para el circuito.

<p align="center">
  <img src="img/rc522.png" alt="Imagen de el circuito" width="550">
</p>

## Tabla de Conexiones

```Plaintext
+------------------+---------------------------+------------------------+
|    COMPONENTE    |      PIN COMPONENTE       |     PIN ESP32 (GPIO)   |
+------------------+---------------------------+------------------------+
|    RFID RC522    | SDA (SS)                  | GPIO 5                 |
|                  | SCK                       | GPIO 18                |
|                  | MOSI                      | GPIO 23                |
|                  | MISO                      | GPIO 19                |
|                  | RST                       | GPIO 15                |
+------------------+---------------------------+------------------------+
|     OLED I2C     | SDA                       | GPIO 21                |
|                  | SCL                       | GPIO 22                |
+------------------+---------------------------+------------------------+
|   PERIFERICOS    | LED Verde                 | GPIO 2  (Res 220 ohm)  |
|                  | LED Rojo                  | GPIO 4  (Res 220 ohm)  |
|                  | Buzzer                    | GPIO 13                |
+------------------+---------------------------+------------------------+
```

### Importante

Los LEDs requieren resistencias en serie de 220Ω para limitar la corriente. El módulo RC522 debe ser alimentado estrictamente con 3.3V; el uso de 5V puede dañar la lógica del módulo.

## Red y Configuración

### Conectividad

El sistema se conecta a una red WiFi estándar WPA2. La configuración estática se define en las directivas del preprocesador:

```c++
// Credenciales WiFi
const char* ssid = "NOMBRE_RED";     // 
const char* password = "PASSWORD";   // 
```

### Integración Backend

El firmware se comunica vía HTTP. La IP del servidor y el puerto deben configurarse según el entorno de despliegue.

```c++
const char* serverIP = "192.168.1.x";  // IP del Backend 
const int serverPort = 5181;           // Puerto de la API 
```

### Sincronización NTP

La gestión del tiempo se realiza mediante pool.ntp.org con un desplazamiento horario configurado para GMT-6 (El Salvador), garantizando marcas de tiempo precisas en la pantalla de reposo.

## Interfaz API

El firmware consume los siguientes endpoints. Todo el intercambio de datos se realiza en formato JSON.

**1. Verificación de Tarjeta**

GET /api/rfid/verificar/{uid}

* Propósito: Verifica si el UID escaneado está asociado a un empleado activo.

* Respuesta Esperada: Objeto JSON con campo booleano valida.

**2. Registro de Fichaje**

POST /api/fichajes/rfid

* Propósito: Registra una entrada o salida válida.

* Payload:
  
```JSON
{
  "codigoRFID": "UID_HEX_STRING",
  "ip": "DIRECCION_IP_DISPOSITIVO"
}
```

**3. Captura de Desconocidos**

POST /api/Rfid/capture/unknown

* Propósito: Registra tarjetas no enroladas en la base de datos para facilitar su posterior alta administrativa.

**4. Notificaciones de Seguridad (Telegram)**

POST /api/telegramnotifications/fichaje-invalido

* Propósito: Envío de alertas en tiempo real por intentos de acceso no autorizados o errores del sistema.

## Lógica del Sistema y Flujo Operativo

### Inicialización:

* Inicio de comunicación Serial (115200 baudios).

* Inicialización del bus SPI y pantalla OLED.

*  Intento de conexión WiFi con barra de progreso visual.

*  Test de conectividad con servicio de Telegram .

### Estado de Reposo (Idle):

* Muestra hora y fecha actual (Sincronización NTP).

* Iconos de estado para WiFi y Lector RFID en la barra superior.

* Emite un parpadeo/sonido de "latido" sutil cada 30 segundos para indicar actividad .

### Proceso de Autenticación:

* Detección: Al acercar una tarjeta, se interrumpe el bucle de reposo.

* Lectura: Se extrae el UID y se formatea a hexadecimal en mayúsculas.

* Validación: Se envía petición HTTP GET al backend.

    * *Si es Válida: Envía POST para registrar fichaje, activa animación "Permitido" (LED Verde + Tono Ascendente) y notifica a Telegram.*

    * *Si es Inválida: Activa animación "Denegado" (LED Rojo + Tono Error), registra el intento fallido y notifica a Telegram.*

* Manejo de Errores: Timeouts de red o errores de API disparan indicadores de fallo específicos.

## Instalación y Configuración

* Clonar este repositorio.

* Abrir el proyecto en PlatformIO o Arduino IDE.

* Instalar las dependencias requeridas mediante el Gestor de Librerías.

* Actualizar las variables ssid, password, serverIP y serverPort en el archivo fuente principal.

* Compilar y subir a la placa ESP32.

* Verificar la conexión mediante el Monitor Serial.

## Código de Indicadores de Estado

```Plaintext
+-------------+--------------------------------------+------------------------------+
|   ESTADO    |           FEEDBACK VISUAL            |      FEEDBACK AUDITIVO       |
+-------------+--------------------------------------+------------------------------+
| Arrancando  | Icono WiFi Animado + Barra Progreso  | Tonos ascendentes de conexion|
+-------------+--------------------------------------+------------------------------+
| Reposo      | Reloj + Fecha + Iconos Estado        | Pitido sutil cada 30s        |
+-------------+--------------------------------------+------------------------------+
| Procesando  | LEDs Rojo/Verde alternando + Puntos  | Sonido tipo "tic-tac"        |
+-------------+--------------------------------------+------------------------------+
| Exito       | Check Verde en OLED + LED Verde Fijo | Acorde mayor (Do-Mi-Sol)     |
+-------------+--------------------------------------+------------------------------+
| Denegado    | 'X' Roja en OLED + LED Rojo          | Tonos graves descendentes    |
+-------------+--------------------------------------+------------------------------+
| Error Red   | Mensaje "SIN CONEXION"               | Alarma rapida de error       |
+-------------+--------------------------------------+------------------------------+
```

## Licencia y Autor

Este proyecto ha sido creado por **Jason Caballero (whoamijas0n)**.

Distribuido bajo la licencia **GNU General Public License v3.0**.  
