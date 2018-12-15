#include "webserver.hpp"

using namespace std; // TODO: remove it !
// Added for the json-example:
namespace pt = boost::property_tree;
namespace po = boost::program_options;

std::map<std::string, std::string> errors = {
    {"ok",              "{\"OK\":\"\"}"},
    {"tempLow",         "{\"KO\":\"Temperature too low\"}"},
    {"errArduino",      "{\"KO\":\"Internal sensor problem\"}" },
    {"errRaspberry",    "{\"KO\":\"Internal transmitter problem\"}" },
    {"errWebserver",    "{\"KO\":\"Internal webserver problem\"}" }
};

std::string Logger::m_state = "ok";
std::string Database::m_db_path = "";

Config::Config(std::string config_file)
{
    pt::ptree root;
    
    pt::read_json(config_file, root);
    m_webserver_port = root.get<int>("webserver_port", 0);
    m_zmq_port = root.get<int>("zmq_port", 0);
    m_db_path = root.get<std::string>("database");
}

std::vector<std::vector<std::string>> Database::query_db(std::string query, int nb_cols)
{
    sqlite3 *db;
    sqlite3_stmt * stmt;
    std::vector<std::vector<std::string>> result;
    
    int exit = 0; 
    exit = sqlite3_open(m_db_path.c_str(), &db);

    if (exit)
    {
        Logger() << "Can't open database: " << sqlite3_errmsg(db);
        Logger().setState("webserver");
    }
    else
    {     
        sqlite3_prepare( db, query.c_str(), -1, &stmt, NULL ); 
        sqlite3_step( stmt ); //executing the statement

        for( int i = 0; i < nb_cols; i++ )
            result.push_back(std::vector< std::string >());

        if (!sqlite3_column_text(stmt, 0))
        {
            Logger() << "Query did not match any data";
            throw std::invalid_argument("query did not match any data");
        }

        while(sqlite3_column_text(stmt, 0))
        {
            for(int i = 0; i < nb_cols; i++)
                result[i].push_back(std::string((char *)sqlite3_column_text(stmt, i)));
            sqlite3_step( stmt );
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return result;
}

void Database::insert_db(std::string timestamp, float temperature)
{
    sqlite3 *db;
    std::string sqlstatement =
        "INSERT INTO temperature_serre(received_time, temperature_celsius) VALUES ('"
        + timestamp + "',"
        + std::to_string(temperature) + ");";

    std::cout << sqlstatement << std::endl;
    
    int exit = 0; 
    exit = sqlite3_open(m_db_path.c_str(), &db); 

    if (exit)
    {
        std::cout << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    }
    else
    {
        char* messaggeError; 
        exit = sqlite3_exec(db, sqlstatement.c_str(), NULL, 0, &messaggeError); 
    
        if (exit != SQLITE_OK) { 
            std::cerr << "Error Insert Into Table:" 
                      <<  messaggeError << std::endl;
            sqlite3_free(messaggeError); 
        } 
        else
            std::cout << "Insert into table successful" << std::endl; 
    }
    
    sqlite3_close(db);
}

stringstream get_kpi_temp()
{
    stringstream stream;

    std::string query = "select min(temperature_celsius), max(temperature_celsius) from temperature_serre where received_time > date('now','-1 day');";

    float minTemp = -99;
    float maxTemp = -99;
    float currentTemp = -99;

    try {
        auto result = Database().query_db(query, 2);
        minTemp = std::stof(result[0][0].c_str());
        maxTemp = std::stof(result[1][0].c_str());
    }
    catch (int e) {
        Logger() << "Error while retrieving temperatures min max";
        Logger().setState("webserver");
    }

    query = "select temperature_celsius from temperature_serre where datetime(received_time) > datetime('now', '-10 minute', 'localtime') order by received_time desc limit 1;";

    try {
        auto result = Database().query_db(query, 1);
        currentTemp = std::stof(result[0][0].c_str());
    }
    catch (int e) {
        Logger() << "Error while retrieving current temperature";
        Logger().setState("webserver");
    }

    stream << std::fixed << std::setw(2) << std::setprecision(2)
           << "{\"currentTemp\":" << currentTemp 
           << ", \"minTemp\":" << minTemp
           << ", \"maxTemp\":" << maxTemp << "}";
    
    return stream;
}

stringstream get_graph_temp()
{
    std::stringstream stream;

    std::string query = "select datetime(strftime('%s', received_time) / 3600 * 3600, 'unixepoch', 'localtime'), avg(temperature_celsius) from temperature_serre where received_time > date('now', '-7 day') group by 1 order by received_time asc;";

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
    catch (int e) {
        Logger() << "Error while retrieving temperature graph";
        Logger().setState("webserver");
    }

    stream << "]}";

    return stream;
}

stringstream get_alarm_status()
{
    stringstream stream;
    stream << "{\"status\":\"" << Logger().getState() << "\"}";
    return stream;
}

void Server::start(int port)
{
    // HTTP-server at port PORT using 1 thread
    // Unless you do more heavy non-threaded processing in the resources,
    // 1 thread is usually faster than several threads
    m_server.config.port = port;

    Logger() << "Server started on port " << m_server.config.port;

    m_server.resource["^/kpi-temp$"]["GET"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        stringstream stream = get_kpi_temp();
        response->write(stream);
    };

    m_server.resource["^/graph-temp$"]["GET"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        stringstream stream = get_graph_temp();
        response->write(stream);
    };

    m_server.resource["^/alert$"]["GET"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        stringstream stream = get_alarm_status();
        response->write(stream);
    };

    m_server.resource["^/buzzer$"]["GET"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        stringstream stream;
        stream << "{\"status\":\"activated\"}";
        response->write(stream);
    };

    m_server.resource["^/buzzer$"]["POST"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        auto content = request->content.string();

        Logger() << content;

        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
                  << content;
    };

    // Default GET-example. If no other matches, this anonymous function will be called.
    // Will respond with content in the web/-directory, and its subdirectories.
    // Default file: index.html
    // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    m_server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        try
        {
            auto web_root_path = boost::filesystem::canonical("web");
            auto path = boost::filesystem::canonical(web_root_path / request->path);
            // Check if path is within web_root_path
            if (distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
                !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
                throw invalid_argument("path must be within root path");
            if (boost::filesystem::is_directory(path))
                path /= "index.html";

            SimpleWeb::CaseInsensitiveMultimap header;

            auto ifs = make_shared<ifstream>();
            ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

            if (*ifs)
            {
                auto length = ifs->tellg();
                ifs->seekg(0, ios::beg);

                header.emplace("Content-Length", to_string(length));
                response->write(header);

                // Trick to define a recursive function within this scope (for example purposes)
                class FileServer
                {
                  public:
                    static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs)
                    {
                        // Read and send 128 KB at a time
                        static vector<char> buffer(131072); // Safe when server is running on one thread
                        streamsize read_length;
                        if ((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0)
                        {
                            response->write(&buffer[0], read_length);
                            if (read_length == static_cast<streamsize>(buffer.size()))
                            {
                                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                                    if (!ec)
                                        read_and_send(response, ifs);
                                    else
                                        cerr << "Connection interrupted" << endl;
                                });
                            }
                        }
                    }
                };
                FileServer::read_and_send(response, ifs);
            }
            else
                throw invalid_argument("could not read file");
        }
        catch (const exception &e)
        {
            response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
        }
    };

    m_server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
        // Handle errors here
        // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
    };

    m_server_thread = std::make_shared<std::thread>(std::thread([this]() {
        // Start server
        m_server.start();
    }));
}

void Server::stop()
{
    m_server_thread->join();
}

void communicate_sensor(int port)
{
    // Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:" + std::to_string(port));

    while (true) {
        //  Wait for next request from client
        zmq::message_t request;
        float temperature;
        std::string timestamp;

        socket.recv(&request);

        std::istringstream iss(static_cast<char*>(request.data()));
        iss >> timestamp >> temperature ;
        // TODO: add alarm state
        std::cout << "Received at " << timestamp 
                  << " temp is " << temperature << std::endl;

        Database().insert_db(timestamp, temperature);

        //  Send reply back to client
        zmq::message_t reply (5);
        memcpy (reply.data (), "World", 5);
        socket.send (reply);

        // TODO: mechanism to empty table every once or so
    }
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

    std::cout << conf.getWebserverPort() << std::endl;

    Server server;

    Logger() << "Starting webserver at port " << conf.getWebserverPort();
    server.start(conf.getWebserverPort());

    Logger() << "Starting sensor listener at port " << conf.getZmqPort();
    std::thread thread_sensor(communicate_sensor, conf.getZmqPort());

    thread_sensor.join();
    server.stop();

    return 0;
}
