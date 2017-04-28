/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


#include <iostream>
#include <cstdlib>
#include <time.h>
#include <fstream>

#include "Logger.h"

using namespace std;

Logger* Logger::instance = nullptr;

Logger::Logger()
{
}

Logger::~Logger()
{
}

Logger*
Logger::GetInstance()
{
    if (instance == nullptr) {
        instance = new Logger();
    }
    return instance;
}

/******************************************
 *  Log message with given log level
 *  (will not be logged if loglevel > environment loglevel)
 *  Loglevel is 1 by default
 *  It is also possible to specify extra indentation (or less when negative)
 *  Indentation is logLevel - 1 spaces by default
 *****************************************/
void Logger::logMessage(std::string msg, int logLevel, int extraIndent)
{
    int driversLogLevel = getLogLevel();

    if (logLevel <= driversLogLevel) {
        int indentLevel = max(0, logLevel - 1 + extraIndent);
        std::string indent(indentLevel, ' ');
        cout << indent << msg << endl;
    }

}

std::string
Logger::getTimeNow()
{

    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

/******************************************
* Get log level from drivers_log_level environment variable
* Return zero if not set (or not a number)
*****************************************/
int Logger::getLogLevel()
{

    if (std::getenv("DRIVERS_LOG_LEVEL") != nullptr) {
        try {
            int lvl = atoi(std::getenv("DRIVERS_LOG_LEVEL"));
            return lvl;
        }
        catch (...) {
        }
    }

    if (std::getenv("drivers_log_level") != nullptr) {
        try {
            int lvl = atoi(std::getenv("drivers_log_level"));
            return lvl;
        }
        catch (...) {
            return 1;
        }
    }
    return 1;
}
