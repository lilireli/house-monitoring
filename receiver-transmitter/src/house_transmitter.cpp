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

namespace pt = boost::property_tree;
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
IHM::IHM(): m_alarm_enabled(false)
          , m_running(true)
          , m_alarm_running(false)
          , m_noise_running(false)
          , m_last_received(time(0))
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

    m_led_thread = std::make_unique<std::thread>(std::thread([this]{
        blink_led();
    }));
}

IHM::~IHM()
{
    std::cout << "Shutting down alarms" << std::endl;
    stop_alarm(99);
    m_running = false;
    m_led_thread->join();
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(ALARM, LOW);

    m_info_screen.print_line_one("Program not");
    m_info_screen.print_line_two("running");
}

void IHM::start_alarm(std::string error_msg)
{
    m_alarm_running = true;

    if (!m_noise_running && m_alarm_enabled 
                         && error_msg != "No webserver" 
                         && difftime(time(0), m_last_received) > WAITING_TIME_ALARM){
        m_alarm_thread = std::make_unique<std::thread>(std::thread([this]{
            make_noise();
        }));
    }

    m_info_screen.print_line_two(error_msg);
}

void IHM::stop_alarm(float min_temp)
{
    m_alarm_running = false;
    m_last_received = time(0);
 
    if (m_noise_running){
        m_alarm_thread->join();
    }

    std::stringstream temp_stream;
    temp_stream << std::fixed << std::setprecision(1) << min_temp;
    m_info_screen.print_line_two("min 24h:  " + temp_stream.str() + " C");
}

int IHM::print_temp(float temp)
{
    std::stringstream temp_stream;
    temp_stream << std::fixed << std::setprecision(1) << temp;
    m_info_screen.print_line_one("temp:     " + temp_stream.str() + " C");    
}

void IHM::no_temp()
{
    m_info_screen.print_line_one("no temp");
}

void IHM::set_alarm_enabled(bool enable)
{
    std::cout << "Alarm set to " << enable << std::endl;
    m_alarm_enabled = enable;

    if (m_noise_running && !m_alarm_enabled){
        m_alarm_thread->join();
    }
}

void IHM::make_noise()
{
    m_noise_running = true;

    while(m_alarm_running && m_alarm_enabled)
    {
        digitalWrite(ALARM, HIGH);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        digitalWrite(ALARM, LOW);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    m_noise_running = false;
}

void IHM::blink_led()
{
    int current_led = LED_RED;

    while(m_running)
    {
        if (m_alarm_running) { current_led = LED_RED; }
        else { current_led = LED_GREEN; }

        digitalWrite(current_led, HIGH);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        digitalWrite(current_led, LOW);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
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
    // We allow a timeout before sending error
    for(int i=0; i < 60; i++)
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

                    // We need to remove unuseful characters at the end of the string
                    std::string req_corrupt = std::string((char*)buf);
                    std::size_t req_corrupt_end = req_corrupt.find("}");
                    
                    if (req_corrupt_end == std::string::npos)
                    {
                        std::cout << "Received badly formatted message";
                        throw std::string("Badly formatted\n");
                    }

                    std::istringstream req(req_corrupt.substr(0, req_corrupt_end + 1));
                    pt::ptree root;
                
                    pt::read_json(req, root);
                    // Not used yet -> std::string device = root.get<std::string>("id");
                    *temp = root.get<float>("temp");

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
ZmqSender::ZmqSender(std::string url): m_context(1), m_url(url)
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

TempKeeper::TempKeeper() 
{
    for (int i = 0; i < SIZE_TEMP_KEEPER; i++)
    {
        m_temp_24h.push_back(99);
    }
}

void TempKeeper::add(float temp) 
{
    time_t now = time(0);
    struct tm * timeinfo = localtime(&now);
    int approx_curr_pos = 60 * timeinfo->tm_hour + timeinfo->tm_min;
    int curr_pos = approx_curr_pos / PRECISION_KEEPER;

    // Erase old values
    int erase_pos = (curr_pos < SIZE_TEMP_KEEPER - 1) ? curr_pos + 1 : 0;
    m_temp_24h[erase_pos] = 99;

    // Compare new one with running 15 min
    if (m_temp_24h[curr_pos] > temp)
    {
        m_temp_24h[curr_pos] = temp;
    }
}

float TempKeeper::min_24h() 
{
    auto min = std::min_element(m_temp_24h.begin(), m_temp_24h.end());
    return *min;
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
    TempKeeper temp_keeper;

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
            temp_keeper.add(temp);
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

        if (status == "ok") { ihm.stop_alarm(temp_keeper.min_24h()); }
        else { ihm.start_alarm(errors[status]); }  
    }

    return 0;
}