ISR(TIMER1_OVF_vect)
{
    overflow_count++;
    if(overflow_count > overflow_offset) { //143*4,194304s = 599,785472s = ~10 minutes
        TCCR1B = 0;			//stop timer
        overflow_count = 65535;		//set to ready
    }
}


/* on_timer_function() 
 * updates the sensor /sensors-variables,
 * and regulate temperature based on mode.
 * mode 1 = heating, mode 2 = cooling
 * NOTE: When DallasTemperature fails, it(the library?) returns -127
 */
void on_timer_function() {
    DHT11.read(DHT11PIN);
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);

    if(ovn_aktiv){
        if(ovn_on || force_on == 2)
        {
            if((ovn_mode == 0 && (temp > high_temp || temp < -120)) 
            || (ovn_mode == 1 && temp < low_temp)
            || force_on == 2)
            {
                digitalWrite(LED_PIN, LOW);
                mySwitch.switchOff('d', 1, 3);
                ovn_on = false;
                force_on = 0;
            }
        }
        else if(!ovn_on || force_on == 1)
        {
            if((ovn_mode == 0 && temp < low_temp && temp > -120)
            || (ovn_mode == 1 && temp > high_temp)
            || force_on == 1)
            {
                digitalWrite(LED_PIN, HIGH);
                mySwitch.switchOn('d', 1, 3);
                ovn_on = true;
                force_on = 0;
            }
        }
    }
}