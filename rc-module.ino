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

#define DEBUG false
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

EthernetClient client;

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
    Serial.println(F("Unable to set server IP address using DHCP"));
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
    EthernetBonjour.run();
    
    char clientline[BUFSIZE];
    int index = 0;
    // listen for incoming clients
    client = server.available();

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

          client.flush();

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
          Serial.print(F("urlString: ")); Serial.println(urlString);
  #endif
          char *action = strtok(clientline,"/");
          char *value = strtok(NULL,"/");
          char outValue[10] = "MU";
          String jsonOut = String();
          if(action != NULL){
  #if DEBUG
          Serial.print(F("action: ")); Serial.println(action);
          Serial.print(F("value: ")); Serial.println(value);
  #endif         
            if(value != NULL) {
              if(strcmp (action,"LEARN") == 0) {
                if(strcmp (value,"START") == 0) {
                  startRcLearning();
                }
              }
              else if (strcmp (action,"EXECUTE") == 0) {
  #if DEBUG
    Serial.println(F("execute mode activated"));
  #endif
                rcExecute(value);
                
              }
              else {
  #if DEBUG
                //  set the pin value
                Serial.println(F("setting pin"));
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
                  Serial.println(F("digital"));
  #endif

                  if(strncmp(value, "HIGH", 4) == 0) {
  #if DEBUG
                    Serial.println(F("HIGH"));
  #endif
                    digitalWrite(selectedPin, HIGH);
                  }
                  if(strncmp(value, "LOW", 3) == 0) {
  #if DEBUG
                    Serial.println(F("LOW"));
  #endif
                    digitalWrite(selectedPin, LOW);
                  }
                } 
                else {
  #if DEBUG
                  //  analog
                  Serial.println(F("analog"));
  #endif
                  //  get numeric value
                  int selectedValue = atoi(value);
  #if DEBUG
                  Serial.println(selectedValue);
  #endif
                  analogWrite(selectedPin, selectedValue);
                }
                showSuccess("msg", "successful");
              }
            } 
            else {
  #if DEBUG
              //  read the pin value
              Serial.println(F("reading pin"));
  #endif

              char* pin = action;
              //  determine analog or digital
              if(pin[0] == 'a' || pin[0] == 'A'){

                //  analog
                int selectedPin = pin[1] - '0';

  #if DEBUG
                Serial.println(selectedPin);
                Serial.println(F("analog"));
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
                Serial.println(F("digital"));
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

              showSuccess("pin", outValue);
            }
          } 
          else {

            //  error
            showError();

          }
          break;
        }
      }
    //closeConnection();
      
    }

  }
}

void startRcLearning() {
#if DEBUG
  Serial.println(F("learn mode activated")); 
#endif
  rcMode = 'l';
  rcModeTick = 5000;
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.print("{'code':'");

  //showSuccess("msg", "Started Successful");
}

void writeBufferLearning(unsigned int length, unsigned int* raw) {
  for (int i=0; i<= length*2; i++) {
    rcModeTick--;

    //rcBuffer += raw[i];

#if DEBUG
    Serial.print(raw[i]);
    Serial.print(F(","));
#endif
    if (client) {
      client.print(raw[i]);
    }

    if (rcModeTick < 0) {
      rcMode = 's';

      client.print("'}");
      closeConnection();
      break;
    }
    else
    {
      client.print(F(","));
      client.flush();
    }
    
  }
}

void showError() {

#if DEBUG
  Serial.println(F("erroring"));
#endif
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println();

  closeConnection();
}

void showSuccess(char* key, char* value) {

#if DEBUG
  Serial.println(F("success"));
#endif
//  return value with wildcarded Cross-origin policy
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.print("{'");
  client.print(key);
  client.print("':'");
  client.print(value);
  client.print("'}");

  closeConnection();
}

void closeConnection() {

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
#if DEBUG
  Serial.print(F("state: "));
  Serial.print(state);
  Serial.print(F(" delayTime: "));
  Serial.println(delayTime);
#endif
  if(state==1)
    digitalWrite(pin,HIGH);
  else
    digitalWrite(pin,LOW);
    
  customDelay(delayTime);
}

void rcExecute(char strCode[]) {

  char* strDelayTime = strtok(strCode,"-");
  char* codes = strtok(NULL,"-");
  //String strCodes = String(codes);
  int delayTime = atoi (strDelayTime);
  //int startDelayTime = strtok(NULL,",");
#if DEBUG
  Serial.println(F("Delay: "));
  Serial.println(delayTime);
  Serial.println(F("Codes: "));
  Serial.println(codes);
  Serial.println(F("Size: "));
  Serial.println(strlen(codes));

#endif

  for(int j=0; j<10; j++){
    setStateWithDelay(TRANSMITTER_PIN,0,11284);

    for (int i = 0; i < strlen(codes) ; i++) {
      int code = (codes[i] - '0') + 1;
      setStateWithDelay(TRANSMITTER_PIN,(i + 1) % 2,delayTime * code);
    }
    digitalWrite(TRANSMITTER_PIN,LOW);
  }
  showSuccess("msg", "success");


}
