/* Autor Michal Zachwieja 2011 v0.7 
 mod. by DonC 2016.08
*/


#ifndef ds2413_h
#define ds2413_h

#include <OneWire.h>

#include <Arduino.h>

class ds2413
{
  public:
  ds2413(OneWire*);
  void setAdr(byte adr[8]);
  void read(byte& A, byte& B);
  void write(boolean A, boolean B);
  byte addr[8];
  
  private: 
  OneWire* _wire;
};

#endif