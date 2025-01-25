/*****************************************
  ESP32 GPS VKEL 9600 Bds
This version is for T22_v01 20190612 board
As the power management chipset changed, it
require the axp20x library that can be found
https://github.com/lewisxhe/AXP202X_Library
You must import it as gzip in sketch submenu
in Arduino IDE
This way, it is required to power up the GPS
module, before trying to read it.

Also get TinyGPS++ library from: 
https://github.com/mikalhart/TinyGPSPlus
******************************************/

#include <TinyGPS++.h>
#include <axp20x.h>
#include "esp_sleep.h"
#include "BluetoothSerial.h"  // Biblioteca Bluetooth Serial
#include <SPIFFS.h>          // Include SPIFFS library

TinyGPSPlus gps;
HardwareSerial GPS(1);
AXP20X_Class axp;
BluetoothSerial SerialBT;  // Objeto Bluetooth Serial

// Estructura para almacenar ubicaciones
struct Ubicacion {
    String nombre;
    float latitud;
    float longitud;
};

// Definición de ubicaciones maestras
Ubicacion ubicacionesMaestras[] = {
    {"Comedor", 22.43617, -79.8984},
    {"U9", 22.43730, -79.8977},
    {"Panaderia", 22.43850, -79.8949},
    {"Camilitos", 22.44225, -79.8958},
    {"Puerta", 22.43747, -79.9028},
    {"FIE", 22.43363, -79.9005}
};

bool guardandoDatos = false; // Variable para controlar el estado de guardado

void setup()
{
    Serial.begin(115200);
    SerialBT.begin("GPS_Device"); // Inicializa Bluetooth con un nombre único
    Serial.println("Bluetooth iniciado correctamente");

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Error al montar el sistema de archivos");
        return;
    }

    Wire.begin(21, 22);
    if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
        Serial.println("AXP192 Begin FAIL");
    } else {
        Serial.println("AXP192 Begin PASS");
    }
    
    axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
    axp.setPowerOutPut(AXP192_LDO3, AXP202_ON); // GPS
    axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
    axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
    axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
    
    GPS.begin(9600, SERIAL_8N1, 34, 12); // TX=34, RX=12
}

void loop()
{
    while (GPS.available()) {
        gps.encode(GPS.read()); // Decodifica los datos del GPS

        // Verificar si hay datos disponibles en Bluetooth
        if (SerialBT.available()) {
            String mensaje = SerialBT.readStringUntil('\n'); // Leer el mensaje hasta nueva línea
            
            // Limpiar espacios y convertir a mayúsculas para evitar errores de comparación
            mensaje.trim();
            mensaje.toUpperCase();

            Serial.println("Mensaje recibido: " + mensaje); // Depuración: imprimir el mensaje recibido

            if (mensaje == "SAVE") {
                guardandoDatos = true; // Iniciar el guardado de datos
                SerialBT.println("Iniciando guardado en FFS...");
                Serial.println("Comando SAVE recibido.");
            } else if (mensaje == "STOP") {
                guardandoDatos = false; // Detener el guardado de datos
                SerialBT.println("Deteniendo el guardado en FFS...");
                Serial.println("Comando STOP recibido.");
            } else if (mensaje == "READ") {
                enviarArchivoPorBluetooth(); // Llamar a la función para enviar el archivo
            } else {
                SerialBT.println("Comando no reconocido.");
                Serial.println("Comando no reconocido: " + mensaje);
            }
        }

        // Mostrar y transmitir datos de ubicación en tiempo real
        mostrarYTransmitirDatos();
        
        smartDelay(1000); // Espera un segundo antes de volver a leer
    }
}

static void smartDelay(unsigned long ms)
{
    unsigned long start = millis();
    do
    {
        while (GPS.available())
            gps.encode(GPS.read());
    } while (millis() - start < ms);
}

void mostrarYTransmitirDatos()
{
    // Obtener latitud y longitud actuales
    float latitudActual = gps.location.lat();
    float longitudActual = gps.location.lng();

    // Formatear los datos en una cadena para transmitir por Bluetooth y monitor serie
    String datos = "Latitude  : " + String(latitudActual, 5) + "\n" +
                   "Longitude : " + String(longitudActual, 4) + "\n" +
                   "Satellites: " + String(gps.satellites.value()) + "\n" +
                   "Altitude  : " + String(gps.altitude.feet() / 3.2808) + " M\n" +
                   "Time      : " + String(gps.time.hour()) + ":" +
                                 String(gps.time.minute()) + ":" +
                                 String(gps.time.second()) + "\n" +
                   "Speed     : " + String(gps.speed.kmph()) + " km/h\n" +
                   "**********************";

    // Mostrar en el monitor serie
    Serial.println(datos);

    // Transmitir por Bluetooth si está conectado
    if (SerialBT.connected()) {
        SerialBT.println(datos);
        
        // Guardar datos en FFS si está habilitado el guardado
        if (guardandoDatos) {
            guardarDatosEnFFS(datos); // Guardar datos en FFS y notificar por Bluetooth
        }
    }

   // Verificar si estamos cerca de alguna ubicación maestra y enviar notificación por Bluetooth
   verificarUbicacionCercana(latitudActual, longitudActual);
}

void guardarDatosEnFFS(String datos)
{
   File file = SPIFFS.open("/datos_gps.txt", FILE_APPEND);
   if (!file) {
       Serial.println("Error al abrir el archivo para escribir");
       return;
   }
   if (file.print(datos)) {
       Serial.println("Datos guardados correctamente en FFS");
       if (SerialBT.connected()) {
           SerialBT.println("Datos guardados correctamente en FFS");
       }
   } else {
       Serial.println("Error al guardar los datos");
   }
   file.close();
}

void enviarArchivoPorBluetooth()
{
   File file = SPIFFS.open("/datos_gps.txt", FILE_READ);
   if (!file) {
       SerialBT.println("Error al abrir el archivo para leer");
       return;
   }

   // Indicar inicio de transferencia del archivo
   SerialBT.println("INICIO_ARCHIVO");

   while (file.available()) {
       String line = file.readStringUntil('\n');
       SerialBT.println(line); // Enviar cada línea del archivo por Bluetooth
   }

   // Indicar fin de transferencia del archivo
   SerialBT.println("FIN_ARCHIVO");

   file.close();
}

void verificarUbicacionCercana(float latitudActual, float longitudActual)
{
   for (const Ubicacion& ubicacion : ubicacionesMaestras) {
       float distancia = calcularDistancia(latitudActual, longitudActual, ubicacion.latitud, ubicacion.longitud);
       if (distancia <= 15) { // Rango de 15 metros
           String mensaje = "Se está acercando a: " + ubicacion.nombre;
           SerialBT.println(mensaje); // Enviar mensaje por Bluetooth
           Serial.println(mensaje); // Imprimir en el monitor serie
           break; // Salir del bucle después de encontrar la primera coincidencia
       }
   }
}

// Función para calcular la distancia entre dos puntos geográficos
float calcularDistancia(float lat1, float lon1, float lat2, float lon2)
{
   const float R = 6371000; // Radio de la Tierra en metros
   float dLat = (lat2 - lat1) * M_PI / 180.0;
   float dLon = (lon2 - lon1) * M_PI / 180.0;

   float a = sin(dLat / 2) * sin(dLat / 2) +
             cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
             sin(dLon / 2) * sin(dLon / 2);

   float c = 2 * atan2(sqrt(a), sqrt(1 - a));

   return R * c; // Distancia en metros
}

void enterDeepSleep()
{
   esp_sleep_enable_timer_wakeup(20 * 1000000);
   esp_deep_sleep_start();
}
