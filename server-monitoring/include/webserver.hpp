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

#define DB_INSERT_INTERVAL 600

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

enum class Error {
    OK,
    TEMPLOW,
    ERRARDUINO,
    ERRRASPBERRY,
    ERRWEBSERVER,
    ERRNODATA
};

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

    Error getState() const
    {
        return m_state;
    }

    static void setState(Error state)
    {
        m_state = state;
    }

    bool getAlarmEnabled() const
    {
        return m_alarm_enabled;
    }

    static void setAlarmEnabled(bool alarm_enabled)
    {
        m_alarm_enabled = alarm_enabled;
    }

    template< typename T >
    Logger& operator<<(const T& p_value)
    {
        std::cout << p_value << std::endl;
        return *this;
    }

  private:
    static Error m_state;
    static bool m_alarm_enabled;
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

    sqlite3* open_db();

    std::vector<std::vector<std::string>> query_db(std::string query, uint nb_cols);

    void insert_db(std::string timestamp, float temperature);

    void empty_table();

    void empty_table_cron();

  private:
    static std::string m_db_path;
    std::unique_ptr<std::thread> m_db_thread;
};

// Aggregator functions
class Aggregator
{
  public:
    Aggregator(): m_last_time(0) {}
    void add_new_value(float temperature, std::string timestamp);

  private:
    std::vector<float> m_temps;
    time_t m_last_time;
};

// Server functions
class Server
{
  public:
    Server(int port);
    std::stringstream get_kpi_temp();
    std::stringstream get_graph_temp();
    std::stringstream get_alarm_status();
    std::stringstream get_alarm_enabled();
    void set_alarm_enabled(std::string content);
    void start();
    void stop();

  private:
    HttpServer m_server;
    std::unique_ptr<std::thread> m_server_thread;
};

class ZmqReceiver
{
  public:
    ZmqReceiver(int port);
    void initialize_socket();
    void communicate();
    void start();
    void stop();

  private:
    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    int m_port;
    std::unique_ptr<std::thread> m_zmq_thread;
};