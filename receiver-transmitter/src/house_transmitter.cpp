// Raspberry transmitter to retrieve the temperature through LoRa and resend it through Zmq

// Dragino Lora SPI Pins
///                 Raspberry    RFM95/96/97/98
///                 GND----------GND   (ground in)
///                 3V3----------3.3V  (3.3V in)
///  interrupt 0 pin 24----------DIO0  (interrupt request out)
///           SS pin 26----------NSS   (chip select in)
///          SCK pin 23----------SCK   (SPI clock in)
///         MOSI pin 19----------MOSI  (SPI Data in)
///         MISO pin 21----------MISO  (SPI Data out)

#include <house_transmitter.h>

namespace po = boost::program_options;

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
        // digitalWrite(buzzer, HIGH);
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
    std::string url;

    po::options_description desc("Transmitter for house-monitoring project");
    desc.add_options()
        ("help,h", "produce help message")
        ("url", po::value<std::string>(), "defines to which url send the data")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    if (vm.count("url")) {
        url = vm["url"].as<std::string>();
    } else {
        std::cout << "No url was provided." << std::endl;
        return 1;
    }

    setup();

    // Prepare LCD screen
    LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
    lcd.begin(16, 2);

    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REQ);

    std::cout << "Connecting to server at address " << url << std::endl;
    socket.connect (url);

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

            // Timeout if no data received -> arduino is down -> alert
            if (rf95.recv(buf, &len))
            {
                std::cout << "Data: " << (char *)buf << std::endl;
                float temp = std::stof((char *)buf);

                lcd.setCursor(0, 1);
                lcd.print((char*)buf);

                int alarm = check_temp(temp);

                // check datetime
                time_t now = time(0);
                struct tm * timeinfo = localtime(&now);
                char buffer[30];

                strftime(buffer,30,"%Y-%m-%dT%H:%M:%S",timeinfo);

                std::cout << "The local date and time is: " << buffer << std::endl;

                //  Send message to all subscribers
                zmq::message_t request(40);
                snprintf((char *) request.data(), 40, "%s %2.2f", buffer, temp);
                socket.send (request);

                //  Get the reply.
                zmq::message_t reply;
                socket.recv (&reply);
                std::cout << "Received " << reply.data() << std::endl;

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