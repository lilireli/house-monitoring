//
//  Hello World client in C++
//  Connects REQ socket to tcp://localhost:5555
//  Sends "Hello" to server, expects "World" back
//
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctime>

#define within(num) (float) ((float) num * random () / (RAND_MAX + 1.0))

int main (int argc, char *argv[])
{
    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REQ);

    

    std::cout << "Connecting to hello world server…" << std::endl;
    socket.connect ("tcp://localhost:8001");

    //  Initialize random number generator
    srandom ((unsigned) time (NULL));

    //  Do 10 requests, waiting each time for a response
    for (int request_nbr = 0; request_nbr != 10; request_nbr++) {
        // check datetime
        time_t now = time(0);
        struct tm * timeinfo = localtime(&now);
        char buffer[30];

        strftime(buffer,30,"%Y-%m-%dT%H:%M:%S",timeinfo);

        std::cout << "The local date and time is: " << buffer << std::endl;

        float temperature;

        //  Get values that will fool the boss
        temperature = within(215) - 80;

        //  Send message to all subscribers
        zmq::message_t request(40);
        snprintf ((char *) request.data(), 40 ,
            "%s %2.2f", buffer, temperature);
        socket.send (request);

        //  Get the reply.
        zmq::message_t reply;
        socket.recv (&reply);
        std::cout << "Received " << reply.data() << request_nbr << std::endl;
    }

    return 0;
}
