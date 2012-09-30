void toggleAll(char who, boolean on)
{
  char code[12];
  char *OO = "F";
  if(!on) OO = "0";
  if(who == '2' || who == 'o' || who == 'f'){
    mySwitch.sendTriState(strcat(strcpy(code, "0F0000000FF"),OO));
    delay(200);
    mySwitch.sendTriState(strcat(strcpy(code, "0F00F0000FF"),OO));
    delay(200);
    mySwitch.sendTriState(strcat(strcpy(code, "FF0000000FF"),OO));
    delay(100);
  }
  if(who == '1' || who == 'f' || who == 'o') {
    extra_outlet(0,0,1);
  }
}


/*************************************************************
 * dev: device code
 * stat: 1=on, 0=off, >1=dim time
 * first_floor: 0=second floor, 1=first floor
 *************************************************************/
void extra_outlet(int dev, int stat, boolean first_floor) {
  char code[] = {
    "11111"  } 
  ;
  if(first_floor) code[0] = '0';
  if(dev == 0) { //Turn all off/on (usually when called from toggleAll(...))
    int i = 1;
    for(;i<=5;i++) {
      if(stat == 1)
        mySwitch.switchOn(code, i);
      else
        mySwitch.switchOff(code, i);
      delay(100);
    }
  } 
  else {
    if (stat == 1)
      mySwitch.switchOn(code, dev);
    else if(stat == 0)
      mySwitch.switchOff(code, dev);
    else {  //some dim value
      int i = 0;
      for(;i<stat;i++)
        mySwitch.switchOn(code, dev);
    }
  }
}

