//#include <Preferences.h>
//Preferences preferences;

#include "EEPROM.h"

EEPROMClass  SETTINGS("eeprom0");

void initeeprom()
{
  if (!SETTINGS.begin(64)) Serial.println("Failed to initialise SETTINGS");
}

void readeeprom()
{
  InvHi    = SETTINGS.readInt( 0);
  InvLo    = SETTINGS.readInt( 4);
  ChgHi    = SETTINGS.readInt( 8);
  ChgLo    = SETTINGS.readInt(12);
  TargetHi = SETTINGS.readInt(16);
  TargetLo = SETTINGS.readInt(20);
  AbsLoBat = SETTINGS.readFloat(24);
  AbsHiBat = SETTINGS.readFloat(28);
  LoopInterval = SETTINGS.readULong(32);
  Shelly_IP    = SETTINGS.readString(40);
}

void writeeprom()
{
  SETTINGS.writeInt( 0,InvHi);
  SETTINGS.writeInt( 4,InvLo);
  SETTINGS.writeInt( 8,ChgHi);
  SETTINGS.writeInt(12,ChgLo);
  SETTINGS.writeInt(16,TargetHi);
  SETTINGS.writeInt(20,TargetLo);
  SETTINGS.writeFloat(24,AbsLoBat);
  SETTINGS.writeFloat(28,AbsHiBat);
  SETTINGS.writeULong(32,LoopInterval);
  SETTINGS.writeString(40,Shelly_IP);

  SETTINGS.commit();
}
