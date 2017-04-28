/*
 * This file is modified, and part of the Doodle3D project (http://doodle3d.com).
 *
 * Original can be found here: https://github.com/Doodle3D/print3d
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include "Serial.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "../utilities/utilities.h"
#include "../../utils/Logger/Logger.h"

#ifdef __APPLE__

#if (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4) /* require at least 10.4 (OSX Tiger) */
#include <termios.h> /* cfmakeraw */
#include <sys/ioctl.h> /* ioctl */

#include <IOKit/serial/ioss.h>

#define TERMIOS_TYPE termios
#else
#error "cannot set serial port speed on OSX versions below 10.4 (Tiger)"
#endif
#elif __linux
#include <linux/serial.h>
#include <linux/termios.h>

#ifdef __cplusplus
extern "C" {
#endif
/** Declarations we need. Using the proper includes creates conflicts with those
 * necessary for termios2.
 */
void cfmakeraw(struct termios2* termios_p);
int ioctl(int fildes, int request, ... /* arg */);
int tcflush(int fildes, int action);

#ifdef __cplusplus
} //extern "C"
#endif

#define TERMIOS_TYPE termios2
#else
#error "cannot set serial port speed for this OS"
#endif

using std::string;

int extractNumber(string code)
{
    int sequenceNumber = -1;

    try {
        std::size_t positionSpace = code.find(" ", 0);
        std::string resendNumberString;

        if (positionSpace != std::string::npos) {
            resendNumberString = code.substr(1, positionSpace - 1);
        }
        else {
            resendNumberString = code.substr(1);
        }

        sequenceNumber = stoi(resendNumberString);
    }

    catch (...) {
        Logger::GetInstance()->logMessage("Exception caught extracting message line number", 1, 0);
        sequenceNumber = -1;
    }

    return sequenceNumber;
}

string addChecksum(string message)
{

    string command = std::string(message);
    size_t posT = (command.find(";"));

    try {
        if (posT != std::string::npos) {
            command = command.substr(0, posT);
        }
    }

    catch (...) {
        std::cout << "substr exception at checksum" << std::endl;
        ;
    }

    const char* cmd = command.c_str();

    int cs = 0;
    for (int i = 0; cmd[i] != '*' && cmd[i] != NULL; i++)
        cs = cs ^ cmd[i];
    cs &= 0xff; // defensive programming

    string finalMessage = command + "*" + std::to_string(cs);

    return finalMessage;
}

const int Serial::READ_BUF_SIZE = 1024;

Serial::Serial()
    : portFd_(-1)
    , buffer_(0)
    , bufferSize_(0)
    , fake(false)
{
}

int Serial::open(const char* file)
{
    portFd_ = ::open(file, O_RDWR | O_NONBLOCK);
    if (portFd_ < 0) {
        return portFd_;
    }

    return 0;
}

int Serial::close()
{
    int rv = 0;

    if (portFd_ >= 0) {
        rv = ::close(portFd_);
        portFd_ = -1;
    }

    return rv;
}

bool Serial::isOpen() const
{
    return portFd_ > -1;
}

Serial::SET_SPEED_RESULT Serial::setSpeed(int speed)
{
    struct TERMIOS_TYPE options;
    int modemBits;

#ifdef __APPLE__
    if (tcgetattr(portFd_, &options) < 0)
        return SSR_IO_GET;
#else
    if (ioctl(portFd_, TCGETS2, &options) < 0)
        return SSR_IO_GET;
#endif

    cfmakeraw(&options);

    // Enable the receiver
    options.c_cflag |= CREAD;

    // Clear handshake, parity, stopbits and size
    options.c_cflag &= ~CLOCAL;
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;

    //FIXME: why is CLOCAL disabled above and re-enabled again here?
    options.c_cflag |= CS8;
    options.c_cflag |= CLOCAL;

//set speed
#ifdef __APPLE__
    //first set speed to 9600, then after tcsetattr set custom speed (as per ofxSerial addon)
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);

#elif __linux
    options.c_ospeed = options.c_ispeed = speed;
    options.c_cflag &= ~CBAUD;
    options.c_cflag |= BOTHER;
#endif

