#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <PicoOTA.h>

const char *ssid = "FAMILIA_BARRERO";
const char *password = "hadesdaniela";

const char *firmware_url = "http://192.168.20.21:5000/firmware.bin";
const char *firmware_file = "/firmware.bin";

#define LED_PIN 8

unsigned long lastToggle = 0;
bool ledState = false;

// -------- DESCARGA DEL FIRMWARE (con bool de éxito) ----------
bool downloadFirmware(const char *url, const char *filename)
{
  HTTPClient http;
  Serial.printf("Conectando a %s ...\n", url);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("Error HTTP GET: %d\n", httpCode);
    http.end();
    return false;
  }

  File f = LittleFS.open(filename, "w");
  if (!f)
  {
    Serial.println("No se pudo abrir el archivo para escribir");
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buff[1024];
  size_t total = http.getSize();
  size_t downloaded = 0;

  Serial.printf("Tamaño firmware: %u bytes\n", (unsigned)total);

  while (http.connected() && (downloaded < total || total == (size_t)-1))
  {
    size_t len = stream->available();
    if (len)
    {
      size_t toRead = len;
      if (toRead > sizeof(buff))
        toRead = sizeof(buff);
      size_t c = stream->readBytes(buff, toRead);
      if (c == 0) // timeout de stream
      {
        Serial.println("\nError: Timeout al leer stream");
        f.close();
        http.end();
        return false;
      }
      f.write(buff, c);
      downloaded += c;
      Serial.printf("\rDescargado: %u / %u bytes", (unsigned)downloaded, (unsigned)total);
    }
    delay(1);
  }
  Serial.println("\nDescarga completada.");
  f.close();
  http.end();

  // Validar tamaño mínimo descargado
  if (downloaded == 0)
  {
    Serial.println("Error: archivo descargado vacío.");
    return false;
  }

  return true;
}

// -------- PROCESO OTA -------------
void doOTA()
{
  if (!LittleFS.begin())
  {
    Serial.println("Error iniciando LittleFS");
    return;
  }

  if (!downloadFirmware(firmware_url, firmware_file))
  {
    Serial.println("Descarga falló. No se realizará OTA.");
    LittleFS.end();
    return; // no reinicia
  }

  Serial.println("Programando OTA...");
  picoOTA.begin();
  picoOTA.addFile(firmware_file);
  picoOTA.commit();
  LittleFS.end();
  Serial.println("OTA lista. Reiniciando en 5 segundos...");
  delay(5000);
  rp2040.reboot();
}

// -------- SETUP -------------
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nConectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Listo. Envie 'U' por Serial para iniciar OTA.");
}

// -------- LOOP -------------
void loop()
{
  // Si llega 'U' por serial: hacer OTA
  if (Serial.available())
  {
    char c = Serial.read();
    if (c == 'U')
    {
      Serial.println("\nIniciando OTA...");
      doOTA();
    }
  }

  // Si no, cada 2s toggle LED
  if (millis() - lastToggle >= 5000)
  {
    unsigned long elapsed = millis() - lastToggle; // tiempo transcurrido
    Serial.printf("Haciendo toggle del LED. Pasaron %lu ms\n", elapsed);

    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    lastToggle = millis();
  }
}
