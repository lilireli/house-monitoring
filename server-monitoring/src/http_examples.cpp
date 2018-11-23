#include "client_http.hpp"
#include "server_http.hpp"

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

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

std::vector<std::vector<std::string>> query_db(std::string query, int nb_cols)
{
    sqlite3 *db;
    sqlite3_stmt * stmt;
    std::vector<std::vector<std::string>> result;
    
    int exit = 0; 
    exit = sqlite3_open("test.db", &db); 

    if (exit)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        fprintf(stderr, "Opened database successfully\n");
        char* messaggeError; 
        
        sqlite3_prepare( db, query.c_str(), -1, &stmt, NULL ); //preparing the statement
        sqlite3_step( stmt ); //executing the statement

        for( int i = 0; i < nb_cols; i++ )
            result.push_back(std::vector< std::string >());

        while( sqlite3_column_text( stmt, 0 ) )
        {
            for( int i = 0; i < nb_cols; i++ )
                result[i].push_back( std::string( (char *)sqlite3_column_text( stmt, i ) ) );
            sqlite3_step( stmt );
        }
    }

    std::cout << "exit" << std::endl;
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return result;
}

stringstream get_kpi_temp(float currentTemp)
{
    stringstream stream;

    std::string query = "select min(temperature_celsius), max(temperature_celsius) from temperature_serre where received_time > date('now','-1 day');";
    // TODO: add where on 24h

    auto result = query_db(query, 2);

    // TODO: handle error
    float minTemp = std::stof(result[0][0].c_str());
    float maxTemp = std::stof(result[1][0].c_str());

    stream << std::fixed << std::setw(2) << std::setprecision(2)
           << "{\"currentTemp\":" << currentTemp 
           << ", \"minTemp\":" << minTemp
           << ", \"maxTemp\":" << maxTemp << "}";
    
    return stream;
}

stringstream get_graph_temp()
{
    stringstream stream;

    std::string query = "select datetime(strftime('%s', received_time) / 3600 * 3600, 'unixepoch', 'localtime'), avg(temperature_celsius) from temperature_serre where received_time > date('now', '-7 day') group by 1 order by received_time asc;";
    // TODO: add where 7j, TODO: add agg 7days

    auto result = query_db(query, 2);

    // TODO: handle error no rep

    stream << "{\"temps\":[";

    for (size_t i = 0; i < result[0].size(); ++i)
    {
        if(i != 0)
            stream << ",";

        stream << std::fixed << std::setw(2) << std::setprecision(2)
               << "{\"time\": \"" << result[0][i] << "\""
               << ", \"temp\":" << std::stof(result[1][i].c_str()) << "}";
    }
    
    stream << "]}";

    return stream;
}

stringstream get_alarm_status()
{
    stringstream stream;
    stream << "{\"status\":\"KO\", \"desc\":\"Température trop basse\"}";
    return stream;
}

int main()
{
    // HTTP-server at port 8080 using 1 thread
    // Unless you do more heavy non-threaded processing in the resources,
    // 1 thread is usually faster than several threads
    HttpServer server;
    server.config.port = 8080;

    float currentTemp = 3.26;

    server.resource["^/kpi-temp$"]["GET"] = [currentTemp]
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        std::cout << "bla" << std::endl;
        stringstream stream = get_kpi_temp(currentTemp);
        response->write(stream);
    };

    server.resource["^/graph-temp$"]["GET"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        std::cout << "bla" << std::endl;
        stringstream stream = get_graph_temp();
        std::cout << "bla2" << std::endl;
        response->write(stream);
        std::cout << "bla3" << std::endl;
    };

    server.resource["^/alert$"]["GET"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        std::cout << "bla" << std::endl;
        stringstream stream = get_alarm_status();
        response->write(stream);
    };

    server.resource["^/buzzer$"]["GET"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        stringstream stream;
        std::cout << "bla" << std::endl;

        stream << "{\"status\":\"activated\"}";

        response->write(stream);
    };

    server.resource["^/buzzer$"]["POST"] = []
        (shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) 
    {
        auto content = request->content.string();

        std::cout << content << std::endl;

        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
                  << content;
    };

    // Default GET-example. If no other matches, this anonymous function will be called.
    // Will respond with content in the web/-directory, and its subdirectories.
    // Default file: index.html
    // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
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

    server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
        // Handle errors here
        // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
    };

    thread server_thread([&server]() {
        // Start server
        server.start();
    });

    // std::cout << "Server started on port " << server.config.port << std::endl;

    server_thread.join();
}
