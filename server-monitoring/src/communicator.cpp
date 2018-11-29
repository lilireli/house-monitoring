//
//  Hello World server in C++
//  Binds REP socket to tcp://*:5555
//  Expects "Hello" from client, replies with "World"
//
#include <zmq.hpp>
#include <sqlite3.h> 
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>

std::map<std::string, std::string> errors = {
    {"ok",              "{\"OK\":\"\"}"},
    {"tempLow",         "{\"KO\":\"Temperature too low\"}"},
    {"errArduino",      "{\"KO\":\"Internal sensor problem\"}" },
    {"errRaspberry",    "{\"KO\":\"Internal transmitter problem\"}" },
    {"errWebserver",    "{\"KO\":\"Internal webserver problem\"}" }
};

void insert_db(std::string timestamp, float temperature)
{
    sqlite3 *db;
    std::string sqlstatement =
        "INSERT INTO temperature_serre(received_time, temperature_celsius) VALUES ('"
        + timestamp + "',"
        + std::to_string(temperature) + ");";

    std::cout << sqlstatement << std::endl;
    
    int exit = 0; 
    exit = sqlite3_open("test.db", &db); 

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

void communicate_sensor()
{
    // Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:5555"); 

    while (true) {
        //  Wait for next request from client
        zmq::message_t request;
        float temperature;
        std::string timestamp;

        socket.recv(&request);

        std::istringstream iss(static_cast<char*>(request.data()));
        iss >> timestamp >> temperature ;
        std::cout << "Received at " << timestamp 
                  << " temp is " << temperature << std::endl;

        insert_db(timestamp, temperature);

        //  Send reply back to client
        zmq::message_t reply (5);
        memcpy (reply.data (), "World", 5);
        socket.send (reply);

        // mechanism to empty table every once or so
    }
}

void communicate_webserver()
{
    std::string state = errors["ok"];

    // Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:5556"); 

    while (true) {
        //  Wait for next request from client
        zmq::message_t request;
        std::string state_server;
        
        socket.recv(&request);
        std::istringstream iss(static_cast<char*>(request.data()));
        iss >> state_server ;
        std::cout << "Received from webserver " << state_server << std::endl;

        //  Send reply back to client
        zmq::message_t reply (20);
        memcpy(reply.data (), state.c_str(), 20);
        socket.send (reply);
    }
}

int main (int argc, char *argv[]) 
{
    std::thread thread_sensor(communicate_sensor);
    std::thread thread_webserver(communicate_webserver);

    thread_sensor.join();
    thread_webserver.join();

    return 0;
}
