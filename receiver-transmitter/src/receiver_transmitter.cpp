#include <receiver_transmitter.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <csignal>

// Create an instance of a driver
RH_RF95 rf95(RF_CS_PIN);

//Flag for Ctrl-C
volatile sig_atomic_t force_exit = false;

void sig_handler(int sig)
{
    std::cout << "\nBreak received, exiting!" << std::endl;
    force_exit = true;
}

void setup()
{
    std::cout << "Starting program" << std::endl;

    // Register handler
    signal(SIGINT, sig_handler);

    // Initialize Pins
    if (!bcm2835_init())
    {
        throw std::string("bcm2835_init() Failed\n\n");
    }

    std::cout << "RF95 CS=GPIO%d" << RF_CS_PIN
              << ", RST=GPIO%d" << RF_RST_PIN
              << std::endl;

    // Pulse a reset on module
    pinMode(RF_RST_PIN, OUTPUT);
    digitalWrite(RF_RST_PIN, LOW);
    bcm2835_delay(150);
    digitalWrite(RF_RST_PIN, HIGH);
    bcm2835_delay(100);

    // Initialize actuators
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(ALARM, OUTPUT);

    if (!rf95.init())
    {
        throw std::string("\nRF95 module init failed, Please verify wiring/module\n");
    }
    
    // check your country max power useable, in EU it's +14dB
    rf95.setTxPower(14, false);

    // Adjust Frequency
    rf95.setFrequency(RF_FREQUENCY);

    // If we need to send something
    rf95.setThisAddress(RF_NODE_ID);
    rf95.setHeaderFrom(RF_NODE_ID);

    // Be sure to grab all node packet
    // we're sniffing to display, it's a demo
    rf95.setPromiscuous(true);

    // We're ready to listen for incoming message
    rf95.setModeRx();

    std::cout << " OK NodeID=" << RF_NODE_ID << " @ "
              << std::fixed << std::setw(3) << std::setprecision(2) 
              << RF_FREQUENCY << "MHz\n" << std::endl;    
}

void teardown()
{
    std::cout << "Ending program" << std::endl;

    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(ALARM, LOW);

    bcm2835_close();
}

int check_temp(float temp)
{
    if(temp < TRIGGER_TEMP)
    {
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(buzzer, HIGH);
        return 1;
    }
    else
    {
        digitalWrite(LED_GREEN, HIGH);
        return 0;
    }
}

//Main Function
int main(int argc, const char *argv[])
{
    setup();

    LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
    lcd.begin(16, 2);

    lcd.print("Temperature:     ");
    lcd.setCursor(0, 1);
    lcd.print("                 ");

    while (!force_exit)
    {
        if (rf95.available())
        {
            // Should be a message for us now
            uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
            uint8_t len = sizeof(buf);

            // Timeout if no data received -> raspberry is down -> alert
            if (rf95.recv(buf, &len))
            {
                std::cout << "Data: " << (char *)buf << std::endl;
                float temp = std::stof((char *)buf);

                // Send a reply to client as ACK
                uint8_t data[] = "200 OK";
                rf95.send(data, sizeof(data));
                rf95.waitPacketSent();

                lcd.setCursor(0, 1);
                lcd.print((char*)buf);

                int alarm = check_temp(temp);

                // TODO: send data to server with alarm status
                // TODO: add sound of alarm

                // TODO: check alarm is activated with result of zmq
            }
            else
            {
                std::cout << "receive failed" << std::endl;
            }
        }

        // Let OS do other tasks
        // For timed critical appliation you can reduce or delete
        // this delay, but this will charge CPU usage, take care and monitor
        bcm2835_delay(5);
    }

    teardown();
    return 0;
}