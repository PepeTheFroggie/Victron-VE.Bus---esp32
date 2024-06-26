#define num_datasets 600 // 600 * 5 sec

struct wp
{
  int red;
  int blue;
  int green;
  int8_t state;
};

wp wp_arr[num_datasets];
uint16_t datasetpt = 0;

void storewp(int red, int blue, int green)
{
  wp_arr[datasetpt].state  = 1;
  wp_arr[datasetpt].red    = red;
  wp_arr[datasetpt].blue   = blue;
  wp_arr[datasetpt].green  = green;
  datasetpt++;
  if (datasetpt >= num_datasets) datasetpt = 0;
}

void clearwp()
{ 
  for(int i=0;i<num_datasets;i++) wp_arr[i].state = -1;
}

#define fullH 300+110
#define halfH 150+10
#define Hfact 0.08  // 1875W -> 150

void getGraph() 
{
  String out;
  char temp[100];
  
  out = "<html>\n";  
  out += "<head>\n";
  out += "<meta http-equiv='refresh' content='15'/>\n";
  out += "</head>\n";
    
  out += "<body><center><br>\n";
  
  out += "PowerMeter: "+String(meterPower)+" W &emsp;";
  out += "ESSPower: "+String(essPower)+" W &emsp;";
  out += "Bat Volt: "+String(BatVolt)+" V &emsp;";
  out += "SoC: "+String(100.0*soc/totalAH)+" %<br>";
  //out += "CSerr " + String(chksmfault) + "<br>";
  
  out += "<img src=\"/pic.svg\" />\n";
  out += "</center></body>\n";
  out += "</html>\n";
  
  server.send(200, "text/html", out);
}

void onoff() 
{
  // "/cm?cmnd=Power%20on" "/cm?cmnd=Power%20off" 
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "cmnd") 
    {
      Serial.println(server.arg(i));
      if      (server.arg(i) == "Power on")  wakeup = true;
      else if (server.arg(i) == "Power off") gosleep = true;  
    }
  }
  if      (wakeup)  server.send(200, "text", "Wakeup");
  else if (gosleep) server.send(200, "text", "GoSleep");
  else server.send(200, "text", "Unknown");
}

void handleNotFound() 
{
  server.send(404, "text/plain", "Not Here");
}

