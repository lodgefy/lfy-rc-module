/*
 RESTduino
 
 A REST-style interface to the Arduino via the 
 Wiznet Ethernet shield.
 
 Based on David A. Mellis's "Web Server" ethernet
 shield example sketch.
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 
 created 04/12/2011
 by Jason J. Gullickson
 
 added 10/16/2011
 by Edward M. Goldberg - Optional Debug flag
 
 */

#define DEBUG true
#define STATICIP true
#define RECEIVER_PIN 0 // Receiver on inerrupt 0 => that is pin #2
#define TRANSMITTER_PIN 7

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetBonjour.h>
#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

#if STATICIP
byte ip[] = {192,168,2,2};
#endif

// s = stopped
// l = learning
char rcMode = 's';
int rcModeTick = 0;
String rcBuffer = String("");

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup() {
#if DEBUG
  //  turn on serial (for debuggin)
  Serial.begin(9600);
#endif

  // start the Ethernet connection and the server:
#if STATICIP
  Ethernet.begin(mac, ip);
#else
  if (Ethernet.begin(mac) == 0) {
#if DEBUG
    Serial.println("Unable to set server IP address using DHCP");
#endif
    for(;;)
      ;
  }
#if DEBUG
  // report the dhcp IP address:
  Serial.println(Ethernet.localIP());
#endif
#endif
  server.begin();
  
  EthernetBonjour.begin("restduino");

  mySwitch.enableReceive(RECEIVER_PIN);
  pinMode(TRANSMITTER_PIN,OUTPUT);
  digitalWrite(TRANSMITTER_PIN,LOW);

}

//  url buffer size
#define BUFSIZE 255

// Toggle case sensitivity
#define CASESENSE true

void loop() {

  if (rcMode == 'l') {
    writeBufferLearning(mySwitch.getReceivedBitlength(), mySwitch.getReceivedRawdata());
    mySwitch.resetAvailable();
  }
  else {

  // needed to continue Bonjour/Zeroconf name registration
  EthernetBonjour.run();
  
  char clientline[BUFSIZE];
  int index = 0;
  // listen for incoming clients
  EthernetClient client = server.available();

  if (client) {

    //  reset input buffer
    index = 0;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        //  fill url the buffer
        if(c != '\n' && c != '\r' && index < BUFSIZE){ // Reads until either an eol character is reached or the buffer is full
          clientline[index++] = c;
          continue;
        }  

#if DEBUG
        Serial.print("client available bytes before flush: "); Serial.println(client.available());
        Serial.print("request = "); Serial.println(clientline);
#endif

        // Flush any remaining bytes from the client buffer
        client.flush();

#if DEBUG
        // Should be 0
        Serial.print("client available bytes after flush: "); Serial.println(client.available());
#endif

        //  convert clientline into a proper
        //  string for further processing
        String urlString = String(clientline);


        //  extract the operation
        String op = urlString.substring(0,urlString.indexOf(' '));

        //  we're only interested in the first part...
        urlString = urlString.substring(urlString.indexOf('/'), urlString.indexOf(' ', urlString.indexOf('/')));

        //  put what's left of the URL back in client line
#if CASESENSE
        urlString.toUpperCase();
#endif
        urlString.toCharArray(clientline, BUFSIZE);

#if DEBUG
        Serial.print("urlString: "); Serial.println(urlString);
#endif

        //  get the first two parameters
        char *action = strtok(clientline,"/");
        char *value = strtok(NULL,"/");
        String strAction = String(action);
        String strValue = String(value);

        //  this is where we actually *do something*!
        char outValue[10] = "MU";
        String jsonOut = String();

        if(action != NULL){
#if DEBUG
        Serial.print("action: "); Serial.println(action);
        Serial.print("value: "); Serial.println(value);
#endif         
          if(value != NULL) {

            if(strAction == String("LEARN")) {
              if(strValue == String("START")) {
                startRcLearning(client);
              }
              else if (strValue == String("STOP")) {
                stopRcLearning(client);
              }
            }
            else if (strAction == String("EXECUTE")) {
#if DEBUG
              Serial.print("execute mode activated");           
#endif    
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
              rcExecute();
            }
            else {
#if DEBUG
              //  set the pin value
              Serial.println("setting pin");
#endif
              //  select the pin
              int selectedPin = atoi (action);
#if DEBUG
              Serial.println(selectedPin);
#endif

              //  set the pin for output
              pinMode(selectedPin, OUTPUT);

              //  determine digital or analog (PWM)
              if(strncmp(value, "HIGH", 4) == 0 || strncmp(value, "LOW", 3) == 0) {

#if DEBUG
                //  digital
                Serial.println("digital");
#endif

                if(strncmp(value, "HIGH", 4) == 0) {
#if DEBUG
                  Serial.println("HIGH");
#endif
                  digitalWrite(selectedPin, HIGH);
                }

                if(strncmp(value, "LOW", 3) == 0) {
#if DEBUG
                  Serial.println("LOW");
#endif
                  digitalWrite(selectedPin, LOW);
                }

              } 
              else {

#if DEBUG
                //  analog
                Serial.println("analog");
#endif
                //  get numeric value
                int selectedValue = atoi(value);              
#if DEBUG
                Serial.println(selectedValue);
#endif
                analogWrite(selectedPin, selectedValue);

              }

              
              showSuccess(client, jsonOut);

            }

          } 
          else {
#if DEBUG
            //  read the pin value
            Serial.println("reading pin");
#endif

            char* pin = action;
            //  determine analog or digital
            if(pin[0] == 'a' || pin[0] == 'A'){

              //  analog
              int selectedPin = pin[1] - '0';

#if DEBUG
              Serial.println(selectedPin);
              Serial.println("analog");
#endif

              sprintf(outValue,"%d",analogRead(selectedPin));

#if DEBUG
              Serial.println(outValue);
#endif

            } 
            else if(pin[0] != NULL) {

              //  digital
              int selectedPin = pin[0] - '0';

#if DEBUG
              Serial.println(selectedPin);
              Serial.println("digital");
#endif

              pinMode(selectedPin, INPUT);

              int inValue = digitalRead(selectedPin);

              if(inValue == 0){
                sprintf(outValue,"%s","LOW");
                //sprintf(outValue,"%d",digitalRead(selectedPin));
              }

              if(inValue == 1){
                sprintf(outValue,"%s","HIGH");
              }

#if DEBUG
              Serial.println(outValue);
#endif
            }

            //  assemble the json output
            jsonOut += "{\"";
            jsonOut += pin;
            jsonOut += "\":\"";
            jsonOut += outValue;
            jsonOut += "\"}";

            showSuccess(client, jsonOut);
          }
        } 
        else {

          //  error
          showError(client);

        }
        break;
      }
    }
  closeConnection(client);
    
  }

  }
}