#ifdef __APPLE__
    if (tcsetattr(portFd_, TCSANOW, &options) < 0)
        return SSR_IO_SET;
    if (ioctl(portFd_, IOSSIOSPEED, &speed) == -1)
        return SSR_IO_IOSSIOSPEED;
#else
    if (ioctl(portFd_, TCSETS2, &options) < 0)
        return SSR_IO_SET;
#endif

    //toggle DTR
    if (ioctl(portFd_, TIOCMGET, &modemBits) < 0)
        return SSR_IO_MGET;
    modemBits |= TIOCM_DTR;
    if (ioctl(portFd_, TIOCMSET, &modemBits) < 0)
        return SSR_IO_MSET1;
    usleep(100 * 1000);
    modemBits &= ~TIOCM_DTR;
    if (ioctl(portFd_, TIOCMSET, &modemBits) < 0)
        return SSR_IO_MSET2;

    return SSR_OK;
}

bool Serial::send(const char* code)
{
    string command = std::string(code);


    // For testing, errors are forced

    //	counter++;
    //	if(counter % 15==0)
    //	{
    //		std::cout << "FAKING COMMAND v1" << std::endl;
    //
    //		int number = extractNumber(command);
    //		command="N"+std::to_string(number)+ " None ";
    //
    //		command = addChecksum(command);
    //		command = command + "\n";
    //		code=command.c_str();
    //	}
    //
    //	else if(counter % 31==0)
    //	{
    //		std::cout << "FAKING COMMAND v2" << std::endl;
    //
    //		int number = extractNumber(command);
    //		command="N"+std::to_string(number)+ " HOLA ";
    //		command = addChecksum(command);
    //		command = command + "\n";
    //		code=command.c_str();
    //	}

    size_t posT = (command.find(";"));

    if (posT != std::string::npos) {
        //std::cout << "Sending command ---->\" "<< command << std::endl;

        Logger::GetInstance()->logMessage("Sending command ----> " + command, 3, 0);
        command = command.substr(0, posT);
        command = command + "\n";

        //std::cout << "Sending command ---->\" "<< command << std::endl;
        const char* cod = command.c_str();

        if (portFd_ >= 0) {
            int returnValue = ::write(portFd_, cod, strlen(cod));

            if (returnValue < 0) {
//				std::cout << "Something went wrong sending last command. ERRNO number: " << errno << std::endl;
                returnValue = ::write(portFd_, cod, strlen(cod));
//				std::cout << " \r Second iteration: " << returnValue << " / "<< strlen(cod) << std::endl;
                if (returnValue < 0) {
//					std::cout << "Something went wrong sending command again. ERRNO number: " << errno << std::endl;
                    return false;
                }
                else
                    return true;
            }
            else
                return true;
        }
        else {
            Logger::GetInstance()->logMessage("ERROR: PortFd < 0 " + command, 1, 0);
            return false;
        }
    }
    else {
        Logger::GetInstance()->logMessage("Sending command ----> " + command, 3, 0);
//		std::cout << "Sending command ---->\"" << code ;//<< std::endl;
        if (portFd_ >= 0) {
            int returnValue = ::write(portFd_, code, strlen(code));
//			std::cout << " \r RESULT: " << returnValue << " / "<< strlen(code) << std::endl;

            if (returnValue < 0) {
//				std::cout << "Something went wrong sending last command. ERRNO number: " << errno << std::endl;
                returnValue = ::write(portFd_, code, strlen(code));
//				std::cout << " \r Second iteration: " << returnValue << " / "<< strlen(code) << std::endl;
                if (returnValue < 0) {
//					std::cout << "Something went wrong sending command again. ERRNO number: " << errno << std::endl;
                    return false;
                }
                else
                    return true;
            }
            else
                return true;
            return true;
        }
        else {
            Logger::GetInstance()->logMessage("ERROR: PortFd < 0 " + command, 1, 0);
            return false;
        }
    }
}

bool Serial::write(const unsigned char* data, size_t datalen)
{
    if (portFd_ >= 0) {

        ::write(portFd_, data, datalen);
        return true;
    }
    else {
        return false;
    }
}

bool Serial::write(const unsigned char b)
{
    if (portFd_ >= 0) {
        ::write(portFd_, &b, 1);
        return true;
    }
    else {
        return false;
    }
}