void drawGraph() 
{
  String out;
  out.reserve(6000);
  char temp[200];

  // num_datasets + 20 / fullH + 20
  out += "<svg viewBox=\"0 0 620 320\" xmlns=\"http://www.w3.org/2000/svg\">\n";
  out += "<rect x=\"10\" y=\"10\" width=\"600\" height=\"300\" stroke=\"silver\" fill=\"none\"/>\n";
 
  out += "<line x1=\"10\" y1=\"160\" x2=\"610\" y2=\"160\" stroke=\"black\"/>\n";

  //<text x="20" y="35" class="small">My</text>
  
  // line graph red
  out += "<polyline points=\"";
  for (int i=0;i<num_datasets;i++)
  {
    // datasetpt is now, show the past
    uint16_t pos = datasetpt + i;
    if (pos >= num_datasets) pos -= num_datasets;
    int powdata = (float)wp_arr[pos].red*Hfact;
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
    int powdata = (float)wp_arr[pos].blue*Hfact;
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
    int powdata = (float)wp_arr[pos].green*Hfact;
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
    else if (server.argName(i) == "ShelT") LoopInterval = 1000UL * constrain(server.arg(i).toInt(),1,59);
    else if (server.argName(i) == "InvHi") InvHi = server.arg(i).toInt();
    else if (server.argName(i) == "InvLo") InvLo = server.arg(i).toInt();
    else if (server.argName(i) == "ChgHi") ChgHi = server.arg(i).toInt();
    else if (server.argName(i) == "ChgLo") ChgLo = server.arg(i).toInt();
    else if (server.argName(i) == "TargetHi") TargetHi = server.arg(i).toInt();
    else if (server.argName(i) == "TargetLo") TargetLo = server.arg(i).toInt();
    else if (server.argName(i) == "LoBat") AbsLoBat = server.arg(i).toFloat();
    else if (server.argName(i) == "HiBat") AbsHiBat = server.arg(i).toFloat();
    else if (server.argName(i) == "LoSoc") LOsoc = server.arg(i).toFloat();
    else if (server.argName(i) == "HiSoc") HIsoc = server.arg(i).toFloat();
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
  out += "<input type=\"number\" name=\"ShelT\" style=\"width:3em\" value=\"";  
  out += LoopInterval/1000;
  out += "\">\n";  
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Inverter hysteresis band&emsp;\n";
  out += "<input type=\"number\" step=\"5\" name=\"InvHi\" style=\"width:5em\" value=\"";
  out += InvHi;
  out += "\">\n";
  out += "<input type=\"number\" step=\"5\" name=\"InvLo\" style=\"width:5em\" value=\"";
  out += InvLo;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Charger hysteresis band&emsp;\n";
  out += "<input type=\"number\" step=\"5\" name=\"ChgHi\" style=\"width:5em\" value=\"";
  out += ChgHi;
  out += "\">\n";
  out += "<input type=\"number\" step=\"5\" name=\"ChgLo\" style=\"width:5em\" value=\"";
  out += ChgLo;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Target Power Invert \n";
  out += "<input type=\"number\" name=\"TargetHi\" style=\"width:5em\" value=\"";
  out += TargetHi;
  out += "\">\n";
  out += "&emsp;Charge \n";
  out += "<input type=\"number\" name=\"TargetLo\" style=\"width:5em\" value=\"";
  out += TargetLo;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Low Bat \n";
  out += "<input type=\"number\" step=\"0.01\" name=\"LoBat\" style=\"width:8em\" value=\"";
  out += AbsLoBat;
  out += "\">\n";
  out += "&emsp;Hi Bat \n";
  out += "<input type=\"number\" step=\"0.01\" name=\"HiBat\" style=\"width:8em\" value=\"";
  out += AbsHiBat;
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<form method=\"post\">\n";
  out += "Low SoC % \n";
  out += "<input type=\"number\" name=\"LoSoc\" style=\"width:6em\" value=\"";
  out += int(LOsoc);
  out += "\">\n";
  out += "&emsp;Hi SoC % \n";
  out += "<input type=\"number\" name=\"HiSoc\" style=\"width:6em\" value=\"";
  out += int(HIsoc);
  out += "\">\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";

  out += "<button onclick=\"window.location.href='/?wrset=\';\">Write Settings</button>&emsp;\n";
  out += "<button onclick=\"window.location.href='/?rdset=\';\">Read Settings</button><br><br>\n";

  out += "<a href=\"/soc\">SoC</a>&emsp;\n";
  out += "<a href=\"/data\">Data</a>&emsp;\n";
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
    else if (server.argName(i) == "auto"){autozero = true; reqPower=0;}
    else if (server.argName(i) == "cho")  chgonly = !chgonly;
    else if (server.argName(i) == "ON")   wakeup  = true;
    else if (server.argName(i) == "OFF")  gosleep = true;
    else if (server.argName(i) == "AON")  autowakeup = !autowakeup;
    else if (server.argName(i) == "manu"){autozero=false; reqPower=0; batlow=false; bathi=false;}
    else if (server.argName(i) == "wrset") writeeprom();
    else if (server.argName(i) == "rdset") readeeprom();
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
//out += "<a href=\"/data\">Data</a>&emsp;\n";
  out += "<a href=\"/shelly\">Settings</a>&emsp;\n";
  out += "<a href=\"/\">Refresh</a><br><br>\n";
  
  out += "PowerMeter: "+String(meterPower)+" W &emsp;";
  if (isSleeping) out += "IS SLEEPING&emsp;";
  out += "AC Power: "+String(ACPower)+" W&emsp;";
  out += "ESSPower: "+String(essPower)+" W<br><br>";

  out += "Bat Volt: "+String(BatVolt)+" V&emsp;";
  out += "EstBatVolt: "+String(estvolt)+" V&emsp;";
  out += "Bat Amp: "+String(multiplusDcCurrent)+" A&emsp;";
  out += "SoC: "+String(100.0*soc/totalAH)+" %<br><br>";
  if (batlow) out += "Bat Low Alarm !<br><br>";
  if (bathi) out += "Bat High Alarm !<br><br>";
  
  //out += "Temp: "+String(multiplusTemp)+" C&emsp;";

  if (nosync) out += "No Sync to Multiplus2<br><br>";

  if (!autozero)
  {
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
  }
  
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
  out += "&emsp;\n";
  if (autowakeup)
    out += "<button onclick=\"window.location.href='/?AON=\';\">NoAutoWakeup</button>\n";
  else
    out += "<button onclick=\"window.location.href='/?AON=\';\">AutoWakeup</button>\n";
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
