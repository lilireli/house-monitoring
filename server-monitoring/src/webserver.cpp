// Webserver to retrieve the temperature and print it

#include "webserver.hpp"

// Added for the json-example:
namespace pt = boost::property_tree;
namespace po = boost::program_options;

Error Logger::m_state = Error::OK;
bool Logger::m_alarm_enabled = true;
std::string Database::m_db_path = "";

//// Config ////
Config::Config(std::string config_file)
{
    pt::ptree root;
    
    pt::read_json(config_file, root);
    m_webserver_port = root.get<int>("webserver_port", 0);
    m_zmq_port = root.get<int>("zmq_port", 0);
    m_db_path = root.get<std::string>("database");
}

//// Database ////
std::vector<std::vector<std::string>> Database::query_db(std::string query, uint nb_cols)
{
    sqlite3 *db;
    std::vector<std::vector<std::string>> result;

    if (sqlite3_open(m_db_path.c_str(), &db))
    {
        Logger() << "Can't open database: " << sqlite3_errmsg(db);
        throw std::invalid_argument("cannot open database");;
    }

    try
    {
        sqlite3_stmt * stmt;

        sqlite3_prepare( db, query.c_str(), -1, &stmt, NULL ); 
        sqlite3_step( stmt ); //executing the statement

        for(uint i = 0; i < nb_cols; i++ )
            result.push_back(std::vector< std::string >());

        if (!sqlite3_column_text(stmt, 0))
        {
            Logger() << "Query did not match any data";
            throw std::invalid_argument("query did not match any data");
        }

        while(sqlite3_column_text(stmt, 0))
        {
            for(uint i = 0; i < nb_cols; i++)
                result[i].push_back(std::string((char *)sqlite3_column_text(stmt, i)));
            sqlite3_step( stmt );
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }
    catch(...)
    {
        sqlite3_close(db);
        throw;
    }

    return result;
}

void Database::insert_db(std::string timestamp, float temperature)
{
    sqlite3 *db;
    
    if (sqlite3_open(m_db_path.c_str(), &db))
    {
        Logger() << "Can't open database: " << sqlite3_errmsg(db);
        throw std::invalid_argument("cannot open database");
    }

    try
    {
        std::string query =
            "INSERT INTO temperature_serre(received_time, temperature_celsius) "
            "VALUES ('"
                + timestamp + "',"
                + std::to_string(temperature) + 
            ");";

        char* msgErr;
    
        if (sqlite3_exec(db, query.c_str(), NULL, 0, &msgErr) != SQLITE_OK) { 
            Logger() << "Error insert into table:" <<  msgErr;
            sqlite3_free(msgErr); 
            throw std::invalid_argument("error insert into table");
        }
        
        sqlite3_close(db);
    }
    catch (...)
    {
        sqlite3_close(db);
        throw;
    }

    // check time and empty table if it's midnight
    time_t now = time(0);
    struct tm * timeinfo = localtime(&now);

    if (timeinfo->tm_hour == 0 && timeinfo->tm_min == 0 && timeinfo->tm_sec < 10)
    {
        Logger() << "Emptying table";
        empty_table();
    }
}

void Database::empty_table()
{
    sqlite3 *db;

    if (sqlite3_open(m_db_path.c_str(), &db))
    {
        Logger() << "Can't open database: " << sqlite3_errmsg(db);
        throw std::invalid_argument("cannot open database");
    }

    try
    {
        std::string query =
            "DELETE FROM temperature_serre "
            "WHERE date(received_time) < date('now', '-1 month');";

        char* msgErr;
    
        if (sqlite3_exec(db, query.c_str(), NULL, 0, &msgErr) != SQLITE_OK) { 
            Logger() << "Error delete from table:" <<  msgErr;
            sqlite3_free(msgErr);
        }
        
        sqlite3_close(db);
    }
    catch (...)
    {
        sqlite3_close(db);
    }
}

//// Server ////
Server::Server(int port)
{
    // HTTP-server at port PORT using 1 thread
    // Unless you do more heavy non-threaded processing in the resources,
    // 1 thread is usually faster than several threads
    m_server.config.port = port;

    m_server.resource["^/kpi-temp$"]["GET"] = [this]
        (std::shared_ptr<HttpServer::Response> response, 
         std::shared_ptr<HttpServer::Request> request) 
    {
        (void)request;
        std::stringstream stream = get_kpi_temp();
        response->write(stream);
    };

    m_server.resource["^/graph-temp$"]["GET"] = [this]
        (std::shared_ptr<HttpServer::Response> response, 
         std::shared_ptr<HttpServer::Request> request) 
    {
        (void)request;
        std::stringstream stream = get_graph_temp();
        response->write(stream);
    };

    m_server.resource["^/alert$"]["GET"] = [this]
        (std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) 
    {
        (void)request;
        std::stringstream stream = get_alarm_status();
        response->write(stream);
    };

    m_server.resource["^/buzzer$"]["GET"] = [this]
        (std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) 
    {
        (void)request;
        std::stringstream stream = get_alarm_enabled();        
        response->write(stream);
    };

    m_server.resource["^/buzzer$"]["POST"] = [this]
        (std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) 
    {
        auto content = request->content.string();
        set_alarm_enabled(content);
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
                  << content;
    };

    // Default GET-example. If no other matches, this anonymous function will be called.
    // Will respond with content in the web/-directory, and its subdirectories.
    // Default file: index.html
    // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    m_server.default_resource["GET"] = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
        try
        {
            auto web_root_path = boost::filesystem::canonical("web");
            auto path = boost::filesystem::canonical(web_root_path / request->path);
            // Check if path is within web_root_path
            if (std::distance(web_root_path.begin(), web_root_path.end()) > std::distance(path.begin(), path.end()) ||
                !std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
                throw std::invalid_argument("path must be within root path");
            if (boost::filesystem::is_directory(path))
                path /= "index.html";

            SimpleWeb::CaseInsensitiveMultimap header;

            auto ifs = std::make_shared<std::ifstream>();
            ifs->open(path.string(), std::ifstream::in | std::ios::binary | std::ios::ate);

            if (*ifs)
            {
                auto length = ifs->tellg();
                ifs->seekg(0, std::ios::beg);

                header.emplace("Content-Length", to_string(length));
                response->write(header);

                // Trick to define a recursive function within this scope (for example purposes)
                class FileServer
                {
                  public:
                    static void read_and_send(const std::shared_ptr<HttpServer::Response> &response, const std::shared_ptr<std::ifstream> &ifs)
                    {
                        // Read and send 128 KB at a time
                        static std::vector<char> buffer(131072); // Safe when server is running on one thread
                        std::streamsize read_length;
                        if ((read_length = ifs->read(&buffer[0], static_cast<std::streamsize>(buffer.size())).gcount()) > 0)
                        {
                            response->write(&buffer[0], read_length);
                            if (read_length == static_cast<std::streamsize>(buffer.size()))
                            {
                                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                                    if (!ec)
                                        read_and_send(response, ifs);
                                    else
                                        std::cerr << "Connection interrupted" << std::endl;
                                });
                            }
                        }
                    }
                };
                FileServer::read_and_send(response, ifs);
            }
            else
                throw std::invalid_argument("could not read file");
        }
        catch (const std::exception &e)
        {
            response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
        }
    };

    m_server.on_error = [](std::shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
        // Handle errors here
        // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
    };
}

