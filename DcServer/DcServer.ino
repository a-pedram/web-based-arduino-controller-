#include <Adafruit_CC3000.h>
#include <SPI.h>
#include <SD.h>
#include <ccspi.h>
#include "utility/debug.h"
#include "utility/socket.h"

# define sBuffSize 100
byte sendBuff1[sBuffSize+1];
byte sendBuff2[sBuffSize+1];

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed
#define WLAN_SSID       "mehdi"//"MN"//"wzLUMS" //    // cannot be longer than 32 characters!
#define WLAN_PASS       "qweqweqwe"//"donttouch"//"lums.ac.ir"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define LISTEN_PORT           80      // What TCP port to listen on for connections.  
                                      // The HTTP protocol uses port 80 by default.
#define MAX_ACTION            20      // Maximum length of the HTTP action that can be parsed.
#define MAX_PATH              64      // Maximum length of the HTTP request path that can be parsed.
                                      // There isn't much memory available so keep this short!
#define BUFFER_SIZE           MAX_ACTION + MAX_PATH + 20  // Size of buffer for incoming request data.
                                                          // Since only the first line is parsed this
                                                          // needs to be as large as the maximum action
                                                          // and path plus a little for whitespace and
                                                          // HTTP version.
#define TIMEOUT_MS            800    // Amount of time in milliseconds to wait for
                                     // an incoming request to finish.  Don't set this
                                     // too high or your server could be slow to respond.
Adafruit_CC3000_Server httpServer(LISTEN_PORT);
uint8_t buffer[BUFFER_SIZE+1];
int bufindex = 0;
char action[MAX_ACTION+1];
char path[MAX_PATH+1];

char pathFile[25];
char loadPathCmd[100];
long loadPathCnt[100];
byte loadPathEnd;
byte loadPathCounter;
byte mode=0;
int alarmEnable=false;

double battery,temperature;
int MQ4,MQ214;
long upTime,startTime;

#define LOADPATH  1
#define SAVEPATH  2
#define GASMODE 3
#define counterRPin 9
#define counterLPin 8
#define motorR1 40
#define motorR2 41
#define motorL1 42
#define motorL2 43
#define LEDGF 26
#define LEDGB 27
#define LEDGR 28
#define LEDGL 29
#define LEDR 30
#define LEDB 31
#define AlarmPIN 35
#define TRIG_PIN 38
#define ECHO_PIN 39


