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

//// Info Screen ////
InfoScreen::InfoScreen(): m_lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7)
{
    m_lcd.begin(16, 2);
}

void InfoScreen::print_line_one(std::string text)
{
    m_lcd.setCursor(0, 0);
    m_lcd.print("                 ");
    m_lcd.setCursor(0, 0);
    m_lcd.print(text.c_str());
}

void InfoScreen::print_line_two(std::string text)
{
    m_lcd.setCursor(0, 1);
    m_lcd.print("                 ");
    m_lcd.setCursor(0, 1);
    m_lcd.print(text.c_str());
}

//// IHM ////
IHM::IHM(): m_alarm_enabled(false), m_alarm_running(false)
{
    // Initialize actuators
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(ALARM, OUTPUT);

    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(ALARM, LOW);

    m_info_screen.print_line_one("Hello");
    m_info_screen.print_line_two("");
}

IHM::~IHM()
{
    std::cout << "Shutting down alarms" << std::endl;
    stop_alarm();
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(ALARM, LOW);

    m_info_screen.print_line_one("Program not");
    m_info_screen.print_line_two("running");
}

void IHM::start_alarm(std::string error_msg)
{
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);

    if (!m_alarm_running && m_alarm_enabled && error_msg != "No webserver"){
        m_alarm_running = true;
        m_alarm_thread = std::make_unique<std::thread>(std::thread([this]{
            make_noise();
        }));
    }

    m_info_screen.print_line_one(error_msg);
}

void IHM::stop_alarm()
{
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);

    if (m_alarm_running){
        m_alarm_running = false;
        m_alarm_thread->join();
    }
    digitalWrite(ALARM, LOW);

    m_info_screen.print_line_one("Hello");
}

int IHM::print_temp(float temp)
{
    std::stringstream temp_stream;
    temp_stream << std::fixed << std::setprecision(2) << temp;
    m_info_screen.print_line_two("temp:    " + temp_stream.str() + " C");
}

void IHM::no_temp()
{
    m_info_screen.print_line_two("");
}

void IHM::set_alarm_enabled(bool enable)
{
    std::cout << "Alarm set to " << enable << std::endl;
    m_alarm_enabled = enable;

    if (m_alarm_running && !m_alarm_enabled){
        m_alarm_thread->join();
    }
}

