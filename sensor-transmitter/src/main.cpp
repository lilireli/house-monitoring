// Arduino sensor to retrieve the temperature and send it through LoRa

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
    // Defaults after init are 868.0Mhz, 13dBm, Bw = 125 kHz, Cr = 4/5,
    //                         Sf = 128chips/symbol, CRC on
    // Can be changed in RH_RF95.cpp

    pinMode(led_pin, OUTPUT);
}

void loop()
{
    /* Read ambient temperature at ~1Hz */
    if (getTemperature(&temperature, true) != READ_OK) {
        Serial.println(F("Erreur de lecture du capteur"));
        return;
    }

    /* Print temperature (for debug purpose) */
    // Serial.print("Temperature : ");
    // Serial.print(temperature, 2);
    // Serial.print("C");
    // Serial.println();
    //
    // Serial.println("Sending to rf95_server");

    digitalWrite(led_pin, HIGH);
    datastring += "{\"id\": \"serre001\", \"temp\": ";
    datastring += dtostrf(temperature, 4, 2, databuf);
    datastring += "}";
    strcpy((char *)dataoutgoing,databuf);

    rf95.send(dataoutgoing, sizeof(dataoutgoing));
    rf95.waitPacketSent();

    delay(1000);
    digitalWrite(led_pin, LOW);
    delay(3000);
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