#define CSPIN 53
void setup(void)
{
    Serial.begin(115200);
  int i;
  for(i=26;i<32;i++)pinMode(i,OUTPUT);  // LEDs
  allLedOff();
  digitalWrite(LEDR,HIGH);
  for(i=40;i<46;i++)pinMode(i,OUTPUT);  // Motor Drive
  digitalWrite(44,HIGH);
  digitalWrite(45,HIGH);
    brake();

  pinMode(53, OUTPUT);
  if(!SD.begin(4))
    Serial.println(F("Card Failure"));
  Serial.println(F("Card is Ready"));
digitalWrite(LEDGF,HIGH);    
 
  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);
  // Initialise the module
  Serial.println(F("\nInitializing WiFi..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
digitalWrite(LEDGL,HIGH);  
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
  Serial.println(F("Connected!"));

digitalWrite(LEDGB,HIGH);

 Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

/*  uint32_t ipAddress = cc3000.IP2U32(192, 168, 43, 253);
  uint32_t netMask = cc3000.IP2U32(255, 255, 255, 0);
  uint32_t defaultGateway = cc3000.IP2U32(192, 168, 43, 1);
  uint32_t dns = cc3000.IP2U32(192, 168, 43, 1);
  if (!cc3000.setStaticIPAddress(ipAddress, netMask, defaultGateway, dns)) {
    Serial.println(F("Failed to set static IP!"));
    while(1);
  }*/

/*  Serial.println(F("\r\nNOTE: This sketch may cause problems with other sketches"));
  Serial.println(F("since the .disconnect() function is never called, so the"));
  Serial.println(F("AP may refuse connection requests from the CC3000 until a"));
  Serial.println(F("timeout period passes.  This is normal behaviour since"));
  Serial.println(F("there isn't an obvious moment to disconnect with a server.\r\n"));
  */
  
//ultrasonic
  pinMode(TRIG_PIN,OUTPUT);
  pinMode(ECHO_PIN,INPUT);
  // Start listening for connections
  httpServer.begin();  
  Serial.println(F("Listening for connections..."));
digitalWrite(LEDGR,HIGH);
delay(1000);
allLedOff();
startTime = millis();
}

long counterL=0,counterR=0;
int cntFlagR=0,cntFlagL=0;
char command=0;
char* commandParam;
char preCommand=0;
long startStop;
int parked=false;
unsigned long preTime=0,cmdCounter=0;
unsigned long t,t2=0;
int vRLow=500,vLLow=500;
int vRHigh=650,vLHigh=650;
int fd1=0,fu1=0,fd2=0,fu2=0;
int pv1=0,pv2=0;
int MR1State=0,MR2State=0,ML1State=0,ML2State=0;
 ////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
 ////////////////////////////////////// L  O  O  P  ///////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop(void)
{
  responseSerial();
  
    if(mode == GASMODE)
    {
      brake();
      alarm(500);
    }
else{

  int v1,v2;
  v1=analogRead(counterRPin);
  if (v1+3 < pv1) 
  {  
    fd1=fd1+1;
    pv1=v1;
  }
  if (v1 > pv1+3) 
  {  
    fu1=fu1+1;
    pv1=v1;
  }
  if (fu1 > 6 && fd1 >6)
  {
    counterR++;
    fu1=0;fd1=0;
  }
  v2=analogRead(counterLPin);
  if (v2+3 < pv2) 
  {  
    fd2=fd2+1;
    pv2=v2;
  }
  if (v2 > pv2+3) 
  {  
    fu2=fu2+1;
    pv2=v2;
  }
  if (fu2 > 6 && fd2 >6)
  {
    counterL++;
    fu2=0;fd2=0;
  }
  
/*  if(v1 < vRLow && cntFlagR ==1)
    {
      counterR++;
      cntFlagR=0;
    };;
  if (v1 > vRHigh) cntFlagR=1;
  
  v2=analogRead(counterLPin);  
  if(v2 < vLLow && cntFlagL ==1)
    {
      counterL++;
      cntFlagL=0;
    };
  if (v2 > vLHigh) cntFlagL=1;*/

  if(mode==LOADPATH)
    {
      //Serial.print(cmdCounter);Serial.println(command);
      if(cmdCounter >= loadPathCnt[loadPathCounter])
      {
        if(loadPathCounter<loadPathEnd)
          loadPathCounter++;        
        else
          loadPathCounter=0;  
        brake();
      allLedOff();  
        command=loadPathCmd[loadPathCounter];
        counterL=0;
        counterR=0;
        cmdCounter=0;
      }
    }
    if(command==0)
    {
      brake();      
      if (preCommand == 'a')
        alarmEnable=false;
      if(preCommand>0)
      {
        allLedOff();    
        if(mode==SAVEPATH)writeCommand(preCommand,cmdCounter);
        preCommand=0;
        counterL=0;
        counterR=0;
      }
    }
    switch(command)
    {
        case 'p':
          park('L');          
          cmdCounter =1;
          command=0;
          break;
        case 'P':
          park('R');          
          cmdCounter =1;
                    command=0;
          break;          
        case 'f':
          MR2State=HIGH;
          MR1State=LOW;
          ML1State=LOW;
          ML2State=HIGH;
          go();          
          digitalWrite(LEDGF,HIGH);
          cmdCounter=counterR;
          break;
        case 'b':
          MR2State=LOW;
          MR1State=HIGH;
          ML1State=HIGH;
          ML2State=LOW;
          go();
          digitalWrite(LEDGB,HIGH);
          cmdCounter=counterR;
          break;
        case 'r':
          MR2State=HIGH;
          MR1State=LOW;
          ML1State=LOW;
          ML2State=LOW;
          go();
          digitalWrite(LEDGR,HIGH);
          cmdCounter=counterL;
          break;
        case 'l':
          MR2State=LOW;
          MR1State=LOW;
          ML1State=LOW;
          ML2State=HIGH;
          go();
          digitalWrite(LEDGL,HIGH);
          cmdCounter=counterR;
          break;
        case 'z':
          turn('L',35);
          command=0;
          if(mode==LOADPATH)
              cmdCounter=99990;
          else
              cmdCounter=1;
          break;
        case 'x':
          turn('R',35);
          command=0;
          if(mode==LOADPATH)
              cmdCounter=99990;
          else
              cmdCounter=1;
          break;
        case 'a':
          alarmEnable = true;
          break;
        case 's':
          brake();
          if(mode==LOADPATH)
          {            
            if(preCommand != 's')startStop=millis();
            t = millis();
            cmdCounter = ( t-startStop)/1000;            
          }
          else
          {
            cmdCounter=atoi(commandParam);
          }
          digitalWrite(LEDR,HIGH);
          break;
      }    
      preCommand = command;      
if(parked)
  command=0;
}

  t=micros();
  
  if (t - preTime > 120000){
    brake();
    delay(5);
    readSensors();
    // Try to get a client which is connected.
   preTime=t+1000;
  Adafruit_CC3000_ClientRef client = httpServer.available();
  if (client) {
    brake();
    digitalWrite(LEDB,HIGH);
    Serial.println(F("Client connected."));
    // Process this request until it completes or times out.
    // Note that this is explicitly limited to handling one request at a time!

    // Clear the incoming data buffer and point to the beginning of it.
    bufindex = 0;
    memset(&buffer, 0, sizeof(buffer));
    
    // Clear action and path strings.
    memset(&action, 0, sizeof(action));
    memset(&path,   0, sizeof(path));

    // Set a timeout for reading all the incoming data.
    unsigned long endtime = millis() + TIMEOUT_MS;
    
    // Read all the incoming data until it can be parsed or the timeout expires.
    bool parsed = false;
    while (!parsed && (millis() < endtime) && (bufindex < BUFFER_SIZE)) {
      if (client.available()) {
        buffer[bufindex++] = client.read();
      }
      parsed = parseRequest(buffer, bufindex, action, path);
    }

    // Handle the request if it was parsed.
    if (parsed) {
      Serial.println(F("Processing request"));
      Serial.print(F("Path: ")); Serial.println(path);
      // Check the action to see if it was a GET request.
      if (strcmp(action, "GET") == 0) {
        response(path,&client);
      }
      else {
        // Unsupported action, respond with an HTTP 405 method not allowed error.
        client.fastrprintln(F("HTTP/1.0 405 Method Not Allowed"));
        client.fastrprintln(F(""));
      }
    }
    // Wait a short period to make sure the response had time to send before
    // the connection is closed (the CC3000 sends data asyncronously).
    delay(100);
    Serial.println(F("Client disconnected"));
    client.close();
    digitalWrite(LEDB,LOW);
  }
  go();
  }
}


void response(char *cmd,Adafruit_CC3000_ClientRef *client)
{ 
// if request is a command
  if(*(cmd+1)=='*')
  {
    switch(*(cmd+2))
    {
      case '+':        
        command = *(cmd+3);
        commandParam=cmd+4;
        break;
      case '-':
        command =0;
        break;
      case 'F'://Start Saving a path file from controller
        strcpy(pathFile,"html/paths/");
        strcat(pathFile,cmd+3);
        strcat(pathFile,".txt");
        httpHeader(client,".");
        (*client).write('1');
        mode=SAVEPATH;
        break;
      case 'R'://Remove File
        SD.remove(cmd+3);
        delay(7);
        (*client).write('1');
        break;
      case 'r': //repoert Sensory data
        reportSensors(client);
        break;
      case 'L'://Load and execute a path File
        strcpy(pathFile,"html/paths/");
        strcat(pathFile,cmd+3);
        httpHeader(client,".");
        (*client).write('1');
        
        delay(50);
        
        File sPath;
        sPath = SD.open(pathFile);
        int i=0;
        while(sPath.available())
        {
          loadPathCmd[i] = sPath.read();
          loadPathCnt[i] = sPath.parseInt();
          sPath.read();sPath.read();
          i++;          
        }
        
        (*client).write('1');
        sPath.close();        
        mode=LOADPATH;
        cmdCounter=0;
        loadPathEnd=i-1;
        loadPathCounter=0;
        command=loadPathCmd[0];
        break;
    }
    return;
  }

  char filename[30];
  char contentType[15];

  strcpy(filename,"html");
  strcat(filename,cmd);

  Serial.println(filename);  
   delay(100);

  if(SD.exists(filename))
 {
     if(strcmp(filename,"html/")==0)
       strcat(filename,"INDEX.HTM");
   File webFile;
   webFile = SD.open(filename);
  if(webFile.isDirectory())
  {
    httpHeader(client,".htm");
    printDirectory(client, webFile,filename);
    (*client).fastrprintln(F("<br><a href=\"index.htm\">Home</a>"));
    return;
  }
      
     byte L;
     L = strlen(filename);
     httpHeader(client,filename+L-4);

     unsigned int j,t;     
     j=0;t=0;
delay(100);
//           test();
//return;

     while(j = webFile.read(sendBuff1,sBuffSize))
     {   // i 2 tai zir ro tarkib konAAAM!
           //Serial.println("1");t=0;
           //t = webFile.read(sendBuff2,sBuffSize); 
          // Serial.println("2");
           //if(j>0){delay(2);t = webFile.read(sendBuff2,sBuffSize); }
           //t= t+j;
           //if(t%10==0)Serial.println(t);
           //delayMicroseconds(2400);
           //delay(10);
           //Serial.println(j);
           //SPI.begin();
           delay(3);
           Serial.println("2");
           (*client).write(sendBuff1,j,0);
           Serial.println("3");
           //if(t>0){delay(6);client.write(sendBuff2,t,0);}
           //unsigned short loc;
          //SpiWrite(sendBuff1,sBuffSize);
           Serial.println("4");
           //if(t>0){delay(1);client.write(sendBuff2,t,0);}
           //Serial.println("W");
           delay(27);
    }
    Serial.println(t);
   webFile.close();
 } 
 else
 {
   (*client).fastrprintln(F("HTTP/1.0 404 POK"));   
   (*client).fastrprintln(F("Connection: close"));
   (*client).fastrprintln(F(""));   
   Serial.println(" doesn't exists");
 }
//  proc=0;
}

void writeCommand(char cmd,unsigned long cnt)
{
  File f= SD.open(pathFile,FILE_WRITE);
  f.print(cmd);
  f.println(cnt);
  f.close();
}

void printDirectory(Adafruit_CC3000_ClientRef *client, File dir,char *filename) {
  beginHTML(client);
   while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files return to the first file in the directory
       dir.rewindDirectory();
       break;
     }
    delay(20);
     (*client).fastrprint(F("<a href=\""));     
     (*client).print(filename+4);(*client).print(entry.name());if(entry.isDirectory())(*client).print('/');
     (*client).fastrprint(F("\">"));(*client).print(entry.name());
     (*client).fastrprint(F("</a>&Tab;"));
     (*client).fastrprint(F("<a href='#' onclick=\"document.getElementById('out').src='/*L")); /**/
     (*client).print(entry.name());(*client).fastrprint(F("';\">Run</a>&Tab;"));
     (*client).fastrprint(F("<a href='#' onclick=\"document.getElementById('out').src='/*R")); /**/
     (*client).print(filename);(*client).print(entry.name());(*client).fastrprint(F("';location.reload();\">Remove</a>&Tab;"));
     (*client).println(entry.size(), DEC);     
     (*client).fastrprint(F("<br>"));
     delay(35);
   }  
  endHTML(client);
}
void beginHTML(Adafruit_CC3000_ClientRef *client)
{
  (*client).fastrprint(F("<html><head></head><body>"));
}
void endHTML(Adafruit_CC3000_ClientRef *client)
{
    (*client).fastrprint(F("<iframe id='out' name='out'></iframe><a href='control.htm'>Control</a><a href='report.htm'>Report</a></body></html>"));
}

