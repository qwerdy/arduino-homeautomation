#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>        //watchdog
#include <dht11.h>
#include <RCSwitch.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <IRremote.h>
//#include <JeeLib.h>



#define ONE_WIRE_BUS 8
#define BUFFER_SIZE 500
#define DIMTIME 2345   // Time to dim a light. us
#define DOORBELL_PIN 7
#define LED_PIN 4   //digital
#define LIGHT_PIN A1 //analog
#define SENSOR_PIN A0//analog
#define TEMP_PRESCALE 5.0       // max-temp = 255/TEMP_PRESCALE, accuracy will be max-temp/255
#define VERSION "1.0"
#define DHT11PIN 6 //digital
#define PORT 80 //ethernet

#define DEBUG 0 // prints free ram on webpage


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
dht11 DHT11;



typedef struct {
  unsigned long id;
  int value;
} Payload;
 
Payload payload;



boolean   ovn_aktiv         = false;
boolean   ovn_on            = false;
uint8_t   force_on          = false; // 0=nothing, 1=force on, 2=force off
boolean   notify            = false;
boolean   doorbell_on       = false;
boolean   light_auto        = false;
uint8_t   light_auto_on     = 0; // 0=off, 2=Activate timer on next movement, 4=On with timer
float     low_temp          = 19.5;
float     high_temp         = 22.5;
uint32_t  lastBellring      = 0;
uint32_t  lastMovement      = 0;
uint16_t  overflow_count    = 0;
uint16_t  overflow_offset   = 143;
uint16_t  sensor_count      = 0;
uint32_t  light_wait_msec   = 15000;
boolean   sensor_last       = false;
uint8_t   ovn_mode          = 0; // 0=heat, 1=cooling
uint8_t   light_limit_auto  = 0;


//Server settings:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1, 115);
EthernetServer server(PORT);
EthernetClient client;
#define   textBuffSize 63
char      textBuff[textBuffSize+1];

//UDP settings:
char packetBuffer[UDP_TX_PACKET_MAX_SIZE+1]; //buffer to hold incoming packet

EthernetUDP Udp;

//Pop client settings:
IPAddress popip(192,168,1, 100);
EthernetClient popserver; 


// RCswitch
RCSwitch mySwitch = RCSwitch();


/* Count watchdog resets */
volatile uint8_t wd_resets __attribute__ ((section (".noinit"))); 


void setup () 
{
    sensors.begin();
    //rf12_initialize(1, RF12_433MHZ, 23);
    mySwitch.enableTransmit(A5);
    // Timer test stuff -------------
    TCNT1  = 0;
    TCCR1A = 0;
    TIMSK1 = 0x01; //enable overflow interrupt
    //-------------------------------
    pinMode(DOORBELL_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LIGHT_PIN, INPUT);

    ovn_aktiv         = eeprom_read_byte((unsigned byte *) 0);
    low_temp          = eeprom_read_byte((unsigned byte *) 1)/TEMP_PRESCALE;
    high_temp         = eeprom_read_byte((unsigned byte *) 2)/TEMP_PRESCALE;
    doorbell_on       = eeprom_read_byte((unsigned byte *) 3);
    ovn_mode          = eeprom_read_byte((unsigned byte *) 4);
    overflow_offset   = eeprom_read_word((unsigned word *) 5); /* WORD (2 bytes) */
    light_auto        = eeprom_read_byte((unsigned byte *) 7);
    light_wait_msec   = eeprom_read_word((unsigned word *) 8)*1000; /* WORD (2 bytes) */
    light_limit_auto  = eeprom_read_byte((unsigned byte *) 10);

    if(ovn_aktiv) { 
        /* Turn off ovn, so we know its state, since default ovn_on = false */
        mySwitch.switchOff('d', 1, 3);
    }
  
    Ethernet.begin(mac, ip);
    server.begin();
    Udp.begin(PORT);
    //Simple overflow preventing on print.
    textBuff[textBuffSize] = '\0';
    packetBuffer[UDP_TX_PACKET_MAX_SIZE] = '\0';
    
    /* Start timer */
    TCCR1B = 0x05;

    /* Enable WatchDog */
    if ((MCUSR & (1<<WDRF))==(1<<WDRF)) //Reset caused by wdt
        wd_resets++; //count resets
    else
      wd_resets = 0;
    wdt_enable(WDTO_8S);

    /* Run timer function on start to set sensors variables */
    on_timer_function();
}

