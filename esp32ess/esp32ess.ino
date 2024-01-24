
#include <WebServer.h>
#include <HTTPUpdateServer.h>

//Wifi password
const char* SSID = "FSM";
const char* PASSWORD = "0101010101";

String Shelly_IP = "192.168.178.47";

uint8_t * bssid;

//Other
const int VEBUS_RXD1=16, VEBUS_TXD1=17, VEBUS_DE=4;  //Victron Multiplus VE.bus RS485 gpio pins

//other variables:
char frbuf1[128];                    //assembles one complete frame received by Multiplus
char frbuf2[128];                    //assembles one complete frame received by Multiplus
char txbuf1[32];                    //buffer for assembling bare command towards Multiplus (without replacements or checksum)
char txbuf2[32];                    //Multiplus output buffer containing the final command towards Multiplus, including replacements and checksum
byte rxnum;                    
byte frp = 0;                       //Pointer into Multiplus framebuffer frbuf[] to store received frames.
byte frameNr = 0;                   //Last frame number received from Multiplus. Own command has be be sent with frameNr+1, otherwise it will be ignored by Multiplus.

static volatile short essPower = 0;

WebServer server(80);
HTTPUpdateServer httpUpdater;

TaskHandle_t Task1; // wifi always core 1!

int prepareESScommand(char *outbuf, short power, byte desiredFrameNr)
{
  byte j=0;
  outbuf[j++] = 0x98;           //MK3 interface to Multiplus
  outbuf[j++] = 0xf7;           //MK3 interface to Multiplus
  outbuf[j++] = 0xfe;           //data frame
  outbuf[j++] = desiredFrameNr;
  outbuf[j++] = 0x00;           //our own ID
  outbuf[j++] = 0xe6;           //our own ID
  outbuf[j++] = 0x37;           //CommandWriteViaID
  outbuf[j++] = 0x02;           //Flags, 0x02=RAMvar and no EEPROM
  outbuf[j++] = 0x83;           //ID = address of ESS power in assistand memory
  outbuf[j++] = (power & 0xFF); //Lo value of power (positive = into grid, negative = from grid)
  outbuf[j++] = (power >> 8);   //Hi value of power (positive = into grid, negative = from grid)
  return j;
}

int preparecmd(char *outbuf, byte desiredFrameNr)
{
  byte j=0;
  outbuf[j++] = 0x98;           //MK3 interface to Multiplus
  outbuf[j++] = 0xf7;           //MK3 interface to Multiplus
  outbuf[j++] = 0xfe;           //data frame
  outbuf[j++] = desiredFrameNr;
  outbuf[j++] = 0x00;           //our own ID
  outbuf[j++] = 0xe6;           //our own ID
  outbuf[j++] = 0x30;           //Command read ram
  outbuf[j++] = 0x04;           //4-bat volt
  outbuf[j++] = 0x05;           //5-bat amps
  outbuf[j++] = 0x0E;           //14-AC Power
  return j;  
}

int destuffFAtoFF(char *outbuf, char *inbuf, int inlength)
{
  int j=0;
  for (int i = 0; i < 4; i++) outbuf[j++] = inbuf[i];
  for (int i = 4; i < inlength; i++)
  {
    byte c = inbuf[i];
    if (c == 0xFA)
    {
      i++;
      c = inbuf[i];
      outbuf[j++] = c + 0x80;
    }
    else outbuf[j++] = c;    //no replacement
  }  
  return j;   //new length of output frame
}

int commandReplaceFAtoFF(char *outbuf, char *inbuf, int inlength)
{
  int j=0;
  //copy over the first 4 bytes of command, as there is no replacement
  for (int i = 0; i < 4; i++) outbuf[j++] = inbuf[i];
  
  //starting from 5th byte, replace 0xFA..FF with double-byte character
  for (int i = 4; i < inlength; i++)
  {
    byte c = inbuf[i];
    if (c >= 0xFA)
    {
      outbuf[j++] = 0xFA;
      outbuf[j++] = 0x70 | (c & 0x0F);
    }
    else outbuf[j++] = c;    //no replacement
  }
  return j;   //new length of output frame
}