void httpHeader(Adafruit_CC3000_ClientRef *client,char *type)
{
   (*client).fastrprintln(F("HTTP/1.0 200 OK"));
   (*client).fastrprint(F("Content-Type: "));
   if(strcmp(type,".htm")==0)
       (*client).fastrprintln(F("text/html"));
   else if(strcmp(type,".HTM")==0)
     (*client).fastrprintln(F("text/html"));
   else if(strcmp(type,".jpg")==0)
       (*client).fastrprintln(F("image/jpg"));
   else if(strcmp(type,".JPG")==0)
       (*client).fastrprintln(F("image/jpg"));
   else
       (*client).fastrprintln(F("text/plain"));
     
   (*client).fastrprintln(F("Connection: close"));
      // Send an empty line to signal start of body.
   (*client).fastrprintln(F("")); 
}

void responseSerial()
{
  while(Serial.available())
  {
    delay(5);
    int c;
    char ch=0;
    int i;
    char buff[30];
    ch = Serial.read();
    Serial.println(ch);
    if(ch=='*')
      {
        ch = Serial.read();
        if (ch=='F')
        {
          i=0;
          while(true)
          {
            ch = Serial.read();
            //Serial.print(i);Serial.println(ch);
            if(ch=='*' || i > 28)
            {
              buff[i]=0;
              Serial.println("File Name:");Serial.println(buff);
              goto cont;
            }
            buff[i]=ch;
            i++;
          };
          cont:
          File f;
          f= SD.open(buff,FILE_WRITE);
          while(true)
          {
            c=Serial.readBytes(buff,30);            
            if(c==0) break;
            f.write(buff,c);
            // Serial.println(buff);
          };
          f.close();
          Serial.println("wrote!");
          delay(5);
        }
      }
    
  }
}