void startRcLearning(EthernetClient client) {
#if DEBUG
  Serial.print("learn mode activated"); 
#endif
  rcMode = 'l';
  rcModeTick = 1000;
  rcBuffer = String("");

  String jsonOut = "";
  jsonOut += "{\"msg\":\"started Successful\"}";

  showSuccess(client, jsonOut);
}

void stopRcLearning(EthernetClient client) {
#if DEBUG
  Serial.print("learn mode deactivated"); 
#endif
  rcMode = 's';
  rcModeTick = 0;

  String jsonOut = "";
  jsonOut += "{\"code\":\"";
  jsonOut += rcBuffer;
  jsonOut += "\"}";

  showSuccess(client, jsonOut);
}

void writeBufferLearning(unsigned int length, unsigned int* raw) {
  for (int i=0; i<= length*2; i++) {
    rcModeTick--;
    if (rcModeTick < 0) {
      rcMode = 's';
    }
    //rcBuffer += raw[i];
    //cBuffer += ",";

#if DEBUG
    Serial.print(raw[i]);
    Serial.print(",");
#endif
  }
}


void showError(EthernetClient client) {

#if DEBUG
  Serial.println("erroring");
#endif
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println();

  closeConnection(client);
}

void showSuccess(EthernetClient client, String json) {

#if DEBUG
  Serial.println("success");
#endif
//  return value with wildcarded Cross-origin policy
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.println(json);

  closeConnection(client);
}

void closeConnection(EthernetClient client) {

// give the web browser time to receive the data
  delay(1);

  // close the connection:
  //client.stop();
  client.stop();
  while(client.status() != 0){
    delay(5);
  }

}

//let’s build our instruments:

void customDelay(unsigned long time) {
    unsigned long end_time = micros() + time;
    while(micros() < end_time);
}

//1000=1ms

void setStateWithDelay(int pin, int state,int delayTime) {
  if(state==1)
    digitalWrite(pin,HIGH);
  else
    digitalWrite(pin,LOW);
    
  customDelay(delayTime);
}

void rcExecute() {


setStateWithDelay(TRANSMITTER_PIN,0,976);


  digitalWrite(TRANSMITTER_PIN,LOW);
}
