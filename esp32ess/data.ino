
#define num_data 100

// 52.22, 0.00
// 52.02, -6.80  - 0.2  -> 6.8A  0.030  ohm
// 51.96, -7.90  - 0.26 -> 7.9A  0.032 ohm
// 51.78, -13.80 - 0.44 -> 12.8A 0.034 ohm
// 52.14, 0.00

struct dp
{
  float red;
  float blue;
  int8_t state;
};

dp dp_arr[num_data];
uint16_t datapt = 0;

void storedp(float volt, float amps)
{
  dp_arr[datapt].state  = 1;
  dp_arr[datapt].red    = volt;
  dp_arr[datapt].blue   = amps;
  datapt++;
  if (datapt >= num_data) datapt = 0;
}

void drawData() 
{
  String out;
  out.reserve(2000);
  char temp[100];
  out = "Volt  Amps<br>";
  for (int i=0;i<num_data;i++)
  {
    uint16_t pos = datapt + i;
    if (pos >= num_data) pos -= num_data;
    if (dp_arr[pos].state > 0)
    {
      sprintf(temp,"%.2f, %.2f<br>",dp_arr[pos].red, dp_arr[pos].blue);
      out += temp;
    }
  }
  server.send(200, "text/html", out);
}
