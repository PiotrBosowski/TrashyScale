#include <WiFi.h>
#include "HX711.h"

#define DOUT 16
#define CLK 17

HX711 scale;

TaskHandle_t Task1;
TaskHandle_t Task2;

// Replace with your network credentials
const char* ssid     = "SmietnikowaSiec";
const char* password = "tobylsmiec";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String ledState = "off";

// LED pins
const int blinkingLed = 4;
const int drivenLed = 27;

float weight = 0.0;

void setup() {
  scale.begin(DOUT, CLK);
  scale.set_scale();
  scale.tare(); //Reset the scale to 0

  long zero_factor = scale.read_average();
  pinMode(drivenLed, OUTPUT);
  digitalWrite(drivenLed, LOW);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();  
  server.begin();
  pinMode(blinkingLed, OUTPUT);

  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    50000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    tskNO_AFFINITY);          /* pin task to core 0 */ 
  xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    1000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    tskNO_AFFINITY);          /* pin task to core 1 */

  Serial.begin(9600);
  
  disableCore0WDT();
  disableCore1WDT();
}

void Task1code( void * pvParameters )
{
  while(1)
  {
    WiFiClient client = server.available();   // Listen for incoming clients
    if (client)                               // If a new client connects,]
    {                             
      String currentLine = "";                // make a String to hold incoming data from the client
      while (client.connected())              // loop while the client's connected
      {
        if (client.available())               // if there's bytes to read from the client,
        {             
          char c = client.read();             // read a byte, then
          Serial.write(c);                    // print it out the serial monitor
          header += c;
          if (c == '\n')                      // if the byte is a newline character
          {                                   // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0)
            {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();             
              if (header.indexOf("GET /27/on") >= 0) { // turns the GPIOs on and off
                ledState = "on";
                digitalWrite(drivenLed, HIGH);
              } else if (header.indexOf("GET /27/off") >= 0) {
                ledState = "off";
                digitalWrite(drivenLed, LOW);
              }
              
              client.println("<!DOCTYPE html><meta http-equiv=\"refresh\" content=\"5\"><html>");             // Display the HTML web page
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #555555;}</style></head>");
              client.println("<body><h1>ESP32 Web Server</h1>");
              client.println("<p>Led is " + ledState + "</p>");       
              if (ledState=="off") {
                client.println("<p><a href=\"/27/on\"><button class=\"button\">SMIEC</button></a></p>");
              } else {
                client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
              }
              client.println("<p>Smieci waza " + String(weight) + " kg.</p>");
              client.println("</body></html>");
              
              // The HTTP response ends with another blank line
              client.println();
              // Break out of the while loop
              break;
            }
            else { // if you got a newline, then clear currentLine
              currentLine = "";
            }
          }
          else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
      }
      // Clear the header variable
      header = "";
      // Close the connection
      client.stop();
    }
  } 
}


float calibration_factor = -21790;
void Task2code( void * pvParameters ){
  while(1)
  {
    scale.set_scale(calibration_factor);
    weight = scale.get_units();
    Serial.print(weight, 1);
    Serial.println(" kg");
    delay(10);
  }
}

void loop() {
  
}
