#include "webserver.hpp"

using namespace std; // TODO: remove it !
// Added for the json-example:
using namespace boost::property_tree;

std::string Logger::m_state = "ok";

std::vector<std::vector<std::string>> query_db(std::string query, int nb_cols)
{
    sqlite3 *db;
    sqlite3_stmt * stmt;
    std::vector<std::vector<std::string>> result;
    
    int exit = 0; 
    exit = sqlite3_open(DATABASE, &db);

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

stringstream get_kpi_temp()
{
    stringstream stream;

    std::string query = "select min(temperature_celsius), max(temperature_celsius) from temperature_serre where received_time > date('now','-1 day');";

    float minTemp = -99;
    float maxTemp = -99;
    float currentTemp = -99;

    try {
        auto result = query_db(query, 2);
        minTemp = std::stof(result[0][0].c_str());
        maxTemp = std::stof(result[1][0].c_str());
    }
    catch (int e) {
        Logger() << "Error while retrieving temperatures min max";
        Logger().setState("webserver");
    }

    query = "select temperature_celsius from temperature_serre where datetime(received_time) > datetime('now', '-10 minute', 'localtime') order by received_time desc limit 1;";

    try {
        auto result = query_db(query, 1);
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
        auto result = query_db(query, 2);    

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

void Server::start()
{
    // HTTP-server at port PORT using 1 thread
    // Unless you do more heavy non-threaded processing in the resources,
    // 1 thread is usually faster than several threads
    m_server.config.port = PORT;

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

int main()
{
    Server server;
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REQ);

    Logger() << "Starting webserver at port " << PORT;
    server.start();

    Logger() << "Connecting to communicator at port 5556…";
    socket.connect ("tcp://localhost:5556");

    while (true) {
        // Send message
        zmq::message_t request(20);
        snprintf ((char *) request.data(), 20 , "How are you");
        socket.send (request);

        //  Get the reply.
        zmq::message_t reply;
        socket.recv (&reply);
        Logger() << "Received " << reply.data();

        // set state with reply from server
        Logger().setState("ok");

        std::this_thread::sleep_for(std::chrono::minutes(1));
    }

    server.stop();

    return 0;
}