std::stringstream Server::get_kpi_temp()
{
    std::stringstream stream;

    std::string query = 
        "select min(temperature_celsius), max(temperature_celsius) "
        "from temperature_serre "
        "where received_time > date('now','-1 day');";

    float minTemp = -99;
    float maxTemp = -99;
    float currentTemp = -99;

    try {
        auto result = Database().query_db(query, 2);
        minTemp = std::stof(result[0][0].c_str());
        maxTemp = std::stof(result[1][0].c_str());
    }
    catch(...) {
        Logger() << "Error while retrieving temperatures min max";
        Logger().setState(Error::ERRWEBSERVER);
    }

    query = 
        "select temperature_celsius "
        "from temperature_serre "
        "where datetime(received_time) > datetime('now', '-10 minute', 'localtime') "
        "order by received_time desc "
        "limit 1;";

    try {
        auto result = Database().query_db(query, 1);
        currentTemp = std::stof(result[0][0].c_str());
    }
    catch(...) {
        Logger() << "Error while retrieving current temperature";
        Logger().setState(Error::ERRWEBSERVER);
    }

    stream << std::fixed << std::setw(2) << std::setprecision(2)
           << "{\"currentTemp\":" << currentTemp 
           << ", \"minTemp\":" << minTemp
           << ", \"maxTemp\":" << maxTemp << "}";
    
    return stream;
}

std::stringstream Server::get_graph_temp()
{
    std::stringstream stream;

    std::string query = 
        "select "
        "   datetime(strftime('%s', received_time) / 3600 * 3600, 'unixepoch', 'localtime'), "
        "   avg(temperature_celsius) "
        "from temperature_serre "
        "where received_time > date('now', '-7 day') "
        "group by 1 "
        "order by received_time asc;";

    stream << "{\"temps\":[";

    try {
        auto result = Database().query_db(query, 2);    

        for (size_t i = 0; i < result[0].size(); ++i)
        {
            if(i != 0) stream << ",";

            stream << std::fixed << std::setw(2) << std::setprecision(2)
                   << "{\"time\": \"" << result[0][i] << "\""
                   << ", \"temp\":" << std::stof(result[1][i].c_str()) << "}";
        }
    }
    catch (...) {
        Logger() << "Error while retrieving temperature graph";
        Logger().setState(Error::ERRWEBSERVER);
    }

    stream << "]}";

    return stream;
}

