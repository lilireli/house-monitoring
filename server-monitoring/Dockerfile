# Use an official Python runtime as a parent image
FROM ubuntu:16.04
MAINTAINER Aurelie Suzanne

# Install libraries
RUN apt-get update -y

# Set the locale
# RUN locale-gen fr_FR.UTF-8  
# ENV LANG fr_FR.UTF-8  
# ENV LANGUAGE fr_FR:fr 
# ENV LC_ALL fr_FR.UTF-8

# Set the working directory to /app
WORKDIR /app

# Copy the current directory contents into the container at /app
COPY . /app

# Install any needed packages specified in requirements.txt
# RUN pip install --trusted-host pypi.python.org -r requirements.txt

# Make port 80 available to the world outside this container
EXPOSE 8080

# Define environment variable
ENV NAME World

# Run app.py when the container launches
RUN apt-get install libboost-all-dev -y
CMD ./build/webserver
