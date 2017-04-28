/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


#include "CommLogger.h"
#include <mutex>
#include <string>
#include <fstream>
#include <ctime>
#include <iostream>
#include "../../utils/Logger/Logger.h"

using namespace std;

/******************************************
 *  Check if given directory exists
 *****************************************/
bool CommLogger::directoryExists(const char* path)
{
    if (path == NULL)
        return false;

    DIR* pDir;
    bool bExists = false;

    pDir = opendir(path);
    if (pDir != NULL) {
        bExists = true;
        (void)closedir(pDir);
    }

    return bExists;
}

/******************************************
 *  Create directory with the given name
 *****************************************/
bool CommLogger::createDirectory(const char* directoryName)
{

    mkdir(directoryName, 0755);
    return true;
}

string
CommLogger::getDateTime()
{
    std::time_t t = std::time(NULL);
    char mbstr[100];
    if (std::strftime(mbstr, 100, "%d/%m %T ", std::localtime(&t))) {
        string result(mbstr);
        return result;
    }
    else {
        string result = "invalid_time: ";
        return result;
    }
}

CommLogger::CommLogger()
{
    printerId = "";
    filePath = "";
    status = -1;
}

CommLogger::CommLogger(std::string printer)
{
    printerId = printer;

    // Check if directory exists
    const char* homeD = getenv("HOME");
    std::string homeDir(homeD);
    if (directoryExists((homeDir + "/formide/communication").c_str())) {
        Logger::GetInstance()->logMessage("CommLogger: communication folder exists", 3, 0);
    }

    // If not, it is created
    else {
        Logger::GetInstance()->logMessage("CommLogger: communication folder does not exist. Creating...", 3, 0);

        int i = system(("mkdir -p " + homeDir + "/formide/communication").c_str());
        Logger::GetInstance()->logMessage("mkdir -p " + homeDir + "/formide/communication/ \tReturn value: " + std::to_string(i), 3, 0);

        if (i < 0) {
            Logger::GetInstance()->logMessage("CommLogger: Error while creating communication folder", 1, 0);
            status = -1;
        }
    }

    // Create/Open communication file
    filePath = homeDir + "/formide/communication/" + printerId.substr(5)+".log";
    Logger::GetInstance()->logMessage("CommLogger: using file " + filePath, 3, 0);

    std::ofstream commFile;
    commFile.open(filePath, std::ios::app);
    if (commFile.is_open()) {
        commFile << getDateTime() << "New driver for printer " << printerId << std::endl;
        commFile.close();
        status = 0;
    }
    else {
        Logger::GetInstance()->logMessage("Error opening file: " + filePath, 1, 0);
        status = -2;
    }
}

CommLogger::~CommLogger()
{
}


// Save communication message in file
bool CommLogger::logCommunicationMessage(string line, bool received)
{

    if (status < 0)
        return false;

    std::ofstream commFile;

    commFile.open(filePath, std::ios::app);
    if (commFile.is_open()) {
        if (received)
            commFile << getDateTime() << "R: " << line << std::endl; // received message
        else
            commFile << getDateTime() << "S: " << line; // sent message

        commFile.close();
        return true;
    }
    else {
        Logger::GetInstance()->logMessage("Error opening file: " + filePath, 3, 0);
        return false;
    }
}
