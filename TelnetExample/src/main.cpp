#include <WiFi.h>

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

WiFiServer telnetServer(23);
WiFiClient telnetClient;

bool sending = true;
unsigned long lastSend = 0;
int counter = 0;

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  telnetServer.begin();
  telnetServer.setNoDelay(true);
  Serial.println("Telnet server started on port 23");
}

void loop()
{

  if (telnetServer.hasClient())
  {
    if (!telnetClient || !telnetClient.connected())
    {
      telnetClient = telnetServer.available();
      telnetClient.println("Connected to RPIW Telnet\r\nType 'stop' or 'start'\r\n");
      Serial.println("Telnet client connected");
    }
    else
    {

      WiFiClient tempClient = telnetServer.available();
      tempClient.stop();
    }
  }

  if (telnetClient && telnetClient.connected() && telnetClient.available())
  {
    String cmd = telnetClient.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    Serial.print("Received command: ");
    Serial.println(cmd);
    if (cmd == "stop")
    {
      sending = false;
      telnetClient.println("Sending stopped\r\n");
    }
    else if (cmd == "start")
    {
      sending = true;
      telnetClient.println("Sending resumed\r\n");
    }
  }

  if (sending && telnetClient && telnetClient.connected() &&
      millis() - lastSend >= 10000)
  {
    lastSend = millis();
    counter++;
    telnetClient.printf("Counter: %d, test message\r\n", counter);
  }
}
