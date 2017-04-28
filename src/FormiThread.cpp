/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


#include "FormiThread.hpp"
#include <node.h>
#include <nan.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <string>
#include <fstream>
#include <limits>
#include <cmath>
#include <math.h>
#include <mutex>
#include "utils/Logger/Logger.h"
#include "handler/PrinterData/PrinterData.h"
#include "handler/DeviceCenter/DeviceCenter.h"

std::mutex threadMtx;

using namespace std;
using namespace v8;
using namespace node;

std::ifstream& GotoLine(std::ifstream& file, unsigned int num)
{
    file.seekg(std::ios::beg);
    for (int i = 0; i < num; ++i) {
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return file;
}

std::string parseLine(std::string line)
{
    bool done = false;

    // First we remove the spaces before the actual gcode line
    while (!done) {
        if (line.find(" ") == 0) {
            line = line.substr(1, line.length() - 1);
        }
        else {
            done = true;
        }
    }

    // Then, we remove all \t characters
    done = false;
    while (!done) {
        std::size_t position = line.find('\t', 0);
        if (position != std::string::npos) {
            line.erase(position, 1);
        }
        else {
            done = true;
        }
    }

    return line;
}

// Read N lines from a file
std::string readNlines(std::ifstream& file, int startLine, int numberOfLines, int& count)
{
    GotoLine(file, startLine);

    string singleLine;
    string result = "";

    int counter = 0;
    for (int i = 0; i < numberOfLines; i++) {
        std::getline(file, singleLine);

        // Now we parse the line to remove \r and wrong spaces
        singleLine = parseLine(singleLine);

        result = result + singleLine + '\n';

        count++;
        counter++;

        if (file.eof()) {
            Logger::GetInstance()->logMessage("End of file", 5, 0);
            break;
        }
    }

    Logger::GetInstance()->logMessage("read " + std::to_string(counter) + " lines from file", 5, 0);
    return result;
}

// Append gcode to gcode buffer
int parseAndAppendGcode(std::string gcode, MarlinDriver* driver)
{
    size_t pos0 = 0;
    int extraLines = 0;
    size_t posT = (gcode.find("\n", pos0)) + 1;
    std::string gcodeChunk;
    bool temperatureOrCoordinate = true;

    int count = 0;
    Logger::GetInstance()->logMessage("Appending gcode.", 5, 0);

    // After each 30 GCODE lines, a M105/M114 (Temperature/coordinates) command is added as well
    while (posT != std::string::npos) {
        if (count == 30) {

            gcodeChunk = gcode.substr(pos0, posT - pos0);

            if (temperatureOrCoordinate) {
                temperatureOrCoordinate = false;
                gcodeChunk += "M105\n";
            }
            else {
                temperatureOrCoordinate = true;
                gcodeChunk += "M114\n";
            }

            extraLines++;
            pos0 = posT;
            count = 0;

            driver->appendGCode(gcodeChunk, true);
        }

        posT = (gcode.find("\n", posT));

        if (posT != std::string::npos) {
            posT += 1;
        }
        else {

            gcodeChunk = gcode.substr(pos0);
            driver->appendGCode(gcodeChunk, true);
        }

        count++;
    }
    Logger::GetInstance()->logMessage("extraLines: " + std::to_string(extraLines), 5, 0);
    return extraLines;
}

// Parse value
string getParameter(string code, string initialString, bool& gotIt)
{

    std::size_t positionVariable;
    std::size_t positionSpace;

    positionVariable = code.find(initialString);
    positionVariable += initialString.length();

    positionSpace = code.find(" ", positionVariable);
    std::string strName = "";

    if (positionSpace != std::string::npos) {
        strName = code.substr(positionVariable, positionSpace - positionVariable);
    }
    else {
        strName = code.substr(positionVariable);
    }

    if (strName.size() > 0) {
        gotIt = true;
    }
    else {
        gotIt = false;
    }
    return strName;
}

FormiThread::FormiThread() {}

FormiThread::~FormiThread() {}

// Calculate printing time from GCODE
void* FormiThread::calculatePrintingTime(void* threadarg)
{

    thread_data* data = static_cast<thread_data*>(threadarg);
    std::string printerID = data->serialDevice;
    std::string gcodeFile = data->gcodeFile;
    std::ifstream gcodeStream(gcodeFile);

    if (gcodeStream.fail()) {
        Logger::GetInstance()->logMessage("File does not exist", 1, 0);
        return NULL;
    }

    if (!gcodeStream.is_open()) {
        Logger::GetInstance()->logMessage("File can not open", 1, 0);
        return NULL;
    }

    // Reset printing time
    DeviceCenter::GetInstance()->updatePrintingTime(printerID, 0);

    string singleLine;

    float coordinateX = 0;
    float coordinateY = 0;

    float oldCoordinateX = 0;
    float oldCoordinateY = 0;
    float speed = 1000 /*milimeters per minute*/;

    speed = speed / 60; // milimeters per second

    float totalTime = 0; // seconds

    float currentExtruderUsed = 0;
    float currentT0Temp = 0;
    float currentT1Temp = 0;
    float currentBedTemp = 0;

    while (true)
    {

        try {

            if (gcodeStream.fail()) {
                Logger::GetInstance()->logMessage("File does not exist", 1, 0);
                break;
            }

            if (!gcodeStream.is_open()) {
                Logger::GetInstance()->logMessage("File can not open", 1, 0);
            }

            std::getline(gcodeStream, singleLine);

            bool hasMovement = ((singleLine.find("G1") != string::npos) || (singleLine.find("G0") != string::npos));
            bool hasX = (singleLine.find("X") != string::npos);
            bool hasY = (singleLine.find("Y") != string::npos);
            bool hasZ = (singleLine.find("Z") != string::npos);
            bool hasF = (singleLine.find("F") != string::npos);
            bool hasS = (singleLine.find("S") != string::npos);
            bool hasHome = (singleLine.find("G28") != string::npos);
            bool gotIt;

            bool hasBedTemperature = (singleLine.find("M190") != string::npos);
            bool hasExtrTemperature = (singleLine.find("M109") != string::npos);

            bool hasUpdateBedTemperature = (singleLine.find("M140") != string::npos);
            bool hasUpdateExtrTemperature = (singleLine.find("M104") != string::npos);

            bool hasM116 = (singleLine.find("M116") != string::npos);
            bool hasM303 = (singleLine.find("M303") != string::npos);

            bool changeToE0 = (singleLine.find("T0") == 0);
            bool changeToE1 = (singleLine.find("T1") == 0);

            bool includesE0 = (singleLine.find("T0") != string::npos);
            bool includesE1 = (singleLine.find("T1") != string::npos);

            if (changeToE0) {
                currentExtruderUsed = 0;
            }

            if (changeToE1) {
                currentExtruderUsed = 1;
            }

            if (hasUpdateBedTemperature) {
                string newTemperature = getParameter(singleLine, "S", gotIt);

                if (gotIt) {
                    float numberTemperature = std::stof(newTemperature);
                    currentBedTemp = numberTemperature;
                }
            }

            if (hasUpdateExtrTemperature) {
                string newTemperature = getParameter(singleLine, "S", gotIt);

                if (gotIt) {
                    float numberTemperature = std::stof(newTemperature);

                    if (currentExtruderUsed == 0) {
                        currentT0Temp = numberTemperature;
                    }
                    else if (currentExtruderUsed == 1) {
                        currentT1Temp = numberTemperature;
                    }
                }
            }

            if (hasBedTemperature) {
                string newTemperature = getParameter(singleLine, "S", gotIt);

                if (gotIt) {
                    float numberTemperature = std::stof(newTemperature);
                    float newTime = (currentBedTemp - numberTemperature) * 5;
                    currentBedTemp = numberTemperature;

                    totalTime += std::abs(newTime);
                }
            }

            if (hasExtrTemperature) {
                if (hasS) {
                    string newTemperature = getParameter(singleLine, "S", gotIt);
                    if (gotIt) {
                        float numberTemperature = std::stof(newTemperature);
                        float newTime = 0;

                        if (includesE0) {
                            newTime = currentT0Temp - numberTemperature;
                            currentT0Temp = numberTemperature;
                        }
                        else if (includesE1) {
                            newTime = currentT1Temp - numberTemperature;
                            currentT1Temp = numberTemperature;
                        }
                        else {
                            if (currentExtruderUsed == 0) {
                                newTime = currentT0Temp - numberTemperature;
                                currentT0Temp = numberTemperature;
                            }
                            else if (currentExtruderUsed == 1) {
                                newTime = currentT1Temp - numberTemperature;
                                currentT1Temp = numberTemperature;
                            }
                        }

                        totalTime += std::abs(newTime);
                    }
                }
            }
            if (hasM116) {
                totalTime += 20 * 60;
            }
            if (hasM303) {
                totalTime += 30 * 60;
            }
            if (hasMovement) {

                if (hasF) {
                    string newSpeed = getParameter(singleLine, "F", gotIt);

                    if (gotIt) {
                        speed = std::stof(newSpeed) / 60.0f;
                    }
                }

                if (hasX && hasY) {

                    string newX = getParameter(singleLine, "X", gotIt);
                    bool flag = false;

                    if (gotIt) {
                        oldCoordinateX = coordinateX;
                        coordinateX = std::stof(newX);
                    }
                    else {
                        flag = true;
                    }

                    string newY = getParameter(singleLine, "Y", gotIt);

                    if (gotIt) {
                        oldCoordinateY = coordinateY;
                        coordinateY = std::stof(newY);
                    }
                    else {
                        flag = true;
                    }

                    if (!flag) {
                        float distanceX = std::abs(coordinateX - oldCoordinateX);
                        float distanceY = std::abs(coordinateY - oldCoordinateY);

                        float distance = sqrt((distanceX * distanceX) + (distanceY * distanceY));
                        float newTime = distance / speed;

                        totalTime += newTime;
                    }
                } // hasX && hasY

                if (hasX && !hasY) {

                    string newX = getParameter(singleLine, "X", gotIt);
                    bool flag = false;

                    if (gotIt) {
                        oldCoordinateX = coordinateX;
                        coordinateX = std::stof(newX);

                        float distanceX = std::abs(coordinateX - oldCoordinateX);
                        float newTime = distanceX / speed;

                        totalTime += newTime;
                    }
                } // only X

                if (hasY && !hasX) {

                    string newY = getParameter(singleLine, "Y", gotIt);
                    bool flag = false;

                    if (gotIt) {
                        oldCoordinateY = coordinateY;
                        coordinateY = std::stof(newY);

                        float distanceY = std::abs(coordinateY - oldCoordinateY);
                        float newTime = distanceY / speed; //

                        totalTime += newTime;
                    }
                } // only Y

            } // if has movement

            if (hasHome) {
                if (hasZ) {
                    totalTime += 90;
                }
                else {
                    if (hasX || hasY)
                        totalTime += 20;
                    else
                        totalTime += 90;
                }
            }

            if (gcodeStream.eof())
                break;
        } // TRY

        catch (...) {
            Logger::GetInstance()->logMessage("Next iteration", 1, 0);
        }
    } // while

    Logger::GetInstance()->logMessage("Updating total printing time: " + std::to_string(totalTime) + " seconds", 1, 0);
    DeviceCenter::GetInstance()->updatePrintingTime(printerID, totalTime);
}

// Count gcode lines in GCODE file
void* FormiThread::countLines(void* threadarg)
{
    thread_data* data = static_cast<thread_data*>(threadarg);

    MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(data->serialDevice);

    std::string printerID = data->serialDevice;
    std::string gcodeFile = data->gcodeFile;
    std::ifstream gcodeStream(gcodeFile);

    // This function should start right after drivers start buffering lines
    // So it sleeps for two seconds
    sleep(2);

    while (DeviceCenter::GetInstance()->isBufferAvailable(printerID) <= 0)
    {
        usleep(100000);

        if (driver->state == 3) {
            Logger::GetInstance()->logMessage("Warning: Printer is not printing, gcode line counter stopped", 1, 0);
            return NULL;
        }
    }

    threadMtx.lock();

    if (gcodeStream.fail()) {
        Logger::GetInstance()->logMessage("File does not exist: " + gcodeFile, 1, 0);
        return NULL;
    }

    if (!gcodeStream.is_open()) {
        Logger::GetInstance()->logMessage("File can not open: " + gcodeFile, 1, 0);
        return NULL;
    }

    int numberOfLines = std::count(std::istreambuf_iterator<char>(gcodeStream),
        std::istreambuf_iterator<char>(), '\n');

    threadMtx.unlock();

    Logger::GetInstance()->logMessage("Number of lines before: " + std::to_string(numberOfLines), 4, 0);

    if (numberOfLines > 1000) {

        int temperatureLines = numberOfLines / 30;

        Logger::GetInstance()->logMessage("Adding " + std::to_string(temperatureLines) + " lines", 4, 0);
        numberOfLines += temperatureLines;
    }

    else {
        int temperatureLines = ((numberOfLines)*4 / 100);

        numberOfLines += temperatureLines;
    }

    Logger::GetInstance()->logMessage("Total lines: " + std::to_string(numberOfLines), 4, 0);
    DeviceCenter::GetInstance()->updateTotalLines(printerID, numberOfLines);
}

// DISABLED FEATURE. Ðebug purposes
void* FormiThread::insertLinesManually(void* threadarg)
{

    thread_data* data = static_cast<thread_data*>(threadarg);
    MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(data->serialDevice);

    std::string printerID = data->serialDevice;
    std::string nothingCallback;

    while (1) {
        std::cout << "Please insert a line: " << std::endl;
        string input;
        getline(cin, input);

        std::cout << "Your line is: " << input << std::endl
                  << std::endl
                  << std::endl
                  << std::endl;

        driver->pushRawCommand(input, nothingCallback, false);
    }
}

// Read GCODE file and buffer it
void* FormiThread::appendGcode(void* threadarg)
{

    thread_data* data = static_cast<thread_data*>(threadarg);

    MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(data->serialDevice);

    std::string printerID = data->serialDevice;
    std::string gcodeFile = data->gcodeFile;
    std::ifstream gcodeStream(gcodeFile);

    if (gcodeStream.fail()) {
        Logger::GetInstance()->logMessage("File does not exist: " + gcodeFile, 1, 0);
        return NULL;
    }

    if (!gcodeStream.is_open()) {
        Logger::GetInstance()->logMessage("File can not open: " + gcodeFile, 1, 0);
        return NULL;
    }

    int currentLine = 0;
    int extraLines = 0;
    int bufferedLines = 0;
    int bufferSize = 100;
    std::string gcodePart;

    while (true) {
        int status = DeviceCenter::GetInstance()->isBufferAvailable(printerID);

        if (status > 0) {
            threadMtx.lock();
            gcodePart = "";
            int count = 0;
            gcodePart = readNlines(gcodeStream, currentLine, bufferSize, count);
            currentLine += bufferSize;

            extraLines = parseAndAppendGcode(gcodePart, driver);
            bufferedLines = count + extraLines;

            DeviceCenter::GetInstance()->addBufferedLines(printerID, bufferedLines);
            threadMtx.unlock();
        }

        else {
            if (status == 0) {
                // Do nothing
            }

            if (status < 0) {
                DeviceCenter::GetInstance()->updateTotalLines(printerID, 0);
                break;
            }
        }

        usleep(50000);

        if (gcodeStream.eof())
            break;
    }
    Logger::GetInstance()->logMessage("Stop buffering file", 1, 0);
    gcodeStream.close();
}
