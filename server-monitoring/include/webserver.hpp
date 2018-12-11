#pragma once

#define BOOST_SPIRIT_THREADSAFE

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <thread>
#include <algorithm>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/program_options.hpp>
#include <sqlite3.h> 
#include <zmq.hpp>

// For the webserver
#include "client_http.hpp"
#include "server_http.hpp"

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

// SQL functions
std::vector<std::vector<std::string>> query_db(std::string query, int nb_cols);

std::stringstream get_kpi_temp();

std::stringstream get_graph_temp();

// ZMQ functions and alarm
std::stringstream get_alarm_status();

// Config
class Config
{
  public:
    Config(std::string config_file);

    int getWebserverPort()
    {
        return m_webserver_port;
    }

    int getZmqPort()
    {
        return m_zmq_port;
    }

    std::string getDbPath()
    {
        return m_db_path;
    }

  private:
    int m_webserver_port;
    int m_zmq_port;
    std::string m_db_path;
};

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

// Database functions
class Database
{
  public:
    Database(){}

    void init(std::string db_path)
    {
        m_db_path = db_path;
    }

    std::vector<std::vector<std::string>> query_db(std::string query, int nb_cols);

    void insert_db(std::string timestamp, float temperature);

  private:
    static std::string m_db_path;
};

// Server functions
class Server
{
  public:
    void start(int port);
    void stop();

  private:
    HttpServer m_server;
    std::shared_ptr<std::thread> m_server_thread;
};