void loop () {
/* Remove function overhead */
while(true) 
{
    wdt_reset();    //reset watchdog
    client = server.available();
    if (client.connected() && client.available()) {
        int charsWaiting = client.available();
        uint8_t charsReceived = 0;
        char c;
        do {
            c = client.read();
            textBuff[charsReceived] = c;
            charsReceived++;
            charsWaiting--;
        }
        while(charsReceived < textBuffSize && charsWaiting > 0);

        if(strncmp("GET /", textBuff, 5) == 0) { //if (textBuff[0] == 'G')
            if(textBuff[5] == ' ') { // Get root
                homePage(); // send web page data
                // homePage includes client.stop()
                //end_TCP(true);
            } else {//other than root, deny!  (favicon for most)
                client.stop();
            }
        }
        else if(textBuff[0] == '!') {
          //multicommand mode
          //takes string in form of e.g: !Oz;5&;rz;
          //  (turn led light on, wait 5ms, turn led red)
          client.print('!');
          uint8_t cmd_pos = 1; // skip first !
          uint8_t pos = 1;     // skip first !
          uint8_t cmd_length = 0;

          while(--charsReceived) {
            if(textBuff[pos] == ';') { //our command delimiter
              job_switch(&textBuff[cmd_pos], cmd_length, true);
              cmd_pos = pos+1;
              cmd_length = 0;
            } else {
               cmd_length++;
            }
            pos++;
          }
          client.stop(); //stop incase no command was executed
        }
        else {
            job_switch(textBuff, charsReceived, true);
        }
    }
    else if(overflow_count == 65535) { // Check temp, doing it this way so it wont delay ethernet too much!
        on_timer_function();
        overflow_count = 0;
        TCCR1B = 0x05; //start timer again
    }

    int packetSize = Udp.parsePacket();
    if(packetSize) {
        // read the packet into packetBufffer
        Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);

        job_switch(packetBuffer, packetSize, false);
        // send a reply, to the IP address and port that sent us the packet we received
        //Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        //Udp.write(ReplyBuffer);
        //Udp.endPacket();
    }
    
    if(doorbell_on && digitalRead(DOORBELL_PIN) == HIGH && (millis() - lastBellring > 5000)) {
        lastBellring = millis();
        delay(100); //remove noise ?!
        doorbell();
    }


    boolean buffRead = digitalRead(SENSOR_PIN);
    if(buffRead != sensor_last) { //count sensor toggles
        sensor_last = buffRead;
        sensor_count++;
    }

    if(buffRead) {
        if(light_auto) {
          lastMovement = millis();
          if(light_auto_on < 4 && (light_auto_on == 2 || light_limit_auto*4 >= analogRead(LIGHT_PIN))) {
            ir_send('O');
            light_auto_on = 4;
          }
        }
    }
    /* Check if we should turn light off */
    else if(light_auto_on == 4 && (millis() - lastMovement > light_wait_msec)) {
      ir_send('F');
      light_auto_on = 0;
    }
}
}


