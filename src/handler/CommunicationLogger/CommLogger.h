/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */

#ifndef HANDLER_COMMLOGGER_H_
#define HANDLER_COMMLOGGER_H_

#include <iostream>
#include <unistd.h>
#include <string>
#include <dirent.h>
#include <sys/stat.h>

class CommLogger {
public:
    CommLogger();
    CommLogger(std::string printer);
    virtual ~CommLogger();

    bool logCommunicationMessage(std::string message, bool received);

private:
    std::string printerId;
    std::string filePath;
    int status;

    bool directoryExists(const char* directoryName);
    bool createDirectory(const char* directoryName);
    std::string getDateTime();
};

#endif /* HANDLER_COMMLOGGER_H_ */
