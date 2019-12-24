#pragma once

#include <iostream>
#include <string>
#include <iomanip>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>

#include <bcm2835.h> // Includes for PINs
#include <RasPi.h> // Includes for PINs
#include <RH_RF69.h> // Lora transmitter
#include <RH_RF95.h> // Lora transmitter
#include "LiquidCrystal.h" // LCD screen
#include <zmq.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Those numbers are the one of the pins of Raspberry (without any conversion)
#define RF_CS_PIN  RPI_V2_GPIO_P1_26 // Slave Select on GPIO25 so P1 connector pin #22
#define RF_RST_PIN RPI_V2_GPIO_P1_12 // Reset on GPIO17 so P1 connector pin #11

#define LCD_RS  RPI_V2_GPIO_P1_37    // Register select pin
#define LCD_E   RPI_V2_GPIO_P1_35    // Enable Pin
#define LCD_D4  RPI_V2_GPIO_P1_33    // Data pin 4
#define LCD_D5  RPI_V2_GPIO_P1_31    // Data pin 5
#define LCD_D6  RPI_V2_GPIO_P1_29    // Data pin 6
#define LCD_D7  RPI_V2_GPIO_P1_36    // Data pin 7

#define LED_GREEN     RPI_V2_GPIO_P1_15 // Green led to show everything is ok
#define LED_RED       RPI_V2_GPIO_P1_16 // Red led to show something is wrong
#define ALARM         RPI_V2_GPIO_P1_13 // Alarm to start anytime something is wrong

// Our RFM95 Configuration 
#define RF_FREQUENCY  868.00
#define RF_NODE_ID    1

#define TRIGGER_TEMP        3     // in degrees
#define WAITING_TIME_ALARM  3600  // in seconds
#define SIZE_TEMP_KEEPER    24*15 // Keep per 15 minutes 24h of data

class InfoScreen
{
  public:
    InfoScreen();
    void print_line_one(std::string text);
    void print_line_two(std::string text);

  private:
    LiquidCrystal m_lcd;
};

class IHM
{
  public:
    IHM();
    ~IHM();

    void start_alarm(std::string error_msg);
    void stop_alarm(float min_temp);
    int print_temp(float temp);
    void no_temp();
    bool get_alarm_enabled(){return m_alarm_enabled;}
    void set_alarm_enabled(bool enable);
    void make_noise();
    void blink_led();

  private:
    InfoScreen m_info_screen;
    std::unique_ptr<std::thread> m_alarm_thread;
    std::unique_ptr<std::thread> m_led_thread;
    bool m_alarm_enabled;
    std::atomic<bool> m_running;
    std::atomic<bool> m_alarm_running;
    std::atomic<bool> m_noise_running;
    time_t m_last_received;
};

class LoraReceiver
{
  public:
    LoraReceiver();
    ~LoraReceiver();

    int recv(float* temp);

  private:
    RH_RF95 m_rf95;
};

class ZmqSender
{
  public:
    ZmqSender(std::string url);

    void initialize_socket();
    int send(float temp, std::string status);
    int receive(bool* alarm_enabled);

  private:
    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    std::string m_url;
};

class TempKeeper
{
  public:
    TempKeeper();
    void add(float temp);
    float min_24h();

  private:
    std::vector<float> m_temp_24h; // keeps last 24h min temp per 15 min
};

// Handling Ctrl-C interrupt
void sig_handler(int sig);