std::stringstream Server::get_alarm_status()
{
    std::stringstream stream;
    std::string state = "ok";

    switch(Logger().getState()){
        case Error::OK: state = "ok"; break;
        case Error::TEMPLOW: state = "tempLow"; break;
        case Error::ERRARDUINO: state = "errArduino"; break;
        case Error::ERRRASPBERRY: state = "errRaspberry"; break;
        case Error::ERRWEBSERVER: state = "errWebserver"; break;
    }

    stream << "{\"status\":\"" << state << "\"}";
    return stream;
}

std::stringstream Server::get_alarm_enabled()
{
    std::stringstream stream;

    if (Logger().getAlarmEnabled()){ stream << "{\"status\":\"activated\"}"; }
    else { stream << "{\"status\":\"disabled\"}"; }

    return stream;
}

void Server::set_alarm_enabled(std::string content)
{
    if (content == "{\"status\":\"off\"}") {
        Logger().setAlarmEnabled(false);
    }
    else {
        Logger().setAlarmEnabled(true);
    }
}

void Server::start()
{
    m_server_thread = std::make_unique<std::thread>(std::thread([this]() {
        m_server.start();
    }));
}

void Server::stop()
{
    m_server_thread->join();
}

//// ZMQ Receiver ////
ZmqReceiver::ZmqReceiver(int port): m_context(1), m_port(port)
{
    initialize_socket();
}

void ZmqReceiver::initialize_socket()
{
    m_socket = std::make_unique<zmq::socket_t>(m_context, ZMQ_REP);
    m_socket->bind("tcp://*:" + std::to_string(m_port));
}

void ZmqReceiver::communicate()
{
    while (true) {
        //  Wait for next request from client
        zmq::message_t request;

        m_socket->recv(&request);
        
        // We need to remove unuseful characters at the end of the string
        std::string req_corrupt = std::string(static_cast<char*>(request.data()), request.size());
        std::size_t req_corrupt_end = req_corrupt.find("}");
        
        if (req_corrupt_end == std::string::npos)
        {
            Logger() << "Received badly formatted message";
            Logger().setState(Error::ERRRASPBERRY);
            return;
        }

        std::istringstream req(req_corrupt.substr(0, req_corrupt_end + 1));
        pt::ptree root;

        Logger() << req.str();
    
        pt::read_json(req, root);
        std::string timestamp = root.get<std::string>("datetime");
        float temperature = root.get<float>("temperature");
        std::string alarm_current = root.get<std::string>("alarm_current");

        if (alarm_current != "errArduino"){
            Database().insert_db(timestamp, temperature);
        }

        if (alarm_current == "tempLow")
        {
            Logger().setState(Error::TEMPLOW);
        }
        else if (alarm_current == "errArduino")
        {
            Logger().setState(Error::ERRARDUINO);
        }
        else
        {
            Logger().setState(Error::OK);
        }
        
        //  Send reply back to client
        zmq::message_t reply (5);
        snprintf((char *)reply.data (), 5, "%d", Logger().getAlarmEnabled());
        m_socket->send (reply);
    }
}

void ZmqReceiver::start()
{
    m_zmq_thread = std::make_unique<std::thread>(std::thread([this]() {
        communicate();
    }));
}

void ZmqReceiver::stop()
{
    m_zmq_thread->join();
}

int main(int argc, char *argv[])
{
    std::string config_file = "";

    po::options_description desc("Webserver for house-monitoring project");
    desc.add_options()
        ("help,h", "produce help message")
        ("config,c", po::value<std::string>(), "defines config file to use")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    if (vm.count("config")) {
        config_file = vm["config"].as<std::string>();
    } else {
        std::cout << "No configuration file was provided." << std::endl;
        return 1;
    }

    Config conf(config_file);
    Database().init(conf.getDbPath());
    Server server(conf.getWebserverPort());
    ZmqReceiver zmq_receiver(conf.getZmqPort());

    Logger() << "Starting webserver at port " << conf.getWebserverPort();
    server.start();

    Logger() << "Starting sensor listener at port " << conf.getZmqPort();
    zmq_receiver.start();

    zmq_receiver.stop();
    server.stop();

    return 0;
}