// Return true if the buffer contains an HTTP request.  Also returns the request
// path and action strings if the request was parsed.  This does not attempt to
// parse any HTTP headers because there really isn't enough memory to process
// them all.
// HTTP request looks like:
//  [method] [path] [version] \r\n
//  Header_key_1: Header_value_1 \r\n
//  ...
//  Header_key_n: Header_value_n \r\n
//  \r\n
bool parseRequest(uint8_t* buf, int bufSize, char* action, char* path) {
  // Check if the request ends with \r\n to signal end of first line.
  if (bufSize < 2)
    return false;
  if (buf[bufSize-2] == '\r' && buf[bufSize-1] == '\n') {
    parseFirstLine((char*)buf, action, path);
    return true;
  }
  return false;
}
///////////////////////////////////////////
// Parse the action and path from the first line of an HTTP request.
void parseFirstLine(char* line, char* action, char* path) {
  // Parse first word up to whitespace as action.
  char* lineaction = strtok(line, " ");
  if (lineaction != NULL)
    strncpy(action, lineaction, MAX_ACTION);
  // Parse second word up to whitespace as path.
  char* linepath = strtok(NULL, " ");
  if (linepath != NULL)
    strncpy(path, linepath, MAX_PATH);
}
/////////////////////////////////////////////////
// Tries to read the IP address and other connection details
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