void job_switch(char *tempBuff, uint8_t charsReceived, boolean tcp) {
        charsReceived--;
        switch (tempBuff[charsReceived]) {
        /* Send info desc:    $D,$D,$F,$D,$D,$F,$D,$D,$D,$D,$D,$D,$D,$D,$D - ovn_aktiv, ovn_on, temp, low_temp, high_temp, TEMP_PRESCALE, doorbell_on, humidity, light(0-1023), sensor count, ovn_mode, overflow_offset, light_auto_on, light_needed_auto, light_wait_msec */
        /* Send info          */ case 'n': if(!tcp) break;       client.print(ovn_aktiv ? "1," : "0,");
                                                                 client.print(ovn_on ? "1," : "0,");
                                                                 client.print(sensors.getTempCByIndex(0));
                                                                 client.print(',');
                                                                 client.print((int)(low_temp*TEMP_PRESCALE));
                                                                 client.print(',');
                                                                 client.print((int)(high_temp*TEMP_PRESCALE));
                                                                 client.print(',');
                                                                 client.print((int)TEMP_PRESCALE);
                                                                 client.print(',');
                                                                 client.print(doorbell_on ? "1," : "0,");
                                                                 client.print(DHT11.humidity);
                                                                 client.print(',');
                                                                 client.print(analogRead(LIGHT_PIN));
                                                                 client.print(',');
                                                                 client.print(sensor_count);
                                                                 client.print(',');
                                                                 client.print(ovn_mode);
                                                                 client.print(',');
                                                                 client.print(overflow_offset);
                                                                 client.print(',');
                                                                 client.print(light_auto);
                                                                 client.print(',');
                                                                 client.print(light_limit_auto);
                                                                 client.print(',');
                                                                 client.print(light_wait_msec);
                                                                 client.stop();
                                                            break;
        /* Send only temp     */ case 't': if(!tcp) break;       sensors.requestTemperatures(); client.print(sensors.getTempCByIndex(0)); client.stop(); break;
        /* Lights off         */ case 'a': if(tcp)  end_TCP();   mySwitch.switchOff(tempBuff[charsReceived-2], 1, tempBuff[charsReceived-1] -48); break;
        /* Lights on          */ case 'p': if(tcp)  end_TCP();   mySwitch.switchOn(tempBuff[charsReceived-2], 1, tempBuff[charsReceived-1] -48); break;
        /* Lights on if dark  */ case 'P': if(tcp)  end_TCP();   if(light_limit_auto*4 < analogRead(LIGHT_PIN)) { break; } mySwitch.switchOn(tempBuff[charsReceived-2], 1, tempBuff[charsReceived-1] -48); break;
        /* Lights dim         */ case 'd': if(tcp)  end_TCP();   mySwitch.switchOn(tempBuff[charsReceived-2], 1, tempBuff[charsReceived-1] -48); delay(DIMTIME); mySwitch.switchOn(tempBuff[charsReceived-2], 1, tempBuff[charsReceived-1] -48); break;
        /* Lights extra       */ case 'x': if(tcp)  end_TCP();   extra_outlet(tempBuff[charsReceived-3] -48, tempBuff[charsReceived-2] -48, tempBuff[charsReceived-1] -48); break;
        /* Send only humidity */ case 'h': if(!tcp) break;       DHT11.read(DHT11PIN); client.print(DHT11.humidity); client.stop(); break;
        /* IR light code      */ case 'z': if(tcp)  end_TCP();   ir_send(tempBuff[charsReceived-1]); break;
        /* IR light auto      */ case 'c': if(tcp)  end_TCP();   light_auto = (light_auto ? false : true); eeprom_write_byte((unsigned byte *) 7,light_auto); break;
        /* IR light auto sec  */ case 'C': if(tcp)  end_TCP();   light_wait_msec = 1000 * ((tempBuff[charsReceived-3]-48)*100 + (tempBuff[charsReceived-2]-48)*10 +(tempBuff[charsReceived-1]-48)); eeprom_write_word((unsigned word *) 8, light_wait_msec/1000); break;
        /* IR light auto limit*/ case 'K': if(tcp)  end_TCP();   light_limit_auto = ((tempBuff[charsReceived-3]-48)*100 + (tempBuff[charsReceived-2]-48)*10 +(tempBuff[charsReceived-1]-48)) / 4; eeprom_write_byte((unsigned byte *) 10, light_limit_auto); break;
        /* Toggle timer(ovn)  */ case 'm': if(tcp)  end_TCP();   if(ovn_aktiv){ ovn_aktiv = false; mySwitch.switchOff('d', 1, 3); ovn_on = false; eeprom_write_byte(0,0); digitalWrite(LED_PIN, 0);} else { ovn_aktiv = true; overflow_count = 0; eeprom_write_byte(0,1);} break;
        /* Change ovn mode    */ case 'M': if(tcp)  end_TCP();   if(++ovn_mode > 1) ovn_mode = 0; eeprom_write_byte((unsigned byte *) 4,ovn_mode); break;
        /* Lights toggle on   */ case 'o': if(tcp)  end_TCP();   toggleAll(tempBuff[charsReceived-1],true); break;
        /* Lights toggle off  */ case 'f': if(tcp)  end_TCP();   toggleAll(tempBuff[charsReceived-1],false); break;
        /* Force ovn on       */ case 'i': if(tcp)  end_TCP();   force_on = 1; on_timer_function(); break;
        /* Force ovn off      */ case 'j': if(tcp)  end_TCP();   force_on = 2; on_timer_function(); break;
        /* Lights sensor      */ case 'l': if(!tcp) break;       client.print(analogRead(LIGHT_PIN)); client.stop(); break;
        /* sensor count       */ case 's': if(!tcp) break;       client.print(sensor_count); client.stop(); sensor_count = 0; break;
        /* Set high temp      */ case 'H': if(tcp)  end_TCP();   high_temp = ((tempBuff[charsReceived-3]-48)*100 + (tempBuff[charsReceived-2]-48)*10 +(tempBuff[charsReceived-1]-48))/TEMP_PRESCALE; eeprom_write_byte((unsigned byte *) 2,high_temp*TEMP_PRESCALE); break;
        /* Set low temp       */ case 'L': if(tcp)  end_TCP();   low_temp  = ((tempBuff[charsReceived-3]-48)*100 + (tempBuff[charsReceived-2]-48)*10 +(tempBuff[charsReceived-1]-48))/TEMP_PRESCALE; eeprom_write_byte((unsigned byte *) 1,low_temp*TEMP_PRESCALE); break;
        /* Doorbell on/off    */ case 'D': if(tcp)  end_TCP();   doorbell_on = (doorbell_on ? false : true); eeprom_write_byte((unsigned byte *) 3,doorbell_on); break;
        /* Force temp check   */ case 'Z': if(tcp)  end_TCP();   on_timer_function(); break;
        /* overflow offset    */ case 'O': if(tcp)  end_TCP();   overflow_offset = ((tempBuff[charsReceived-5]-48)*10000 + (tempBuff[charsReceived-4]-48)*1000 + (tempBuff[charsReceived-3]-48)*100 + (tempBuff[charsReceived-2]-48)*10 +(tempBuff[charsReceived-1]-48)); overflow_count = 0; eeprom_write_word((unsigned word *) 5, overflow_offset); break;
        /* Doorbell           */ case 'r': if(tcp)  end_TCP();   doorbell(); break;
      ///* DIM TEST           */ case 'B': if(tcp)  end_TCP();   Serial.begin(1200);Serial.write("UUUUUUU");Serial.write(tempBuff[charsReceived-4]);Serial.write(((tempBuff[charsReceived-3]-48)*100 + (tempBuff[charsReceived-2]-48)*10 +(tempBuff[charsReceived-1]-48)));Serial.end();break;
      ///* RF12 TEST          */ case 'R': if(tcp)  end_TCP();   rf12_send_test(); break;
        /* Delay, for multi   */ case '&':  delay((tempBuff[charsReceived-1]-48)); break;
        /* Send error msg     */ default : if(!tcp) break;       client.print("ERROR"); client.stop();
      }
}

/*
void rf12_send_test() {
  payload.id = 3;
  payload.value = analogRead(LIGHT_PIN);
  rf12_sleep(RF12_WAKEUP);
  while (!rf12_canSend()) // wait until we can send 
      rf12_recvDone();
  // Send our payload
  rf12_sendStart(0, &payload, sizeof payload); 
  //rf12_sendWait(2);    //wait for RF to finish sending while in standby mode
}
*/

#if DEBUG
static int freeRam () {
    extern int __heap_start, *__brkval; 
    int v; 
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
#endif

