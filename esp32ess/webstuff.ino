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
    if      (server.argName(i) == "sip") Shelly_IP = server.arg(i);   
    else if (server.argName(i) == "ShelT") LoopInterval = 1000UL * constrain(server.arg(i).toInt(),1,60);
    else if (server.argName(i) == "KP") Kp = constrain(server.arg(i).toFloat(),0.0,1.0);
    else if (server.argName(i) == "KD") Kd = constrain(server.arg(i).toFloat(),0.0,1.0);
    else if (server.argName(i) == "KI") Ki = constrain(server.arg(i).toFloat(),0.0,1.0);
  }
   
  out += "<html><br><br><center>\n";

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
  out += "KP&emsp;\n";
  out += "<input type=\"number\" name=\"KP\" value=\"";
  out += Kp;
  out += "\">\n";

  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";
  
  sprintf(temp,"BSSID: %02X %02X %02X %02X %02X %02X <br><br>\n",bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]); 
  out += temp;

  out += "<a href=\"/\">Back</a>\n";
  out += "</center></html>";
  server.send(200, "text/html", out);
}

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
  }
  
  out =  "<html>\n";  
  out += "<head>\n";
  out += "<meta http-equiv='refresh' content='10'/>\n";
  out += "</head>\n";

  out += "<body><center>\n";
  out += "<p><b>ESP32 Multiplus ESS</b></p>";
  out += "<a href=\"/up\">Upload</a>&emsp;\n";
  out += "<a href=\"/graph\">Graph</a>&emsp;\n";
  out += "<a href=\"/data\">Data</a>&emsp;\n";
  out += "<a href=\"/shelly\">Settings</a>&emsp;\n";
  out += "<a href=\"/\">Refresh</a><br><br>\n";
  
  out += "PowerMeter: "+String(meterPower)+" W &emsp;";
  out += "ESSPower: "+String(essPower)+" W<br><br>";

  out += "Bat Volt: "+String(0.01*float(BatVolt))+" V&emsp;";
  out += "Bat Amp: "+String(0.1*float(BatAmp))+" A&emsp;";
  out += "AC Power: "+String(ACPower)+" W<br><br>";

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

  for (int i=0;i<rxnum;i++) 
  {
    sprintf(temp,"%02x ",frbuf2[i]);
    out += temp;
  }
  out += "<br><br>";
/*
  for (int i=0;i<extframelen;i++) 
  {
    sprintf(temp,"%02x ",extframe[i]);
    out += temp;
  }
  out += "<br>";
*/  

  out += "</center></html>";
  server.send(200, "text/html", out);
}