void brake()
{
 digitalWrite(motorR1,LOW);
 digitalWrite(motorR2,LOW);
 digitalWrite(motorL1,LOW);
 digitalWrite(motorL2,LOW); 
}

void go()
{
 digitalWrite(motorR1,MR1State);
 digitalWrite(motorR2,MR2State);
 digitalWrite(motorL1,ML1State);
 digitalWrite(motorL2,ML2State); 
}


void allLedOff()
{
  int i;
    for(i=26;i<32;i++)digitalWrite(i,LOW);  // LEDs  
}

int alarmLed=true;
void alarm(long duration)
{
  long m,pm,start;
  start=millis();
  int dm;
  dm=1200; //200Hz
  while(1)
  {
    if(! alarmEnable )
   { 
     digitalWrite(LEDR,LOW);
     return;
   }
   
    digitalWrite(AlarmPIN,HIGH);
    delayMicroseconds(dm);
    digitalWrite(AlarmPIN,LOW);
    delayMicroseconds(dm);
    m=millis();
    if (m > pm)
    {
      pm = millis();
      dm = dm - 3;
      if ( m - start > duration)
        break; 
    }
  }
    if (alarmLed)
  {
    digitalWrite(LEDR,HIGH);
    alarmLed = false;
  }
  else{
    digitalWrite(LEDR,LOW);
    alarmLed = true;
  }
}

void reportSensors(Adafruit_CC3000_ClientRef *client)
{
 float d=frontDistance();
  httpHeader(client,".htm");
 (*client).fastrprint(F("{\"MQ4\":"));
 (*client).print(MQ4);
 (*client).fastrprint(F(",\"MQ214\":"));
 (*client).print(MQ214);
 (*client).fastrprint(F(",\"battery\":"));
 (*client).print(battery);
 (*client).fastrprint(F(",\"temperature\":"));
 (*client).print(temperature);
 (*client).fastrprint(F(",\"upTime\":"));
 (*client).print(upTime);
 (*client).fastrprint(F(",\"distance\":"));
 (*client).print(d);
 (*client).fastrprint(F("} \n"));
 delay(50);
 (*client).close();
}

