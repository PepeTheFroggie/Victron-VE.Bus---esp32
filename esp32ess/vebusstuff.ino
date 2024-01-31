// mp2 variables
byte    masterMultiLED_LEDon;              //Bits 0..7 = mains on, absorption, bulk, float, inverter on, overload, low battery, temperature
byte    masterMultiLED_LEDblink;           //(LEDon=1 && LEDblink=1) = blinking; (LEDon=0 && LEDblink=1) = blinking_inverted 
byte    masterMultiLED_Status;             //0=ok, 2=battery low
byte    masterMultiLED_AcInputConfiguration;
float   masterMultiLED_MinimumInputCurrentLimit;
float   masterMultiLED_MaximumInputCurrentLimit;  
float   masterMultiLED_ActualInputCurrentLimit;
byte    masterMultiLED_SwitchRegister;
float   multiplusTemp;
float   multiplusDcCurrent;
int16_t multiplusAh;
byte    multiplusStatus80;                 //status from the charger/inverter frame 0x80: 0=ok, 2=battery low
bool    multiplusDcLevelAllowsInverting;

float BatVolt,BatAmp;
int16_t ACPower;

//Other
const int VEBUS_RXD1=16, VEBUS_TXD1=17, VEBUS_DE=4;  //Victron Multiplus VE.bus RS485 gpio pins

//other variables:
char frbuf1[128];    //assembles one complete frame received by Multiplus
char frbuf2[128];    //assembles one complete frame received by Multiplus
char txbuf1[32];     //buffer for assembling bare command towards Multiplus (without replacements or checksum)
char txbuf2[32];     //Multiplus output buffer containing the final command towards Multiplus, including replacements and checksum
byte rxnum;                    
byte frp = 0;        //Pointer into Multiplus framebuffer frbuf[] to store received frames.
byte frlen = 0;      //Pointer into Multiplus framebuffer
byte frameNr = 0;    //Last frame number received from Multiplus. Own command has be be sent with frameNr+1, otherwise it will be ignored by Multiplus.

void init_vebus()
{
  pinMode(VEBUS_DE, OUTPUT);    //RS485 RE/DE direction pin for UART1
  digitalWrite(VEBUS_DE,LOW);   //set RS485 direction to read

  //Setup Serial port for VE.Bus RS485 to Multiplus
  Serial1.begin(256000, SERIAL_8N1, VEBUS_RXD1, VEBUS_TXD1);
}

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

int cmdOnOff(char *outbuf, byte desiredFrameNr, bool doon)
{
  // 98F7 FE 60 3F07 0000005DFF wakeup
  // 98F7 FE 6F 3F04 00000051FF sleep
  byte j=0;
  outbuf[j++] = 0x98;           //MK3 interface to Multiplus
  outbuf[j++] = 0xf7;           //MK3 interface to Multiplus
  outbuf[j++] = 0xfe;           //data frame
  outbuf[j++] = desiredFrameNr;
  outbuf[j++] = 0x3F;           //cmd
  if (doon) outbuf[j++] = 0x07; //wakeup
  else      outbuf[j++] = 0x04; //sleep
  outbuf[j++] = 0x00;           
  outbuf[j++] = 0x00;           
  outbuf[j++] = 0x00;           
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
  if (msgtype == 3)
    len = cmdOnOff(txbuf1, (frameNr+1) & 0x7F, false);   
  if (msgtype == 4)
    len = cmdOnOff(txbuf1, (frameNr+1) & 0x7F, true);   
  len = commandReplaceFAtoFF(txbuf2, txbuf1, len);
  len = appendChecksum(txbuf2, len);
  //write command into Multiplus :-)
  digitalWrite(VEBUS_DE,HIGH);      //set RS485 direction to write
  Serial1.write(txbuf2, len); //write command bytes to UART
  Serial1.flush(); //
  digitalWrite(VEBUS_DE,LOW);   //set RS485 direction to read
}

