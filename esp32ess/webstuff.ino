#define num_datasets 480

struct wp
{
  int meterpow;
  int esspow;
  int mp2pow;
  int8_t state;
};

wp wp_arr[num_datasets];
uint16_t datasetpt = 0;

void storewp(int meter, int ess, int mp2)
{
  wp_arr[datasetpt].state = 1;
  wp_arr[datasetpt].meterpow = meter;
  wp_arr[datasetpt].esspow = ess;
  wp_arr[datasetpt].mp2pow = mp2;
  datasetpt++;
  if (datasetpt >= num_datasets) datasetpt = 0;
}

void clearwp()
{ 
  for(int i=0;i<num_datasets;i++) wp_arr[i].state = -1;
}

#define fullH 240
#define halfH 120
#define Hfact 0.12  // 500W -> 120

void drawData() 
{
  String out;
  out.reserve(5000);
  char temp[200];
  for (int i=0;i<num_datasets;i++)
  {
    uint16_t pos = datasetpt + i;
    if (pos >= num_datasets) pos -= num_datasets;
    int powdata = wp_arr[pos].meterpow;
    int essdata = wp_arr[pos].esspow;
    int mp3data = wp_arr[pos].mp2pow;
    if (wp_arr[pos].state >= 0)
    {
      sprintf(temp,"%4d, %4d, %4d<br>",powdata,essdata,mp3data);
      out += temp;
    }
  }
  server.send(200, "text/html", out);
}

void getGraph() 
{
  String out;
  char temp[100];
  
  out = "<html>\n";  
  out += "<head>\n";
  out += "<meta http-equiv='refresh' content='15'/>\n";
  out += "</head>\n";
    
  out += "<body><center>\n";
  out += "<h2>ESP32 Multiplus2 ESS</h2>\n";  
  
  out += "PowerMeter: "+String(meterPower)+" W &emsp;";
  out += "ESSPower: "+String(essPower)+" W &emsp;";
  out += "Bat Volt: "+String(BatVolt)+" V <br><br>";
  
  out += "<img src=\"/pic.svg\" />\n";
  out += "</center></body>\n";
  out += "</html>\n";
  
  server.send(200, "text/html", out);
}

void handleNotFound() 
{
  server.send(404, "text/plain", "Not Here");
}

void drawGraph() 
{
  String out;
  out.reserve(5000);
  char temp[200];

  out += "<svg viewBox=\"0 0 500 300\" xmlns=\"http://www.w3.org/2000/svg\">\n";
  out += "<rect x=\"10\" y=\"0\" width=\"480\" height=\"240\" stroke=\"silver\" fill=\"none\"/>\n";
  out += "<line x1=\"10\" y1=\"120\" x2=\"490\" y2=\"120\" stroke=\"black\"/>\n";

  // line graph red
  out += "<polyline points=\"";
  for (int i=0;i<num_datasets;i++)
  {
    // datasetpt is now, show the past
    uint16_t pos = datasetpt + i;
    if (pos >= num_datasets) pos -= num_datasets;
    int powdata = (float)wp_arr[pos].meterpow*Hfact;
    if (wp_arr[pos].state >= 0)
    {
      sprintf(temp,"%d,%d ",10+i,halfH-powdata);
      out += temp;
    }
  }
  out += "\" fill=\"none\" stroke=\"red\"/>\n";
    
  // line graph blue
  out += "<polyline points=\"";
  for (int i=0;i<num_datasets;i++)
  {
    // datasetpt is now, show the past
    uint16_t pos = datasetpt + i;
    if (pos >= num_datasets) pos -= num_datasets;
    int powdata = (float)wp_arr[pos].esspow*Hfact;
    if (wp_arr[pos].state >= 0)
    {
      sprintf(temp,"%d,%d ",10+i,halfH-powdata);
      out += temp;
    }
  }
  out += "\" fill=\"none\" stroke=\"blue\"/>\n";

  // line graph green
  out += "<polyline points=\"";
  for (int i=0;i<num_datasets;i++)
  {
    // datasetpt is now, show the past
    uint16_t pos = datasetpt + i;
    if (pos >= num_datasets) pos -= num_datasets;
    int powdata = (float)wp_arr[pos].mp2pow*Hfact;
    if (wp_arr[pos].state >= 0)
    {
      sprintf(temp,"%d,%d ",10+i,halfH-powdata);
      out += temp;
    }
  }
  out += "\" fill=\"none\" stroke=\"green\"/>\n";
  out += "</svg>\n";
  
  server.send(200, "image/svg+xml", out);
}

