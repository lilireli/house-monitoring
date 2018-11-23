/*
get temp data and sent to the Lora Server
This is an example to show how to get sensor data from a remote Arduino via Wireless Lora Protocol
The exampels requries below hardwares:
1) Client Side: Arduino + Lora Shield (868Mhz) + DS18B20 (Temperature Sensor).
2) Server Side: Arduino + Lora Shield (868Mhz) + Yun Shield + USB flash. make sure the USB flash has
this file datalog.csv in the data directory of root.
requrie below software:
Radiohead library from:  http://www.airspayce.com/mikem/arduino/RadioHead/
OneWire library for Arduino
DallasTemperature library for Arduino
Client side will get the temperature and keep sending out to the server via Lora wireless.
Server side will listin on the Lora wireless frequency, once it get the data from Client side, it will
turn on the LED and log the sensor data to a USB flash,
*/

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
///                 Arduino      RFM95/96/97/98
///                 GND----------GND   (ground in)
///                 3V3----------3.3V  (3.3V in)
/// interrupt 0 pin D2-----------DIO0  (interrupt request out)
///          SS pin D10----------NSS   (chip select in)
///         SCK pin D13----------SCK   (SPI clock in)
///        MOSI pin D11----------MOSI  (SPI Data in)
///        MISO pin D12----------MISO  (SPI Data out)

#include <Arduino.h>

//Include required lib so Arduino can talk with the Lora Shield
#include <SPI.h>
#include <RH_RF95.h>

/* Dependancy for the bus 1-Wire */
// In here we have a DS18B20 temperature sensor
#include <OneWire.h>


// Singleton instance of the radio driver
RH_RF95 rf95;
int temp_pin = 4;
int led_pin = 5;
float temperature;
String datastring="";
char databuf[10];
uint8_t dataoutgoing[10];

/* Return codes for the function getTemperature() */
enum DS18B20_RCODES {
    READ_OK,  // Reading ok
    NO_SENSOR_FOUND,  // No Sensor
    INVALID_ADDRESS,  // Received address unvalid
    INVALID_SENSOR  // Sensor unvalid (not a DS18B20)
};

/* OneWire object */
OneWire ds(temp_pin);

byte getTemperature(float *temperature, byte reset_search);


void setup()
{
    Serial.begin(9600);
    if (!rf95.init())
    Serial.println("init failed");
    // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
    // Need to change to 868.0Mhz in RH_RF95.cpp

    pinMode(led_pin, OUTPUT);
}

void loop()
{
    /* Read ambient temperature at ~1Hz */
    if (getTemperature(&temperature, true) != READ_OK) {
        Serial.println(F("Erreur de lecture du capteur"));
        return;
    }

    /* Affiche la température */
    Serial.print(F("Temperature : "));
    Serial.print(temperature, 2);
    Serial.write(176); // Caractère degré
    Serial.write('C');
    Serial.println();

    Serial.println("Sending to rf95_server");

    digitalWrite(led_pin, HIGH);
    datastring += dtostrf(temperature, 4, 2, databuf);
    strcpy((char *)dataoutgoing,databuf);
    Serial.println(databuf);

    rf95.send(dataoutgoing, sizeof(dataoutgoing));
    rf95.waitPacketSent();

    // Now wait for a reply
    if (rf95.waitAvailableTimeout(3000))
    {
        // Should be a reply message for us now
        uint8_t indatabuf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(indatabuf);

        if (rf95.recv(indatabuf, &len))
        {
            Serial.print("got reply: ");
            Serial.println((char*)indatabuf);
        }
        else
        {
            Serial.println("recv failed");
        }
    }
    else
    {
        Serial.println("No reply, is rf95_server running?");
    }

    digitalWrite(led_pin, LOW);
    delay(4000);

}

/**
* Reading from a sensor DS18B20.
*/
byte getTemperature(float *temperature, byte reset_search) {
    byte data[9], addr[8];
    // data[] : Data read from the scratchpad
    // addr[] : Detected address for the bus onewire

    /* Reset the bus 1-Wire if necessary (needed to read the first sensor) */
    if (reset_search)
        ds.reset_search();

    if (!ds.search(addr))
        return NO_SENSOR_FOUND;

    if (OneWire::crc8(addr, 7) != addr[7])
        return INVALID_ADDRESS; // CRC is not valid

    if (addr[0] != 0x28) // address 0x28 for DS18B20 and 0x10 for DS18S20
        return INVALID_SENSOR; // Device family not recognized

    /* Reset the bus 1-Wire and select the sensor */
    ds.reset();
    ds.select(addr);

    /* Start a measurement and wait for the end of it */
    ds.write(0x44, 1); // start conversion, with parasite power on at the end
    delay(800);

    /* Reset the bus 1-Wire, select the sensor and send a request to the scratchpad */
    ds.reset();
    ds.select(addr);
    ds.write(0xBE);  // Read Scratchpad

    /* Read the scratchpad */
    for (byte i = 0; i < 9; i++)
        data[i] = ds.read();

    /* Calculate the temperature in degree celsius */
    *temperature = (int16_t) ((data[1] << 8) | data[0]) * 0.0625;

    return READ_OK;
}
