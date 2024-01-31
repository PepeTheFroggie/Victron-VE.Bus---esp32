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
float expectedPower = 0.0; // expected power of multiplus

// PID factor
float Kp = 0.94;
float Kd = 0.0;
float Ki = 0.0;

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
        //gotMP2data = false; //if comm not reliable
        if (gotMP2data)
        {
          gotMP2data = false;
          totalWatt = Kp * (meterPower - ACPower);
        }
        else
        {
          //ACPower = 0;
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
