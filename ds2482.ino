
/*-----( Declare User-written Functions )-----*/

// -----------------------------------------------------------------------

// function to print a device address --------
void printAddress(DeviceAddress deviceAddress)
{

// should look like this :    { 0x28, 0xE1, 0xC7, 0x40, 0x04, 0x00, 0x00, 0x0D };

  Serial.print(deviceAddress[0], HEX);
  Serial.print(" # ");          // { Device Typ ....
  

  Serial.print("{ ");          // { befor ....
  
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print("0x");                   // 0x befor ....
    if (deviceAddress[i] < 16) Serial.print("0");   // 
    Serial.print(deviceAddress[i], HEX);
    if (i < 7) Serial.print(", ");        // , between .... 
  }
  Serial.print(" }");          // } after ....
 
}
// function to print a device address --------









// -----------------------------------------------------------------------

