/* Autor Michal Zachwieja 2011 v0.7 
*/


#include <Arduino.h>
#include <OneWire.h>
#include <ds2413.h>

 ds2413::ds2413(OneWire* _oneWire)
{
//_OW_pin = OW_pin;
_wire = _oneWire;
}

 void ds2413::setAdr(byte adr[8])
  {
  for(byte i=0;i<=7;i++) addr[i]=adr[i];  
  
  }
  void ds2413::read(byte& A, byte& B)
  {
	byte data;
	_wire->reset();				//reset bus
	_wire->select(addr);	   	//select device 
	_wire->write(0xF5);
	data=_wire->read();
	A=data<<7;
	A=A>>7;

	B=data<<5;
	B=B>>7;

	_wire->reset();	
  }
  
  void ds2413::write(boolean A, boolean B)
  {
  
    byte data[2];
	byte cmd;
 
 
	if(B) cmd=0xFE; else cmd=0xFC;
	if(A) cmd=cmd|1;else cmd=cmd|0;
 
	_wire->reset();				//reset bus
	_wire->select(addr);	   	//select device 
	_wire->write(0x5A);	   		//send PIO access drive command
	_wire->write(cmd);	   		//write new pin status 
	_wire->write(~cmd);		  	//The inversion of previous data byte
	data[0] = _wire->read(); 	//Read AA confirmation byte; here we don't care
	data[1] = _wire->read(); 	//Read Pin Status byte; here we don't care
 
  }
  
