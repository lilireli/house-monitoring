#pragma once

#include "client_http.hpp"
#include "server_http.hpp"
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <thread>

// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <sqlite3.h> 
#include <fstream>
#include <vector>
#include <iomanip>
#ifdef HAVE_OPENSSL
#include "crypto.hpp"
#endif

#define DATABASE "test.db"
#define PORT 8080

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

// SQL functions
std::vector<std::vector<std::string>> query_db(std::string query, int nb_cols);

std::stringstream get_kpi_temp();

std::stringstream get_graph_temp();

// ZMQ functions and alarm
std::stringstream get_alarm_status();

// Logger
// the logger will now how to output data
// and also the status of the application at a particular point in time
class Logger
{
  public:
    Logger(){}

    std::string getState() const
    {
        return m_state;
    }

    static void setState(std::string new_state)
    {
        m_state = new_state;
    }

    template< typename T >
    Logger& operator<<(const T& p_value)
    {
        std::cout << p_value << std::endl;
        return *this;
    }

  private:
    static std::string m_state;
};

// Server functions
class Server
{
  public:
    void start();
    void stop();

  private:
    HttpServer m_server;
    std::shared_ptr<std::thread> m_server_thread;
};
