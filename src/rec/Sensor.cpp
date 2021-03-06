/*
EM: Electronic Monitor - Control Box Software
Copyright (C) 2012 Ecotrust Canada
Knowledge Systems and Planning

This file is part of EM.

EM is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EM. If not, see <http://www.gnu.org/licenses/>.

You may contact Ecotrust Canada via our website http://ecotrust.ca
*/

#include "Sensor.h"
#include "output.h"

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

Sensor::Sensor(const char* _moduleName, unsigned long _NO_CONNECTION, unsigned long _NO_DATA):StateMachine(_NO_CONNECTION, false) {
    moduleName = string(_moduleName);
    state = _NO_CONNECTION;
    NO_CONNECTION = _NO_CONNECTION;
    NO_DATA = _NO_DATA;
}

void Sensor::SetPort(const char* aPort) {
    serialPort = aPort;
}

void Sensor::SetBaudRate(int aBaudRate) {
    baudRate = aBaudRate;
}

int Sensor::Connect() {
    static bool silenceConnectErrors = false;
    termios options;

    SetState(NO_CONNECTION);

    for(int i = 0; GetState() & NO_CONNECTION && i < MAX_TRY_COUNT; i++) {
        if(i > 0) usleep(SENSOR_RECONNECT_DELAY);

        serialHandle = open(serialPort, O_RDWR | O_NOCTTY | O_NDELAY); // opens port

        if (serialHandle == -1) {
            if(!silenceConnectErrors) {
                E("Failed to open port '" + serialPort + "'");
            }
            continue;
        }

        // enable blocking I/O (read() blocks until bytes available)
        if(fcntl(serialHandle, F_SETFL, 0) == -1) {
            if(!silenceConnectErrors) {
                E("Failed SETFL");
            }
            continue;
        }

        // fill options struct
        if(tcgetattr(serialHandle, &options) == -1) {
            if(!silenceConnectErrors) {
                E("Failed to get serial config");
            }
            continue;
        }

        if(cfsetispeed(&options, baudRate) == -1) { // sets baud rates
            if(!silenceConnectErrors) {
                E("Failed to set input baud rate");
            }
            continue;
        }

        if(cfsetospeed(&options, baudRate) == -1) { // sets baud rates
            if(!silenceConnectErrors) {
                E("Failed to set output baud rate");
            }
            continue;
        }

        // no parity 8N1 mode
        options.c_cflag &= ~PARENB;  // sets some default communication flags
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;

        // disable hardware flow control
        options.c_cflag &= ~CRTSCTS;

        // disable hang-up on close to avoid reset
        options.c_cflag &= ~HUPCL;
        //options.c_cflag |= HUPCL;

        // turn on READ & ignore ctrl lines
        options.c_cflag |= CREAD | CLOCAL;
        
        options.c_lflag &= ~ISIG; // disable signals
        options.c_lflag &= ~ICANON; // disable canonical input
        options.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ECHOCTL | ECHOPRT | ECHOKE); // disable all echoing

        // disable input parity checking, marking, and character mapping
        options.c_iflag &= ~(INPCK | PARMRK | INLCR | ICRNL | IUCLC);

        // turn off s/w flow ctrl; maybe comment this out for non-GPS sensors
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        
        // raw output
        options.c_oflag &= ~OPOST;
        
        options.c_cc[VMIN]  = 0;
        options.c_cc[VTIME] = 20;

        if(tcsetattr(serialHandle, TCSANOW, &options) == -1) {
            if(!silenceConnectErrors) {
                E("Failed to set config");
            }
            continue;
        }

        UnsetState(NO_CONNECTION);
        O("Connected");
    }

    if(GetState() & NO_CONNECTION && (!silenceConnectErrors || OVERRIDE_SILENCE)) {
        E("Failed to connect after " + std::to_string(MAX_TRY_COUNT) + " tries; will keep at it but silencing further Connect() errors");
        silenceConnectErrors = true;
        return -1;
    }

    return 0;
}

int Sensor::Send(const char* cmd) {
    int bytesWritten = write(serialHandle, cmd, strlen(cmd));
    
    if(bytesWritten == -1) {
        SetState(NO_CONNECTION);
    } else {
        UnsetState(NO_CONNECTION);
    }

    return bytesWritten;
}

int Sensor::Receive(char buf[], int min, int max, bool set_no_data) {
    if(GetState() & NO_CONNECTION) {
        if(Connect() != 0) {
            // couldn't connect
            return -1;
        }
    }

    int bytesToRead = 0, bytesRead = 0;
    int status = ioctl(serialHandle, FIONREAD, &bytesToRead); // check how many bytes are waiting to be read

    if (status == -1) {
        SetState(NO_CONNECTION);
        return status;
    } else {
        UnsetState(NO_CONNECTION);
    }

    if(bytesToRead > max) bytesToRead = max;

    if(bytesToRead >= min) {
        bytesRead = read(serialHandle, buf, bytesToRead);
        buf[bytesRead] = '\0'; // null terminate the data.
        UnsetState(NO_DATA);
    } else {
        if (set_no_data) SetState(NO_DATA);
    }

    return bytesRead;
}

void Sensor::Close() {
    close(serialHandle);
    SetState(NO_CONNECTION);
}

bool Sensor::isConnected() {
    if (GetState() & NO_CONNECTION) return false;

    return true;
}