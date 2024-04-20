
float totalAH = 305.0;
float soc = totalAH/2.0;

const float StoH = 1.0/3600.0; // sec to hr

void updateSoc(float amp, int dt) // dt in msec
{
  float mdt = 0.001 * StoH * float(dt);
  soc = constrain(soc + amp * mdt, 0.0, totalAH); 
}

void doSoc() 
{
  String out;

  for (uint8_t i = 0; i < server.args(); i++)
  {
    if      (server.argName(i) == "soc") soc = server.arg(i).toFloat();
    else if (server.argName(i) == "tah") totalAH = server.arg(i).toFloat();
  }
  
  out =  "<!doctype html>\n";
  out += "<html><body><center>\n";  
  out += "<br>";
  out += "<p><b>SoC</b></p>";

  out += "SOC: "+String(soc)+" AH &emsp;";
  out += String(100.0*soc/totalAH)+" %";
  out += "<br><br>";
  
  out += "<form method=\"post\">\n";
  out += "Bat AH&emsp;\n";
  out += "<input type=\"number\" name=\"tah\" style=\"width:5em\" value=\"";
  out += int(totalAH);
  out += "\">&emsp;\n";
  out += "SoC&emsp;\n";
  out += "<input type=\"number\" name=\"soc\" style=\"width:5em\" value=\"";
  out += int(soc);
  out += "\">&emsp;\n";
  out += "<input type=\"submit\"><br>\n";
  out += "</form>\n";
  out += "<br>";
  
  out += "<a href=\"/\">Back</a>\n";
  out += "</center></html>";
  server.send(200, "text/html", out);    
}

#define soc_datasets 100

struct mysoc
{
  float volt;
  float amp;
  float act;
};

mysoc mysoc_arr[soc_datasets];
uint16_t socdatasetpt = 0;

void storeData(float volt, float amp, float act)
{
  mysoc_arr[socdatasetpt].volt = volt;
  mysoc_arr[socdatasetpt].amp  = amp;
  mysoc_arr[socdatasetpt].act  = act;
  socdatasetpt++;
  if (socdatasetpt >= soc_datasets) socdatasetpt = 0;
}

void doData() 
{
  String out;
  out =  "<!doctype html>\n";
  out += "<html><body><center>\n";  
  out += "<br>";
  
  for (int i=0;i<soc_datasets;i++)
  {
    uint16_t pos = socdatasetpt + i;
    if (pos >= soc_datasets) pos -= soc_datasets;
    if (mysoc_arr[pos].act != 0.0)
    {
      out += String(mysoc_arr[pos].volt) + "  ";
      out += String(mysoc_arr[pos].act) + "  ";
      out += String(mysoc_arr[pos].amp) + "<br>";
    }
  }
  
  out += "<br>";
  out += "<a href=\"/\">Back</a>\n";
  out += "</center></html>";
  server.send(200, "text/html", out);    
}