void readSensors()
{
  battery = (5.0 * analogRead(6))/512.0;
  MQ4 = analogRead(12);
  MQ214 = analogRead(13);
  
  temperature = (analogRead(5)/1024.0)*5000 /10;
  upTime = (millis()-startTime)/60000;
  
  if (MQ4 >705 || MQ214 >395 || temperature > 40 )
  {
    if (upTime >=1)
    {
    alarmEnable=true;
    brake();
    mode=GASMODE;
    command=255;
    }
  }
}

int frontDistance()
{
  long duration; 
  float distanceCm;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN,HIGH);
  float soundSpeed;
  soundSpeed = 331.3 + 0.606 * temperature;
  distanceCm = (soundSpeed * duration)/ 20000 ;
  return(distanceCm);
}

int parkingState=0;
int d=1000,pd=0;
long parkTime;
void park(char dir)
{
  float d;
  brake();
  d=frontDistance();
  while(true)
  {    
    turn('L',1);
    if(frontDistance()>d) break;
  }
  while(true)
  {    
    d=frontDistance();
    turn('R',1);
    if(frontDistance() >= d) break;
  }
  goXcmForward(35.0);
  delay(50);
  turn(dir,11);  
  delay(50);
  goXcmForward(35.0);
  brake();
return;
}

void turn(char dir,int amount)
{
  if (dir=='R')
  {
    digitalWrite(motorL2,LOW);
    digitalWrite(motorL1,LOW);
    digitalWrite(motorR1,LOW);
    digitalWrite(motorR2,HIGH);
    digitalWrite(LEDGR,HIGH);
  }
  if(dir=='L')
  {
    digitalWrite(motorR1,LOW);
    digitalWrite(motorR2,LOW);
    digitalWrite(motorL2,HIGH);
    digitalWrite(motorL1,LOW);
    digitalWrite(LEDGL,HIGH);
  }
  int v1,v2;
  int r=0,l=0;
  int ppv1=0,ppv2=0;
while(true){
  v1=analogRead(counterRPin);
  if (v1+3 < ppv1) 
  {  
    fd1=fd1+1;
    ppv1=v1;
  }
  if (v1 > ppv1+3) 
  {  
    fu1=fu1+1;
    pv1=v1;
  }
  if (fu1 > 6 && fd1 >6)
  {
    r++;
    fu1=0;fd1=0;
  }
  v2=analogRead(counterLPin);
  if (v2+3 < ppv2) 
  {  
    fd2=fd2+1;
    ppv2=v2;
  }
  if (v2 > ppv2+3) 
  {  
    fu2=fu2+1;
    ppv2=v2;
  }
  if (fu2 > 6 && fd2 >6)
  {
    l++;
    fu2=0;fd2=0;
  }  
  if(r>amount)break;
  if(l>amount)break;
  delay(2);
}  

brake();
allLedOff();
}

void goXcmForward(float x)
{
    digitalWrite(motorR1,LOW);
    digitalWrite(motorR2,HIGH);
    digitalWrite(motorL2,HIGH);
    digitalWrite(motorL1,LOW);
    digitalWrite(LEDGF,HIGH);

  int v1;
  int ppv1=0;
fu1=0;fd1=0;  
while(true){
   
    v1=analogRead(counterLPin);
  if (v1+3 < ppv1) 
  {  
    fd1=fd1+1;
    ppv1=v1;
  }
  if (v1 > ppv1+3) 
  {  
    fu1=fu1+1;
    ppv1=v1;
  }
  if (fu1 > 4 && fd1 >4)
  {    
    fu1=0;fd1=0;
   if (frontDistance() < x)
   {
     break;
   }
   else
   {
      digitalWrite(motorR1,LOW);
      digitalWrite(motorR2,HIGH);
      digitalWrite(LEDGR,HIGH);
      digitalWrite(motorL2,HIGH);
      digitalWrite(motorL1,LOW);
      digitalWrite(LEDGL,HIGH);
   }
  }  
  delay(2);
}  

brake();
allLedOff();
 
}
