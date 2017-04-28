/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


#ifndef UTILS_LOGGER_H_
#define UTILS_LOGGER_H_

class Logger {
public:
    Logger();
    static Logger* GetInstance();
    void logMessage(std::string msg, int logLevel, int extraIndent);
    std::string getTimeNow();
    int getLogLevel();
    int getDataLevel();
    virtual ~Logger();

private:
    static Logger* instance; // singleton instance
};

#endif /* UTILS_LOGGER_H_ */