void IHM::make_noise()
{
    while(m_alarm_running && m_alarm_enabled)
    {
        digitalWrite(ALARM, HIGH);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        digitalWrite(ALARM, LOW);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    m_alarm_running = false;
}

//// LoRa Receiver ////
LoraReceiver::LoraReceiver(): m_rf95(RF_CS_PIN)
{
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

    if (!m_rf95.init())
    {
        throw std::string("\nRF95 module init failed, Please verify wiring/module\n");
    }
    
    // check your country max power useable, in EU it's +14dB
    m_rf95.setTxPower(14, false);

    // Adjust Frequency
    m_rf95.setFrequency(RF_FREQUENCY);

    // If we need to send something
    m_rf95.setThisAddress(RF_NODE_ID);
    m_rf95.setHeaderFrom(RF_NODE_ID);

    // Be sure to grab all node packet
    // we're sniffing to display, it's a demo
    m_rf95.setPromiscuous(true);

    // We're ready to listen for incoming message
    m_rf95.setModeRx();

    std::cout << " OK NodeID=" << RF_NODE_ID << " @ "
            << std::fixed << std::setw(3) << std::setprecision(2) 
            << RF_FREQUENCY << "MHz\n" << std::endl;    
}

LoraReceiver::~LoraReceiver(){
    std::cout << "Closing Lora connection" << std::endl;
    bcm2835_close();
}

int LoraReceiver::recv(float* temp)
{
    // We allow a timeout
    for(int i=0; i < 30; i++)
    {
        if (m_rf95.available())
        {
            // Should be a message for us now
            uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
            uint8_t len = sizeof(buf);

            if (m_rf95.recv(buf, &len))
            {
                try {
                    std::cout << "Data: " << (char *)buf << std::endl;
                    *temp = std::stof((char *)buf);
                    return 1;
                }
                catch (...) {
                    std::cout << "No temperature received from sensor" << std::endl;
                }
            }
        }

        // wait one more sec to have result
        bcm2835_delay(1000);
    }
    
    // Alert no data received after one minute
    std::cout << "No temperature received from sensor" << std::endl;
    return -1;
}

//// ZMQ Sender ////
ZmqSender::ZmqSender(std::string url)
: m_context(1), m_url(url), m_last_time(0),m_last_temp(-99)
{
    std::cout << "Connecting to server at address " << url << std::endl;
    initialize_socket();
}

void ZmqSender::initialize_socket()
{
    m_socket = std::make_unique<zmq::socket_t>(m_context, ZMQ_REQ);
    int timeout = 10000;
    
    m_socket->setsockopt(ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
    m_socket->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    m_socket->connect(m_url);
}

int ZmqSender::send(float temp, std::string status)
{
    try
    {
         // check datetime
        time_t now = time(0);
        struct tm * timeinfo = localtime(&now);
        char datetime_str[30];
        strftime(datetime_str,30,"%Y-%m-%dT%H:%M:%S",timeinfo);

        if (difftime(now, m_last_time) < 60 && abs(temp - m_last_temp) > 1)
        {
            std::cout << "Ignoring value " << temp << std::endl;
            return 1;
        }

        m_last_temp = temp;
        m_last_time = now;

        // create message
        std::ostringstream json_stream;

        json_stream << std::setprecision(2) << std::fixed
                    << "{"
                    << "\"datetime\":\"" << datetime_str << "\","
                    << "\"temperature\":" << temp << ","
                    << "\"alarm_current\":\"" << status << "\""
                    << "}";

        std::string json_msg = json_stream.str();

        zmq::message_t request(json_msg.length()+1);
        snprintf((char *) request.data(), 
                json_msg.length()+1, 
                json_msg.c_str());

        // send it
        int result = m_socket->send(request);

        return result;
    }
    catch (...)
    {
        std::cout << "ZMQ crashed" << std::endl;
        return 1;
    }
   
}

int ZmqSender::receive(bool* alarm_enabled)
{
    try
    {
        zmq::message_t reply;
        int result = m_socket->recv(&reply);

        if (result <= 0)
        {
            // delete client if answer not received
            std::cout << "Webserver is down" << std::endl;
            initialize_socket();
        }

        if (result > 0)
        {
            std::istringstream iss(static_cast<char*>(reply.data()));
            iss >> *alarm_enabled;
        }
        
        return result;
    }
    catch (...)
    {
        std::cout << "ZMQ crashed" << std::endl;
        return 1;
    }
}

//Flag for Ctrl-C
volatile sig_atomic_t force_exit = false;

void sig_handler(int sig)
{
    std::cout << "\nBreak received, exiting!" << std::endl;
    force_exit = true;
}

// Main Function
int main(int argc, const char *argv[])
{
    // * Parsing arguments *
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

    // * Main program *
    std::cout << "Starting program" << std::endl;

    // Register handler
    signal(SIGINT, sig_handler);

    LoraReceiver lora_receiver;
    ZmqSender zmq_sender(url);
    IHM ihm;

    std::map<std::string, std::string> errors = {
        {"tempLow",         "Temp too low"},
        {"errArduino",      "No temp received" },
        {"errWebserver",    "No webserver" }
    };

    while (!force_exit)
    {
        float temp = 99;
        bool alarm_enabled = ihm.get_alarm_enabled();
        std::string status = "ok";
        std::string print_status = "";

        if (lora_receiver.recv(&temp) > 0)
        {
            ihm.print_temp(temp);
            if(temp < TRIGGER_TEMP) { status = "tempLow"; }
        }
        else
        {
            ihm.no_temp();
            status = "errArduino";
        }

        zmq_sender.send(temp, status);

        if (zmq_sender.receive(&alarm_enabled) <= 0) { 
            if (status == "ok") { status = "errWebserver"; }
        }

        if (ihm.get_alarm_enabled() != alarm_enabled)
        {
            ihm.set_alarm_enabled(alarm_enabled);
        }

        if (status == "ok") { ihm.stop_alarm(); }
        else { ihm.start_alarm(errors[status]); }        
    }

    return 0;
}