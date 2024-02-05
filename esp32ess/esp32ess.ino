#include <WebServer.h>
#include <HTTPUpdateServer.h>

//Wifi password
const char* SSID = "FSM";
const char* PASSWORD = "0101010101";

String Shelly_IP = "192.168.178.47";

uint8_t * bssid;

static volatile short essPower = 0;

WebServer server(80);
HTTPUpdateServer httpUpdater;

TaskHandle_t Task1; // wifi always core 1!

bool autozero = true;      // auto zero 
bool chgonly = true;       // auto charge only 
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

#define txdelay 8          // delay from sync to tx
#define i_maxpower  1000   // max power for inverter  
#define c_maxpower -1750   // max power for charger. Must be negative

char extframe[32];   
int extframelen;  
 
void setup() 
{
  //Setup SerialMonitor for debug information
  Serial.begin(115200);

  init_vebus();
  
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
  server.on("/graph", getGraph);
  server.on("/pic.svg", drawGraph);
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

extern int16_t ACPower;
int shellyfail;
bool batlow,bathi;
extern float BatVolt;

float filtPower;
int expectedPower = 0; // expected power of multiplus

bool DoInv,DoChg;
int InvHi = 100;
int InvLo =  50;
int ChgHi = -20;
int ChgLo = -80;
int TargetHi =  50;
int TargetLo = -25;

void VEBuscode(void * parameter) 
{
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
        bool isok;
        isok = http_getpwr("http://" + Shelly_IP + "/status", &meterPower);
        if (!isok) 
        { 
          meterPower /= 2; 
          shellyfail++; 
        }
        Serial.print("Shelly Power " + String(meterPower));
      }
      else meterPower = 0;

      // meterPower from shelly, positive = net consumption
      // essPower positive = invert, negative = charge
      // ACPower is actual power measurement from mp2
      // reqPower is power request sent to MP2
            
      if (autozero)
      {       
        if (gotMP2data)
        {
          gotMP2data = false;          
          expectedPower = - ACPower; // invert
          batlow = (BatVolt <= 51.0);
          bathi  = (BatVolt >= 58.0);
        }
        else
          expectedPower = 0.9*reqPower;
        
        totalWatt = meterPower + expectedPower;
        
        // do target band with hysteresis
        if      (totalWatt > InvHi) DoInv = true;
        else if (totalWatt < InvLo) DoInv = false;
        if      (totalWatt > ChgLo) DoChg = false;
        else if (totalWatt < ChgHi) DoChg = true;
        
        // check conditions
        if (chgonly) DoInv = false;
        if (batlow)  DoInv = false;
        if (bathi)   DoChg = false;
        
        if (totalWatt > 0) // inverting
          reqPower = totalWatt - TargetHi; 
        else               // charging      
          reqPower = totalWatt - TargetLo; 

        if (((meterPower < -50) && (expectedPower >  50)) ||
            ((meterPower >  50) && (expectedPower < -50))) 
          filtPower = 0.9 * float(reqPower); // fast adjustment
        else 
          filtPower = 0.5 * (filtPower + float(reqPower));

        reqPower = int(filtPower);
      
        // invert, positive values         
        if      (DoInv) reqPower = constrain(reqPower,0,i_maxpower);
        // charge, negative values
        else if (DoChg) reqPower = constrain(reqPower,c_maxpower,0);
        // do nothing
        else  reqPower = 0; // do zero    
           
        syncrxed = false;
      }
      
      essPower = short(reqPower);
      gotmsg = true; // make sure no timeout

      //storewp(meterPower,ACPower,reqPower); // red, blue, green
      storewp(meterPower,totalWatt,reqPower);      
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

bool gosleep;
bool wakeup;

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
          if (wakeup) // pull Stby Low on ve.bus!
          {
            sendmsg(4);
            wakeup = false;
          }
          else if (gosleep) 
          {
            sendmsg(3);
            gosleep = false;
          }
          else sendmsg(2);
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