void shellyIP() 
{
  String out;
  char temp[100];
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if      (server.argName(i) == "sip")   Shelly_IP = server.arg(i);   
    else if (server.argName(i) == "ShelT") LoopInterval = 1000UL * constrain(server.arg(i).toInt(),1,60);
    else if (server.argName(i) == "InvHi") InvHi = server.arg(i).toInt();
    else if (server.argName(i) == "InvLo") InvLo = server.arg(i).toInt();
    else if (server.argName(i) == "ChgHi") ChgHi = server.arg(i).toInt();
    else if (server.argName(i) == "ChgLo") ChgLo = server.arg(i).toInt();
    else if (server.argName(i) == "TargetHi") TargetHi = server.arg(i).toInt();
    else if (server.argName(i) == "TargetLo") TargetLo = server.arg(i).toInt();
    else if (server.argName(i) == "LoBat") LoBat = server.arg(i).toFloat();
    else if (server.argName(i) == "HiBat") HiBat = server.arg(i).toFloat();
  }
   
  out += "<html><br><br><center>\n";

  if (shellyfail > 0) out += "Shelly fail: "+String(shellyfail)+"<br><br>"; 
  sprintf(temp,"Connected to BSSID &emsp; %02X %02X %02X %02X %02X %02X <br><br>\n",bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]); 
  out += temp;

  out += "<form method=\"post\">\n";
  out += "Shelly IP &emsp;\n";
  out += "<input type=\"text\" name=\"sip\" value=\"";
  out += Shelly_IP;
  out += "\"/>\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Shelly timing&emsp;\n";
  out += "<input type=\"number\" name=\"ShelT\" value=\"";
  out += LoopInterval/1000;
  out += "\">\n";  
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Inverter hysteresis band&emsp;\n";
  out += "<input type=\"number\" step=\"10\" name=\"InvHi\" value=\"";
  out += InvHi;
  out += "\">\n";
  out += "<input type=\"number\" step=\"10\" name=\"InvLo\" value=\"";
  out += InvLo;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Charger hysteresis band&emsp;\n";
  out += "<input type=\"number\" step=\"10\" name=\"ChgHi\" value=\"";
  out += ChgHi;
  out += "\">\n";
  out += "<input type=\"number\" step=\"10\" name=\"ChgLo\" value=\"";
  out += ChgLo;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Target Power Invert \n";
  out += "<input type=\"number\" name=\"TargetHi\" value=\"";
  out += TargetHi;
  out += "\">\n";
  out += "&emsp;Charge \n";
  out += "<input type=\"number\" name=\"TargetLo\" value=\"";
  out += TargetLo;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Low Bat \n";
  out += "<input type=\"number\" step=\"0.01\" name=\"LoBat\" value=\"";
  out += LoBat;
  out += "\">\n";
  out += "&emsp;Hi Bat \n";
  out += "<input type=\"number\" step=\"0.01\" name=\"HiBat\" value=\"";
  out += HiBat;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<a href=\"/\">Back</a>\n";
  out += "</center></html>";
  server.send(200, "text/html", out);
}

bool param;

void handleRoot() 
{
  char temp[10];
  String out;  
   
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if      (server.argName(i) == "ci")  {reqPower = server.arg(i).toInt(); gotmsg = true;}
    else if (server.argName(i) == "auto") autozero = true;
    else if (server.argName(i) == "manu"){autozero = false; reqPower = 0;}
    else if (server.argName(i) == "cho")  chgonly = !chgonly;
    else if (server.argName(i) == "ON")  wakeup  = true;
    else if (server.argName(i) == "OFF") gosleep = true;
    param = true;
  }
  
  out =  "<html>\n";
  if (param)
  {
    param = false;
    out += "<head>\n";
    out += "<meta http-equiv='refresh' content='1; url=/'/>\n";
    out += "</head>\n";
  }
  out += "<body><center>\n";
  out += "<p><b>ESP32 Multiplus ESS</b></p>";
  out += "<a href=\"/up\">Upload</a>&emsp;\n";
  out += "<a href=\"/graph\">Graph</a>&emsp;\n";
  out += "<a href=\"/data\">Data</a>&emsp;\n";
  out += "<a href=\"/shelly\">Settings</a>&emsp;\n";
  out += "<a href=\"/\">Refresh</a><br><br>\n";
  
  out += "PowerMeter: "+String(meterPower)+" W &emsp;";
  out += "ESSPower: "+String(essPower)+" W<br><br>";

  out += "Bat Volt: "+String(BatVolt)+" V&emsp;";
  out += "Bat Amp: "+String(multiplusDcCurrent)+" A&emsp;";
  out += "AC Power: "+String(ACPower)+" W<br><br>";
  if (batlow) out += "Bat Low Alarm !<br><br>";
  if (bathi) out += "Bat High Alarm !<br><br>";
  
  //out += "Temp: "+String(multiplusTemp)+" C&emsp;";

  if (nosync) out += "No Sync to Multiplus2<br><br>";
  
  out += "<button onclick=\"window.location.href='/?ci=-200\';\">charge 200</button>\n";
  out += "&emsp;\n";
  out += "<button onclick=\"window.location.href='/?ci=-1000\';\">charge 1000</button>\n";
  out += "&emsp;\n";
  out += "<button onclick=\"window.location.href='/?ci=0\';\">set 0</button>\n";
  out += "&emsp;\n";
  out += "<button onclick=\"window.location.href='/?ci=100\';\">invert 100</button>\n";
  out += "&emsp;\n";
  out += "<button onclick=\"window.location.href='/?ci=500\';\">invert 500</button>\n";
  out += "<br><br>\n";

  if (autozero)
    out += "<button onclick=\"window.location.href='/?manu=\';\">Manual</button>\n";
  else
    out += "<button onclick=\"window.location.href='/?auto=\';\">Auto</button>\n";
  out += "&emsp;\n";
  if (chgonly)
    out += "<button onclick=\"window.location.href='/?cho=\';\">Charge & Invert</button>\n";
  else
    out += "<button onclick=\"window.location.href='/?cho=\';\">Charge only</button>\n";
  out += "<br><br>\n";
  
  out += "<button onclick=\"window.location.href='/?ON=\';\">Wakeup</button>\n";
  out += "&emsp;\n";
  out += "<button onclick=\"window.location.href='/?OFF=\';\">Sleep</button>\n";
  out += "<br><br>\n";


  for (int i=0;i<rxnum;i++) 
  {
    sprintf(temp,"%02x ",frbuf2[i]);
    out += temp;
  }
  out += "<br><br>";

  for (int i=0;i<extframelen;i++) 
  {
    sprintf(temp,"%02x ",extframe[i]);
    out += temp;
  }
  out += "<br>";
  
  out += "</center></html>";
  server.send(200, "text/html", out);
}
