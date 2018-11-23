#pragma once

#include <bcm2835.h> // Includes for PINs
#include <RasPi.h> // Includes for PINs
#include <RH_RF69.h> // Lora transmitter
#include <RH_RF95.h> // Lora transmitter
#include "LiquidCrystal.h" // LCD screen

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

#define TRIGGER_TEMP    23 // in degrees

// Handling Ctrl-C interrupt
void sig_handler(int sig);

void setup();
void teardown();

int check_temp(float temp);