ISR(TIMER1_OVF_vect)
{
  overflow_count++;
  if(overflow_count > 143) { //143*4,194304s = 599,785472s = ~10 minutes
    TCCR1B = 0;			//stop timer
    overflow_count = 255;		//set to ready
  }
}

void check_temp() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  if((ovn_on && (temp > high_temp || temp < -120)) ||  force_on == 2) { //when DallasTemperature? fails, it(the library?) returns -127
    digitalWrite(LED_PIN, LOW);
    mySwitch.switchOff('d', 1, 3);
    ovn_on = false;
    force_on = 0;
  }
  else if((!ovn_on && temp < low_temp && temp > -120) || force_on == 1) {
    digitalWrite(LED_PIN, HIGH);
    mySwitch.switchOn('d', 1, 3);
    ovn_on = true;
    force_on = 0;
  }
}

