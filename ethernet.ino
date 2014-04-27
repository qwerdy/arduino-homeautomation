void end_TCP() {
    client.print("OK");
    client.stop();
}

void homePage() {
    #define TITLE_START "<html><title>Arduino v" VERSION
    #define TITLE TITLE_START "</title><h1>"
    sensors.requestTemperatures();
    DHT11.read(DHT11PIN);
    long t = millis() / 1000;
    // Calculation without overflow ?!
    /*
    uint8_t s = t % 60;               //seconds
    uint8_t m = (t / 60) % 60;        //minutes
    uint8_t h = ((t / 60) / 60) % 24; //hours
    uint8_t d = ((t / 60) / 60) / 24; //days
    */
    client.print(
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
    );
    client.print(TITLE);
    client.print(sensors.getTempCByIndex(0));
    client.print("</h1><p>Ovn daemon is: "); client.print(ovn_aktiv);
    client.print("<p>Ovn status is: "); client.print(ovn_on);
    client.print("<p>Ovn mode is: "); client.print(ovn_mode ? "cooling" : "heating");
    client.print("<p>Light is: "); client.print(light_auto);client.print(light_auto_on);
    client.print("<p>Light sec: "); client.print(light_wait_msec/1000);
    client.print("<p>brightness limit: "); client.print(light_limit_auto*4);
    client.print("<p>Doorbell is: "); client.print(doorbell_on);
    client.print("<p>Min-Temp: "); client.print(low_temp);
    client.print("<p>Max-Temp: "); client.print(high_temp);
    client.print("<p>DHT11: "); client.print(DHT11.humidity);client.print("% - ");client.print(DHT11.temperature);
    client.print("<p>Uptime: "); client.print(t);
    client.print("<p>Brightness: "); client.print(analogRead(LIGHT_PIN));
    client.print("<p>Sensor count: "); client.print(sensor_count);
    client.print("<p>Last movement: "); client.print(lastMovement/1000);
    client.print("<p>Overflow: "); client.print(overflow_count);client.print(" / ");client.print(overflow_offset);
    client.print("<p>WD resets: "); client.print(wd_resets);
    client.print("<hr><pre>"); client.println(textBuff);
    client.print("<hr><pre>"); client.println(packetBuffer);
#if DEBUG
    client.print("<p>bytes free: "); client.print(freeRam());
#endif
    client.stop();
}


void doorbell()
{   /*
    if (htpc.connect()) {
      htpc.println("GET /xbmcCmds/xbmcHttp?command=ExecBuiltIn(RunScript(special://home/addons/script.doorbell/default.py,doorbell,http://85.221.118.225/axis-cgi/jpg/image.cgi)) HTTP/1.0");
      Serial.println("Doorbell!!!");
      htpc.println();
      htpc.stop();
    }*/
    if(popserver.connect(popip, 80)) {
        popserver.println("GET /notify.php?p=2&e=Doorbell&d=Det%20ringer%20p%C3%A5%20d%C3%B8ra HTTP/1.0");
        popserver.println("Host: example.com");
        popserver.println();
        popserver.stop();
    }
}