int appendChecksum(char *buf, int inlength)
{
  int j=0;
  //calculate checksum starting from 3rd byte
  byte cs=1;
  for (int i = 2; i < inlength; i++)
  {
    cs -= buf[i];
  }
  j = inlength;
  if (cs >= 0xFA)
  {
    buf[j++] = 0xFA;
    buf[j++] = (cs-0xFA);
  }
  else
  {
    buf[j++] = cs;
  }
  buf[j++] = 0xFF;  //append End Of Frame symbol
  return j;   //new length of output frame
}

void sendmsg(int msgtype)
{
  int len;
  if (msgtype == 1) 
    len = prepareESScommand(txbuf1, essPower, (frameNr+1) & 0x7F);
  if (msgtype == 2)
    len = preparecmd(txbuf1, (frameNr+1) & 0x7F);    
  len = commandReplaceFAtoFF(txbuf2, txbuf1, len);
  len = appendChecksum(txbuf2, len);
  //write command into Multiplus :-)
  digitalWrite(VEBUS_DE,HIGH);      //set RS485 direction to write
  Serial1.write(txbuf2, len); //write command bytes to UART
  Serial1.flush(); //
  digitalWrite(VEBUS_DE,LOW);   //set RS485 direction to read
}

bool autozero;             // auto zero 
bool chgonly ;             // auto charge only 
bool syncrxed;             // sync from multiplus received
bool gotmsg;               // have message to be sent
bool acked;                // message has been acked
bool nosync;               // NO sync from multiplus
bool gotMP2data;               
bool sendnow;              // send alt msg
unsigned long synctime;    // tx timeslot
unsigned long getpwrtime;  // shelly getpower
unsigned long getdatatime; // multiplus getpower
unsigned long LoopInterval = 5000;
int meterPower = 0;        // result of newest power measurement
int reqPower = 0;          // requested power to multiplus
float expectedPower = 0.0; // expected power of multiplus

// PID factor
float Kp = 0.9;
float Kd = 0.0;
float Ki = 0.0;

#define txdelay 8          // delay from sync to tx
#define i_maxpower   900   // max power for inverter  
#define c_maxpower -1500   // max power for charger. Must be negative

char extframe[32];   
int extframelen;   
int16_t BatVolt,BatAmp,ACPower;

void multiplusCommandHandling()
{
  //Check for new bytes on UART
  while (Serial1.available())
  {
    char c = Serial1.read();       //read one byte
    frbuf1[frp++] = c;   //store into framebuffer
    if (c==0x55) synctime = millis(); 
    if (c==0xFF)        //in case current byte was EndOfFrame, interprete frame
    {
      frp = destuffFAtoFF(frbuf2,frbuf1,frp);
      if ((frbuf2[2] == 0xFD) && (frbuf2[4] == 0x55))  //if it was a sync frame:
      {
        frameNr = frbuf2[3];
        syncrxed = true;
      }
      if ((frbuf2[0] == 0x83) && (frbuf2[1] == 0x83) && (frbuf2[2] == 0xFE))
      {
        if ((frbuf2[4] == 0x00) && (frbuf2[5] == 0xE6)) 
        {
          if      (frbuf2[6] == 0x87) acked = true;
          else if (frbuf2[6] == 0x85) 
          {
            gotMP2data = true;
            BatVolt = 256*frbuf2[8]  + frbuf2[7];
            BatAmp  = 256*frbuf2[10] + frbuf2[9];
            ACPower = 256*frbuf2[12] + frbuf2[11];
            for (int i=0;i<frp;i++) extframe[i] = frbuf2[i];
            extframelen = frp;
          }
        }
      }
      rxnum = frp;
      frp = 0;
    }
    else syncrxed = false; // unexpected char received
  }
}

