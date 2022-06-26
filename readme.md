# House-Monitoring

This project contains the code and the schematics (3D printer code and electronic diagrams) for a deported temperature sensor.
It contains three parts:

- An Arduino sensor which contains a temperature sensor and a Lora transmitter,
- A Raspberry which contains a Lora receiver to get the data from the Arduino, and which sends it to a webserver,
- A Webserver which remembers past temperatures and displays it.

An example of usecase can be found in a family house where a wintergarden is at the back of the house, quite far away. 
However, it is necessary to monitor the temperature of this glass house. 
Thus, the Arduino is placed in the glass house, and the raspberry in the house, to control from afar the temperature. 
When not at home, the temperature can still be monitored with the web application.

## Sensor Transmitter
This is the Arduino part.

It contains:
- an arduino Nano
- a temperature sensor
- a Lora transmitter
- a LED (to check the device is working)

The code is done in C++, and compiled with Platformio.

## Receiver Transmitter
This is the Raspberry part.

It contains:
- a Raspberry
- a Lora receiver
- a buzzer (to alarm people when the temperature gets dangerous for the plants!)
- a LED screen (to print the current temperature and lowest temperature in the past 24 hours)

The code is done in C++.

## Server Monitoring
This is the webserver.

Initially it was done in C++, Vue.js (server-monitoring), in its new version it is in Python Django, React (server-monitoring2).

Installation with Python is done with virtualenv.

```bash
cd server-monitoring2
virtualenv venv
source venv/bin/activate
pip install requirements.txt
```

The application is currently running on Heroku
- To run locally https://devcenter.heroku.com/articles/getting-started-with-python#run-the-app-locally
- To deploy https://devcenter.heroku.com/articles/getting-started-with-python#deploy-the-app