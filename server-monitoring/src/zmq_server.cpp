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

int main (int argc, char *argv[]) 
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
    }

    

   

    return 0;
}