int Serial::readData(int timeout, bool onlyOnce)
{
    int rv = readAndAppendAvailableData(portFd_, &buffer_, &bufferSize_, timeout, onlyOnce ? 1 : 0);
    return rv;
}

//TODO: rename
int Serial::readDataWithLen(int len, int timeout)
{
    int startSize = bufferSize_;

    while (bufferSize_ < startSize + len) {
        int rv = readData(timeout, true); //read with timeout but do not retry (we do that ourselves using the while)

        if (rv < 0)
            return rv; //error occured
        else if (rv == 0)
            break; //nothing read within timeout, do 'normal' return
    }

    return bufferSize_ - startSize;
}

int Serial::readByteDirect(int timeout)
{
    unsigned char data = 0;
    int rv = readBytesDirect(&data, 1, timeout);

    if (rv == 0)
        return -2;
    else if (rv < 0)
        return rv;
    else
        return data;
}

int Serial::readBytesDirect(unsigned char* buf, size_t buflen, int timeout)
{
    struct pollfd pfd;
    pfd.fd = portFd_;
    pfd.events = POLLIN;

    while (true) {
        int rv = read(portFd_, buf, buflen);

        if (rv < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                //recv() would block...if a timeout has been requested, we wait and then try again if data became available
                pfd.revents = 0;
                if (timeout > 0)
                    poll(&pfd, 1, timeout);

                if ((pfd.revents & POLLIN) == 0)
                    return 0;
            }
            else if (errno != EINTR) {
                //ignore it if the call was interrupted (i.e. try again)
                return rv;
            }
        }
        else if (rv == 0) {
            return -2;
        }
        else {
            return rv;
        }
    }

    return 0;
}

char* Serial::getBuffer()
{
    return buffer_;
}

int Serial::getBufferSize() const
{
    return bufferSize_;
}

int Serial::getFileDescriptor() const
{
    return portFd_;
}

void Serial::clearBuffer()
{
    free(buffer_);
    buffer_ = 0;
    bufferSize_ = 0;
}

int Serial::flushReadBuffer()
{
    return tcflush(portFd_, TCIFLUSH);
}

//returns -1 if no data available
int Serial::extractByte()
{
    if (bufferSize_ == 0)
        return -1;

    unsigned char result = buffer_[0];
    memmove(buffer_, buffer_ + 1, bufferSize_ - 1);
    bufferSize_--;
    buffer_ = (char*)realloc(buffer_, bufferSize_);

    return result;
}

//returns -1 if no data available
int Serial::extractBytes(unsigned char* buf, size_t buflen)
{
    if ((unsigned int)bufferSize_ < buflen)
        return -1;

    memcpy(buf, buffer_, buflen);
    memmove(buffer_, buffer_ + buflen, bufferSize_ - buflen);
    bufferSize_ -= buflen;
    buffer_ = (char*)realloc(buffer_, bufferSize_);

    return buflen;
}

string* Serial::extractLine()
{
    char* p = buffer_;
    //printf("beforeWhile\n");
    while (p < buffer_ + bufferSize_) {
        if (*p == '\n')
            break;
        p++;
    }
    if (p == buffer_ + bufferSize_)
        return NULL;

    int lineLen = p - buffer_;
    char* lineCopy = (char*)malloc(lineLen + 1);
    memcpy(lineCopy, buffer_, lineLen);

    //this is rather ugly but at least it should work...
    lineCopy[lineLen] = '\0'; //overwrite \n with nulbyte
    if (lineCopy[lineLen - 1] == '\r')
        lineCopy[lineLen - 1] = '\0'; //also remove \r if present
    string* line = new string(lineCopy);
    free(lineCopy);

    //now resize the read buffer
    memmove(buffer_, p + 1, bufferSize_ - (lineLen + 1));
    bufferSize_ -= lineLen + 1;
    buffer_ = (char*)realloc(buffer_, bufferSize_);

    Logger::GetInstance()->logMessage("<---- Recv: " + *line, 3, 0);
    //std::cout << "\r\r <---- Recv: \""<< *line<<  "\"" << std::endl;

    return line;
}
