#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

WiFiClientSecure clientSecure; // cliente TLS

void doGET()
{
  HTTPClient http;
  http.begin(clientSecure, "https://reqres.in/api/users/1");
  http.addHeader("x-api-key", "reqres-free-v1");

  int httpCode = http.GET();
  if (httpCode == 200)
  {
    Serial.println(http.getString());
  }
  else
  {
    Serial.printf("GET failed, code: %d\n", httpCode);
  }
  http.end();
}

void doPOST()
{
  HTTPClient http;
  http.begin(clientSecure, "https://reqres.in/api/users");
  http.addHeader("x-api-key", "reqres-free-v1");
  http.addHeader("Content-Type", "application/json");

  String body = "{\"name\":\"Hades lobo\",\"job\":\"wolf\"}";
  int httpCode = http.POST(body);

  if (httpCode == 201)
  {
    Serial.println(http.getString());
  }
  else
  {
    Serial.printf("POST failed, code: %d\n", httpCode);
  }
  http.end();
}

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  clientSecure.setInsecure();

  Serial.println(" ============================= ");
  Serial.println("    Haciendo petición GET ");
  Serial.println(" ============================ ");
  doGET();
  delay(2000);
  Serial.println("\n ============================= ");
  Serial.println("    Haciendo petición POST ");
  Serial.println(" ============================ ");
  doPOST();
}

void loop() {}
