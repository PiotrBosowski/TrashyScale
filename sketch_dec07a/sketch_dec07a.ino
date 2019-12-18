#include <HX711.h>
#include <WiFi.h>

#define TAB_LENGTH 50

float tab[TAB_LENGTH] = {0};
float tabSorted[TAB_LENGTH] = {0};
int currentIndex = 0;
float currentWeight = 0.0;
float totalWeight = 0.0;

bool waitingForBin = false;

#define DOUT 16
#define CLK 17
float calibration_factor = 21790;

HX711 scale;

TaskHandle_t Task1;
TaskHandle_t Task2;

const char* ssid     = "SmietnikowaSiec";
const char* password = "tobylsmiec";

// Set web server port number: 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// LED pins
const int RED_LED = 25;
const int BLUE_LED = 32;
const int GREEN_LED = 33;
// setting PWM properties
const int freq = 5000;
const int blueChannel = 3;
const int redChannel = 1;
const int greenChannel = 2;
const int resolution = 8;
bool isLedActive = true;


// Button
const int BUTTON = 4;



void setup() {
  // configure LED PWM functionalitites
  ledcSetup(blueChannel, freq, resolution);
  ledcSetup(redChannel, freq, resolution);
  ledcSetup(greenChannel, freq, resolution);
    // attach the channel to the GPIO to be controlled
  ledcAttachPin(BLUE_LED, blueChannel);
  ledcAttachPin(RED_LED, redChannel);
  ledcAttachPin(GREEN_LED, greenChannel);
  // weight setup
  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor);
  scale.tare();

  // pins setup
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  pinMode(BLUE_LED, OUTPUT);
  digitalWrite(BLUE_LED, LOW);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);

  // button setup
  pinMode(BUTTON, INPUT);

  // serial setup
  Serial.begin(9600);

  // wifi server setup
  WiFi.softAP(ssid, password);
  server.begin();
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());

  //2threads setup
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
              if (header.indexOf("GET /27/on") >= 0) {
                isLedActive = true;
              } else if (header.indexOf("GET /27/off") >= 0) {
                isLedActive = false;
              }
              
              client.println("<!DOCTYPE html><meta http-equiv=\"refresh\" content=\"4\"><html>");             // Display the HTML web page
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 4px 12px;");
              client.println("text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #555555;}</style></head>");
              client.println("<body><h1>Serwer odpadowy</h1>");
              client.println("<p>Dioda ostrzegawcza jest ");       
              if (!isLedActive) {
                client.println("nieaktywna. <a href=\"/27/on\"><button class=\"button\">AKTYWUJ</button></a></p>");
              } else {
                client.println("aktywna. <a href=\"/27/off\"><button class=\"button button2\">DEZAKTYWUJ</button></a></p>");
              }
              if(waitingForBin){
                client.println("<p>Czekam na pojemnik...</p>");
              } else {
                client.println("<p>Aktualna waga: " + String(currentWeight > 0 ? currentWeight : 0) + " kg.</p>");
              }
              client.println("<p>Suma od ostatniego resetu: " + String(totalWeight) + " kg.</p>");
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


//zapisujemy w buforze ostatnie 25 pomiarow
//jak nagle pojawi sie < 0.3kg to bierzemy mediane z ostatnich 20 i dodajemy do totalWeight
//TODO: ledy swiecace migajace

void blueLedUp(){
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  for(int i = 0; i < 255; i++){
    delay(2);
    ledcWrite(blueChannel, i);
  }
}
void blueLedDown(){
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  for(int i = 255; i --> 0;){
    delay(2);
    ledcWrite(blueChannel, i);
  }
}


void tareScale(){ //tutaj czyscimy tablice poprzednich pomiarow btw
      blueLedUp();
      scale.tare();
      ledcWrite(blueChannel, 0);
      blueLedDown();
}

void refreshLed(){
  if(isLedActive){
    float temp = currentWeight/3.0 * 255;
    int redness = temp > 255 ? 255 : temp < 0 ? 0 : temp;
    ledcWrite(redChannel, redness);
    ledcWrite(greenChannel, 255 - redness);
  } else {
    ledcWrite(redChannel, 0);
    ledcWrite(greenChannel, 0);
  }
}

void quicksort(float * table){quicksort(table,0, TAB_LENGTH);} //wrapper
void quicksort(float * table, int left, int right) //zwykly quicksort ktory zlicza operacje porownan w zmiennej compCounter
{
  int i = left;
  int j = right;
  float x = table[(left + right) / 2]; //piwot na srodku
  do
  {
    while (table[i] < x) i++;
    while (table[j] > x) j--;
    if (i <= j)
    {
      float temp = table[i];
      table[i] = table[j];
      table[j] = temp;
      i++;
      j--;
    }
  } while (i <= j);
  if (left < j)  quicksort(table, left, j);
  if (right > i) quicksort(table, i, right);
}


void trashEmptied(){
  blueLedUp();
  for(int i = 0; i < TAB_LENGTH; i++){
    tabSorted[i] = tab[i];
  }
  blueLedDown();
  quicksort(tabSorted);
  blueLedUp();
  float currWei = tabSorted[TAB_LENGTH/2];
  totalWeight += currWei > 0 ? currWei : 0;
  for(int i = 0; i < TAB_LENGTH; i++){
    tabSorted[i] = 0; 
    tab[i] = 0;
  }
  currentWeight = 0.0;
  blueLedDown();
}

float measureWeight(){
    currentIndex++;
    if(currentIndex >= TAB_LENGTH) currentIndex = 0;
    tab[currentIndex] = scale.get_units();
    return tab[currentIndex];
}

void waitForButton(){
  waitingForBin = true;
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  while(1){
    for(int i = 0; i < 255; i++){
      delay(3);
      ledcWrite(blueChannel, i);
      if(digitalRead(BUTTON)){
        return;
      }
    }
    for(int i = 255; i --> 0;){
      delay(3);
      ledcWrite(blueChannel, i);
      if(digitalRead(BUTTON)){
        return;
      }
    }
  }
}

void blueTripleBlink(){
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  ledcWrite(blueChannel, 0);
  for(int i = 0; i < 3; i++){
  ledcWrite(blueChannel, 0);
  delay(100);
  ledcWrite(blueChannel, 255);
  delay(350);  
  }
  ledcWrite(blueChannel, 0);
  delay(200);
}

void printToSerial(){
  Serial.print("single reading: ");
  Serial.print(currentWeight, 2);
  Serial.print(" kg, total reading: ");
  Serial.print(totalWeight, 2);
  Serial.println(" kg"); 
}

void Task2code( void * pvParameters ){
  while(1)
  {
    if(digitalRead(BUTTON)){
      tareScale();
      if(digitalRead(BUTTON)){
        tareScale();
        if(digitalRead(BUTTON)){
          tareScale();
          if(digitalRead(BUTTON)){
          totalWeight = 0.0;
          blueTripleBlink();
          }
        }
      }
    }
    currentWeight = measureWeight();
    refreshLed();
    if(currentWeight < -0.3){
      trashEmptied();
      waitForButton();
      waitingForBin = false;
      tareScale();
    }
    printToSerial();
  }
}

void loop() {}