void setup() 
{
  pinMode(VEBUS_DE, OUTPUT);    //RS485 RE/DE direction pin for UART1
  digitalWrite(VEBUS_DE,LOW);   //set RS485 direction to read
  
  //Setup SerialMonitor for debug information
  Serial.begin(115200);
  //Setup Serial port for VE.Bus RS485 to Multiplus
  Serial1.begin(256000, SERIAL_8N1, VEBUS_RXD1, VEBUS_TXD1);

  //Init webserver
  WiFi.mode(WIFI_STA);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  bssid = WiFi.BSSID(); 
  httpUpdater.setup(&server,"/up"); 
  server.on("/", handleRoot);
  server.on("/graph", drawGraph);
  server.on("/data", drawData);
  server.on("/shelly", shellyIP);
  server.begin();
  Serial.println(" connected to WiFi");
  Serial.println(WiFi.localIP());

  clearwp();  
  // WiFi always core 1
  xTaskCreatePinnedToCore(VEBuscode, "VEBus", 10000, NULL, 0, &Task1, 0);
}

//---------------------------------------

float fact = 0.02;
float invfact = 1.0 - fact;

void VEBuscode(void * parameter) 
{
  int targetWatt;
  int totalWatt;
    
  Serial.print("Comm core ID: ");
  Serial.println(xPortGetCoreID());
  
  while(true) 
  {
    if (millis() > getpwrtime)
    {
      getpwrtime = millis() + LoopInterval;
      getdatatime = getpwrtime - 500;

      if (WiFi.status() == WL_CONNECTED) 
      {
        meterPower = http_getpwr("http://" + Shelly_IP + "/status");
        if (meterPower == 12345) meterPower = 0;
        Serial.print("Shelly Power "); Serial.println(meterPower);
      }
      else meterPower = 0;

      // meterPower positive = net consumption
      // essPower positive = invert
      // essPower negative = charge
    
      if (autozero)
      {       
        //totalWatt = Kp * (meterPower + reqPower); // barebones
        if (gotMP2data)
        {
          gotMP2data = false;
          totalWatt = Kp * (meterPower - ACPower);
        }
        else
        {
          ACPower = 0;
          totalWatt = Kp * (meterPower + int(expectedPower));
        }

        if (totalWatt > 0) targetWatt =  25; // inverting
        else               targetWatt = -10; // charging
      
        reqPower = totalWatt - targetWatt; 
      
        // invert, positive values
        if (reqPower > 50) 
        {
          if (reqPower > i_maxpower) reqPower = i_maxpower;
          if (chgonly) reqPower = 0;
        }
        // charge, negative values
        else if (reqPower < -25)
        {
          if (reqPower < c_maxpower) reqPower = c_maxpower;
        }
        else reqPower = 0; // do zero      
        syncrxed = false;
      }
         
      essPower = short(reqPower);
      gotmsg = true; // make sure no timeout

      //storewp(meterPower,reqPower,ACPower); // barebones
      expectedPower = fact*expectedPower + invfact*reqPower; //5s
      storewp(meterPower,int(expectedPower),ACPower);
    }  
    if (millis() > getdatatime)
    {
      getdatatime += LoopInterval; 
      sendnow = true; 
    }
  
    server.handleClient(); // do the webserver
    delay(10);
  }
}

//---------------------------------------

void loop() 
{  
  {
    multiplusCommandHandling(); //Multiplus VEbus: Process VEbus information
    
    if (syncrxed) 
    {
      nosync = false;
      if (gotmsg)
      {
        if (millis() > synctime + txdelay) 
        {
          syncrxed = false;
          sendmsg(1);
          gotmsg = false;
        }
      }
      else if (sendnow)
      {
        if (millis() > synctime + txdelay) 
        {
          syncrxed = false;
          sendmsg(2);
          sendnow = false;
        }        
      }
      else syncrxed = false;
    }    
    if (millis() > synctime + 1000) 
    {
      nosync = true;
      synctime = millis();
      Serial.println("No Sync");
    }
  }
}