void decodeVEbusFrame(char *frame, int len)
{
  // data frame
  if      (frame[4] == 0x80) // 80 = Condition of Charger/Inverter (Temp+Current)
  { 
    if ((frame[5]==0x80) && ((frame[6]&0xFE)==0x12) && (frame[8]==0x80) && ((frame[11]&0x10)==0x10))
    {
      multiplusStatus80 = frame[7];
      multiplusDcLevelAllowsInverting = (frame[6] & 0x01);
      int16_t t = 256*frame[10] + frame[9];
      multiplusDcCurrent = 0.1*t;
      if ((frame[11] & 0xF0) == 0x30) multiplusTemp = 0.1*frame[15];
    }
  }
  else if (frame[4] == 0xE4) // E4 = AC phase information (comes with 50Hz)
  { 
    // 83 83 fe 51 e4  80 56 c3 c3 a6 4c be 8f d3 68 19 4b 7a 00  1a ff 
    // 83 83 fe 68 e4  2b 46 c3 9c 68 31 be 8f d8 68 19 4b 7a 00  e3 ff
    // 83 83 fe 3d e4  13 5e c3 c4 dc 39 be 8f 7d 68 0b 4b 7a 00  d3 ff
  }
  else if (frame[4] == 0x70) // 70 = DC capacity counter)
  {
    for (int i=0;i<len;i++) extframe[i] = frame[i];
    extframelen = len;      
  }
  else if (frame[4] == 0x41) // 41 = Multiplus mode / master led
  {
    if ((len==19) && (frame[5]==0x10))    //frame[5] unknown byte 
    {
      masterMultiLED_LEDon = frame[6];
      masterMultiLED_LEDblink = frame[7];
      masterMultiLED_Status = frame[8];
      masterMultiLED_AcInputConfiguration = frame[9];
      int16_t t = 256*frame[11] + frame[10];
      masterMultiLED_MinimumInputCurrentLimit = t/10.0;
      t = 256*frame[13] + frame[12];
      masterMultiLED_MaximumInputCurrentLimit = t/10.0;
      t = 256*frame[15] + frame[14];
      masterMultiLED_ActualInputCurrentLimit = t/10.0;
      masterMultiLED_SwitchRegister = frame[16];
    }
  }
  else if (frame[4] == 0x38) // ?
  {
    // 83 83 fe 05 38 01 c0 c0 45 ff   
  }
  else if (frame[4] == 0x00) // ack or response
  {
    if (frame[5] == 0xE6)
    { 
      if      (frame[6] == 0x87) acked = true;
      else if (frame[6] == 0x85) 
      {
        gotMP2data = true;
        int16_t v = 256*frame[8]  + frame[7];
        BatVolt = 0.01*float(v);
        int16_t a = 256*frame[10] + frame[9];
        BatAmp  = 0.1*float(a);
        ACPower = 256*frame[12] + frame[11];
      }
      else
      {
      }
    }
  }
  else
  {
  }
}

void multiplusCommandHandling()
{
  //Check for new bytes on UART
  while (Serial1.available())
  {
    char c = Serial1.read();       //read one byte
    frbuf1[frp++] = c;   //store into framebuffer
    if (c==0x55) { if (frp == 5) synctime = millis(); }
    if (c==0xFF)        //in case current byte was EndOfFrame, interprete frame
    {
      frlen = destuffFAtoFF(frbuf2,frbuf1,frp);
      if ((frbuf2[2] == 0xFD) && (frbuf2[4] == 0x55))  //if it was a sync frame:
      {
        frameNr = frbuf2[3];
        syncrxed = true;
      }
      if ((frbuf2[0] == 0x83) && (frbuf2[1] == 0x83) && (frbuf2[2] == 0xFE))
      {
        decodeVEbusFrame(frbuf2, frlen);
      }
      rxnum = frlen;
      frp = 0;
    }
    else syncrxed = false; // unexpected char received
  }
}
