// https://github.com/dragino/Lora/blob/master/Lora%20Shield/Examples/Lora_Temperature_RadioHead/get_data_from_lora_node_and_store_in_USB/get_data_from_lora_node_and_store_in_USB.ino

// https://github.com/hallard/RadioHead/blob/master/examples/raspi/rf95/rf95_client.cpp

/*
Lora Shield- Yun Shield to log data into USB
This is an example to show how to get sensor data from a remote Arduino via Wireless Lora Protocol
The exampels requries below hardwares:
1) Client Side: Arduino + Lora Shield (868Mhz) + DS18B20 (Temperature Sensor).
2) Server Side: Arduino + Lora Shield (868Mhz) + Yun Shield + USB flash. make sure the USB flash has
this file datalog.csv in the data directory of root.
requrie below software:
Radiohead library from:  http://www.airspayce.com/mikem/arduino/RadioHead/
Client side will get the temperature and keep sending out to the server via Lora wireless.
Server side will listin on the Lora wireless frequency, once it get the data from Client side, it will
turn on the LED and log the sensor data to a USB flash,
More about this example, please see:
*/

// Dragino Lora SPI Pins
///                 Raspberry    RFM95/96/97/98
///                 GND----------GND   (ground in)
///                 3V3----------3.3V  (3.3V in)
/// interrupt 0 pin GPIO19-------DIO0  (interrupt request out)
///          SS pin GPIO8--------NSS   (chip select in)
///         SCK pin GPIO9--------SCK   (SPI clock in)
///        MOSI pin GPIO7--------MOSI  (SPI Data in)
///        MISO pin GPIO10-------MISO  (SPI Data out)

#include <wiringPi.h>

//Include required lib so Arduino can talk with the Lora Shield
#include <wiringPiSPI.h>
#include <RH_RF95.h>
#include <string>
#include <iostream>


// Singleton instance of the radio driver
RH_RF95 rf95;
int led = 0; // GPIO17
int reset_lora = 1; // GPIO18
std::string dataString = "";


void setup()
{
    pinMode(led, OUTPUT);
    pinMode(reset_lora, OUTPUT);

    // reset lora module first. to make sure it will works properly
    digitalWrite(reset_lora, LOW);
    //delay(1000);
    digitalWrite(reset_lora, HIGH);

    Serial.begin(9600);
    if (!rf95.init())
        Serial.println("init failed");
    // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
    // Need to change to 868.0Mhz in RH_RF95.cpp
}


void loop()
{
    if (rf95.available())
    {
        Serial.println("Get new message");

        // Should be a message for us now
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);

        if (rf95.recv(buf, &len))
        {
            digitalWrite(led, HIGH);
            // RH_RF95::printBuffer("request: ", buf, len);
            Serial.print("got message: ");
            Serial.println((char*)buf);
            // Serial.print("RSSI: ");
            // Serial.println(rf95.lastRssi(), DEC);

            // Send a reply to client as ACK
            uint8_t data[] = "200 OK";
            rf95.send(data, sizeof(data));
            rf95.waitPacketSent();

            digitalWrite(led, LOW);
        }
        else
        {
            Serial.println("recv failed");
        }

        // delay(200);
    }
}
