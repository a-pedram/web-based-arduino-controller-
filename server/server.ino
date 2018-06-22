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
#define LOADPATH  1
#define SAVEPATH  2

int motorPin[2][4]={
  {40,42,41,43}, // blue,Pink,Yellow Orange in order
  {44,46,45,47}
  }; 
int motorSpeed = 1200;  //variable to set stepper speed
int countsperrev = 544; // number of steps per full revolution
int lookup[8] = {B01000,B01100,B00100,B00110,B00010,B00011,B00001,B01001};
byte stepI;
long stepCount=0;
byte stepStart=0;
long stepTime;

#define CSPIN 53
void setup(void)
{
  Serial.begin(115200);
  pinMode(53, OUTPUT);
  if(!SD.begin(4))
    Serial.println(F("Card Failure"));
  Serial.println(F("Card is Ready"));
  int i;
  for(i=30;i<35;i++)pinMode(i,OUTPUT);  // LED directions
  for(i=40;i<48;i++)pinMode(i,OUTPUT);  // Motor Drive
 
  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);
  
  // Initialise the module
  Serial.println(F("\nInitializing WiFi..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  // Display the IP address DNS, Gateway, etc.
  while (! displayConnectionDetails()) {
    delay(1000);
  }
/*  Serial.println(F("\r\nNOTE: This sketch may cause problems with other sketches"));
  Serial.println(F("since the .disconnect() function is never called, so the"));
  Serial.println(F("AP may refuse connection requests from the CC3000 until a"));
  Serial.println(F("timeout period passes.  This is normal behaviour since"));
  Serial.println(F("there isn't an obvious moment to disconnect with a server.\r\n"));
  */
  // Start listening for connections
  httpServer.begin();  
  Serial.println(F("Listening for connections..."));
  
}

char command=0;
char preCommand=0;
unsigned long preTime=0,cmdCounter=0;
void loop(void)
{
  aStep(command);
  responseSerial();
  if(stepCount==0)
  {
    if(mode==LOADPATH)
    {
      //Serial.print(cmdCounter);Serial.println(command);
      if(cmdCounter >= loadPathCnt[loadPathCounter])
      {
        if(loadPathCounter<loadPathEnd)
          loadPathCounter++;        
        else
          loadPathCounter=0;  
        command=loadPathCmd[loadPathCounter];
        cmdCounter=0;
      }
    }
    if(command==0)
    {
      if(preCommand>0)
      {
        int i;for(i=30;i<35;i++)digitalWrite(i,LOW);
        if(mode==SAVEPATH)writeCommand(preCommand,cmdCounter);
        preCommand=0;
        cmdCounter =0;
      }
    }
    else
    {
      int i;for(i=30;i<35;i++)digitalWrite(i,LOW);
      switch(command)
      {
        case 'P':
          if(mode==LOADPATH)motorSpeed=loadPathCnt[loadPathCounter];
          cmdCounter = motorSpeed;
          break;
        case 'f':
          digitalWrite(30,HIGH);
          break;
        case 'b':
          digitalWrite(31,HIGH);
          break;
        case 'r':
          digitalWrite(32,HIGH);
          break;
        case 'l':
          digitalWrite(33,HIGH);
          break;
        case 's':
          digitalWrite(34,HIGH);
          break;
      }
      stepCount =1;
      preCommand = command;
      cmdCounter++;
    }    
  }

  unsigned long t;
  t=micros();
  if (t - preTime > 20000){
  // Try to get a client which is connected.
  preTime=t+1000;
  Adafruit_CC3000_ClientRef client = httpServer.available();
  if (client) {
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
  }
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
        break;
      case '-':
        command =0;
        break;
      case 'P':
        motorSpeed= atoi(cmd+3);
        writeCommand('P',motorSpeed);
        command = 'P';
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
    (*client).fastrprint(F("<iframe id='out' name='out'></iframe></body></html>"));
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

/// Motor Functions
void aStep(char dir)
{

  if(stepCount<1)
    return;
  if(dir==0)
    return;
 // if(stepStart==0)
 // {
 //   stepStart=1;
  //  stepTime = micros() +motorSpeed;
  //}
 
  if(stepTime > micros())
    return;
  switch(dir)
  {
    case 'f':
      setOutput(0,7-stepI);
      setOutput(1,stepI);
      break;
    case 'b':
      setOutput(0,stepI);
      setOutput(1,7-stepI);
      break;
    case 'r':
      setOutput(0,stepI);
      setOutput(1,stepI);
      break;
    case 'l':
      setOutput(0,7-stepI);
      setOutput(1,7-stepI);
      break;
    case 's':
      break;
  }
  stepI++;
  if(stepI>7)
  {
    stepI=0;
    stepCount--;
  }
  stepTime=micros()+motorSpeed;
}

void setOutput(byte motor,int out)
{
  Serial.println(motorPin[motor][0]);
  digitalWrite(motorPin[motor][0], bitRead(lookup[out], 0));
  digitalWrite(motorPin[motor][1], bitRead(lookup[out], 1));
  digitalWrite(motorPin[motor][2], bitRead(lookup[out], 2));
  digitalWrite(motorPin[motor][3], bitRead(lookup[out], 3));
}

  
