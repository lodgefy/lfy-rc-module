/*
 Lodgefy RC-Module
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * RC Transmitter attached to pin 07
 * RC Receiver attached to pin 02
 
 Arduino 1.0.6
 
 */

#define DEBUG false
#define STATICIP true
#define RECEIVER_PIN_INTERRUPT 0 // Receiver on inerrupt 0 => that is pin #2
#define RECEIVER_PIN 2 
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
byte myip[] = {192,168,2,2};
#endif
byte serverip[] = {192,168,2,1};
int serverport = 8080;

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
  Ethernet.begin(mac, myip);
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
  mySwitch.enableReceive(RECEIVER_PIN_INTERRUPT);
  pinMode(TRANSMITTER_PIN,OUTPUT);
  digitalWrite(TRANSMITTER_PIN,LOW);
  webHook(0, 1);

}

#define BUFSIZE 255
#define CASESENSE true

int lambda;   // on pulse clock width (if fosc = 2KHz than lambda = 500 us)
static unsigned long buffer; //buffer for received data storage
 struct rfControl    //Struct for RF Remote Controls  
 {  
   unsigned long addr; //ADDRESS CODE  
   boolean btn1;    //BUTTON 1  
   boolean btn2;    //BUTTON 2  
 }; 

void loop() {

  if (rcMode == 'l') {
    writeBufferLearning(mySwitch.getReceivedBitlength(), mySwitch.getReceivedRawdata());
    mySwitch.resetAvailable();
  }
  if (rcMode == 'p') {

    rcModeTick--;
    if (rcModeTick < 0) {
      rcMode = 's';

      client.print("'}");
      closeConnection();
    }
    else
    {
      struct rfControl rfControl_1; 
      if(ACT_HT6P20B_RX(rfControl_1))  
      {  
        client.print(buffer, BIN);
        rcModeTick = 0;
      }
    }
  }
  else {
    //verify if something happened in normal mode
    struct rfControl rfControl_1;   
    if(ACT_HT6P20B_RX(rfControl_1))  
    {  
#if DEBUG
      Serial.print("Buffer: "); Serial.println(buffer, BIN); 
      Serial.println();
#endif
      webHook(1, buffer);
    }  
    EthernetBonjour.run();
    char clientline[BUFSIZE];
    int index = 0;
    client = server.available();
    if (client) {
      index = 0;
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          if(c != '\n' && c != '\r' && index < BUFSIZE){
            clientline[index++] = c;
            continue;
          }  

          client.flush();
          String urlString = String(clientline);
          String op = urlString.substring(0,urlString.indexOf(' '));
          urlString = urlString.substring(urlString.indexOf('/'), urlString.indexOf(' ', urlString.indexOf('/')));
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
                if(strcmp (value,"FREE") == 0) {
                  startRcLearning();
                } else if(strcmp (value,"PPA") == 0) {
                  startPpaLearning();
                } 
              }
              else if (strcmp (action,"EXECUTE") == 0) {
#if DEBUG
                Serial.println(F("execute mode activated"));
#endif
                rcExecute(value);
              }
              else if (strcmp (action,"PPA") == 0) {
#if DEBUG
                Serial.println(F("ppa mode activated"));
#endif
                ppaExecute(value);
                
              }
            }
          } 
          else {
            showError();
          }
          break;
        }
      }
    }
  }
}
void webHook(int type, unsigned long code) {

  if (client.connect(serverip, serverport)) {
    client.print("GET /");
    if (type == 0) {
     client.print("started");
    } else if (type == 1) {
      client.print("code");
    }
    client.print("/");
    client.print(code, BIN);
    client.println(" HTTP/1.0");
    client.println();

    closeConnection();
  } else {
    
  }
}

void startPpaLearning() {
#if DEBUG
  Serial.println(F("ppa learn mode activated")); 
#endif
  rcMode = 'p';
  rcModeTick = 1000;
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println();
  client.print(F("{'code':'"));
}

boolean ACT_HT6P20B_RX(struct rfControl &_rfControl){   
  static boolean startbit;   
  static int counter;      
  int dur0, dur1; 
  if (!startbit)  
  {
   dur0 = pulseIn(RECEIVER_PIN, LOW); 
    if((dur0 > 9200) && (dur0 < 13800) && !startbit) {
      lambda = dur0 / 23;  
      dur0 = 0;  
      buffer = 0;  
      counter = 0;  
      startbit = true;  
    }
  }  
  if (startbit && counter < 28)  
  {  
    ++counter;  
    dur1 = pulseIn(RECEIVER_PIN, HIGH);  
    if((dur1 > 0.5 * lambda) && (dur1 < (1.5 * lambda))) {  
      buffer = (buffer << 1) + 1;  
    }  
    else if((dur1 > 1.5 * lambda) && (dur1 < (2.5 * lambda))) {  
      buffer = (buffer << 1);    
    }  
    else {
      startbit = false;  
    }  
  }  
  if (counter==28) {
    if ((bitRead(buffer, 0) == 1) && (bitRead(buffer, 1) == 0) && (bitRead(buffer, 2) == 1) && (bitRead(buffer, 3) == 0)) {
      counter = 0;  
      startbit = false;  
      //Get ADDRESS CODE from Buffer  
      _rfControl.addr = buffer >> 6;  
      //Get Buttons from Buffer  
       _rfControl.btn1 = bitRead(buffer,4);  
       _rfControl.btn2 = bitRead(buffer,5);   
      //If a valid data is received, return OK  
      return true;  
    }
    else {
      startbit = false;  
    }
  }
  //If none valid data is received, return NULL and FALSE values   
  _rfControl.addr = NULL;  
  _rfControl.btn1 = NULL;  
  _rfControl.btn2 = NULL;   
  return false;  
 }  

void startRcLearning() {
#if DEBUG
  Serial.println(F("learn mode activated")); 
#endif
  rcMode = 'l';
  rcModeTick = 1000;
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println();
  client.print(F("{'code':'"));
}

void writeBufferLearning(unsigned int length, unsigned int* raw) {
  for (int i=0; i<= length*2; i++) {
    rcModeTick--;
#if DEBUG
    Serial.print(raw[i]);
    Serial.print(F(","));
#endif
    if (client) {
      client.print(raw[i]);
    }
    if (rcModeTick < 0) {
      rcMode = 's';
      client.print(F("'}"));
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
  delay(1);
  client.stop();
  while(client.status() != 0){
    delay(5);
  }
}

void customDelay(unsigned long time) {
  unsigned long end_time = micros() + time;
  while(micros() < end_time);
}

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


void ppaExecute(char strCode[]) {

  char* codes = strCode;
  //String strCodes = String(codes);
  int delayTime = 500;
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
    setStateWithDelay(TRANSMITTER_PIN,1,delayTime);
    for (int i = 0; i < strlen(codes) ; i++) {
      int code = (codes[i] - '0') + 1;
      int code2 = 1;
      if (code == 1) { 
        code2 = 2;
      }
      setStateWithDelay(TRANSMITTER_PIN,0,delayTime * code);
      setStateWithDelay(TRANSMITTER_PIN,1,delayTime * code2);
    }
    setStateWithDelay(TRANSMITTER_PIN,0,delayTime);
  }
  showSuccess("msg", "success");
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
