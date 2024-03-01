#include <EEPROM.h>

// EEPROM.write(address, value);
// EEPROM.commit();
// EEPROM.read(address);

#define EEPROM_SIZE 64

bool initeeprom()
{
  EEPROM.begin(EEPROM_SIZE);
  return (EEPROM.read(0) == 0xAA);
}

void readeeprom()
{
  EEPROM.get(4,InvHi);
  EEPROM.get(8,InvLo);
  EEPROM.get(12,ChgHi);
  EEPROM.get(16,ChgLo);
  EEPROM.get(20,TargetHi);
  EEPROM.get(24,TargetLo);
  EEPROM.get(28,LoBat);
  EEPROM.get(32,HiBat);  
  EEPROM.get(36,LoopInterval);
  LoopInterval = constrain(LoopInterval,1,60);
}

void writeeprom()
{
  EEPROM.write(0, 0xAA);
  EEPROM.put(4,InvHi);
  EEPROM.put(8,InvLo);
  EEPROM.put(12,ChgHi);
  EEPROM.put(16,ChgLo);
  EEPROM.put(20,TargetHi);
  EEPROM.put(24,TargetLo);
  EEPROM.put(28,LoBat);
  EEPROM.put(32,HiBat);    
  EEPROM.put(36,LoopInterval);
  EEPROM.commit();
}
