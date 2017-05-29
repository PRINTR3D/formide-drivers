/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */

#include <nan.h>
#include <math.h>
#include <cmath>
#include <vector>
#include <ctime>
#include <unistd.h>
#include <string>
#include <iostream>
#include <sstream>
#include <mutex>
#include <fstream>
#include <stdlib.h>
#include "DeviceCenter.h"
#include "../../utils/Logger/Logger.h"
#include "../CommunicationLogger/CommLogger.h"
#include "../../driver/MarlinDriver/MarlinDriver.h"

std::mutex mtxBuffer;
std::mutex totalLinesMtx;

using namespace v8;
using namespace std;

const std::string currentDateTime()
{
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

/******************************************
 *  Check if given directory exists
 *****************************************/
bool directoryExists(const char* path)
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

DeviceCenter* DeviceCenter::instance = nullptr;
DeviceCenter::DeviceCenter()
{
    milisecUpdate = 100000;
}

DeviceCenter*
DeviceCenter::GetInstance()
{
    if (instance == nullptr) {
        instance = new DeviceCenter();
    }
    return instance;
}

DeviceCenter::~DeviceCenter()
{

}

// Saves callback function so events can be triggered later
void DeviceCenter::setCallback(Nan::Callback* call)
{
    callback = call;
}
Nan::Callback*
DeviceCenter::getCallback()
{
    return callback;
}

// Sends printFinished event
void DeviceCenter::printFinished(std::string port)
{
	// V8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {
        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Retrieve printJob
    int a = 0;
    int printjob = -1;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (port == printerList[a])
            if (connectedList[a]) {
                printjob = PrinterDataList[a].printjobID;
            }

        a++;
    }

    // Callback argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);

    // Result Object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printFinished"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));
    if (printjob >= 0)
            printerOBJ->Set(String::NewFromUtf8(isolate, "printjobID"), Integer::New(isolate, printjob));


    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);
}

// Sends event printerError with extruder_jam
void DeviceCenter::extruderJam(std::string port)
{
	// V8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {

        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Callback argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);


    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerError"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "key"), String::NewFromUtf8(isolate, "extruder_jam"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "message"), String::NewFromUtf8(isolate, "Extruder is jammed"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));


    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);

    Logger::GetInstance()->logMessage("Extruder is jammed", 1, 0);
}

// Sends error event
void DeviceCenter::printerErrorEvent(std::string port, std::string key, std::string message)
{
	// V8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {

        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Callback argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);

    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerError"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "key"), String::NewFromUtf8(isolate, key.c_str()));
    printerOBJ->Set(String::NewFromUtf8(isolate, "message"), String::NewFromUtf8(isolate, message.c_str()));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));


    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);

    Logger::GetInstance()->logMessage("Sending printerError: " + key, 1, 0);
}

// Sends Warning event
void DeviceCenter::printerWarningEvent(std::string port, std::string key, std::string message)
{

	// V8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {

        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Callback argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);

    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerWarning"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "key"), String::NewFromUtf8(isolate, key.c_str()));
    printerOBJ->Set(String::NewFromUtf8(isolate, "message"), String::NewFromUtf8(isolate, message.c_str()));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));

    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);

    Logger::GetInstance()->logMessage("Sending printerWarning: " + key, 1, 0);
}

// Sends Info Event (string message)
void DeviceCenter::printerInfoEvent(std::string port, std::string key, std::string message)
{

	// v8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {

        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Return argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);

    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerInfo"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "key"), String::NewFromUtf8(isolate, key.c_str()));
    printerOBJ->Set(String::NewFromUtf8(isolate, "message"), String::NewFromUtf8(isolate, message.c_str()));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));


    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);

    Logger::GetInstance()->logMessage("Sending printerInfo: " + key, 1, 0);
}

// Sends Info Event (int value)
void DeviceCenter::printerInfoEvent(std::string port, std::string key, int message)
{
	// v8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {

        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Return argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);

    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerInfo"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "key"), String::NewFromUtf8(isolate, key.c_str()));
    printerOBJ->Set(String::NewFromUtf8(isolate, "message"), Integer::New(isolate, message));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));

    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);

    Logger::GetInstance()->logMessage("Sending printerInfo: " + key, 1, 0);
}

// Sends Printer Online Event
void DeviceCenter::printerOnline(std::string port, int baudrate)
{

	// V8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {

        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Callback argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);

    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerOnline"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));


    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);

    writeBaudrate(port, baudrate);
}

// Sends printerConnected event
void DeviceCenter::printerConnected(std::string port)
{

	// V8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {
        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Callback argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);


    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerConnected"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));

    // Callback
    Logger::GetInstance()->logMessage("Sending event: printer connected", 1, 0);
    argv[2] = printerOBJ;
    callback->Call(3, argv);
}

// Sends printerDisconnected event
void DeviceCenter::printerDisconnected(std::string port)
{

	// V8 Variables
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if (!isolate) {

        // isolate = v8::Isolate::New();
        // isolate->Enter();
    }

    // Callback argument
    v8::Local<v8::Value> argv[3];
    argv[0] = v8::Undefined(isolate);
    argv[1] = v8::Undefined(isolate);
    argv[2] = v8::Undefined(isolate);


    // Result object
    Local<Object> printerOBJ = Object::New(isolate);
    printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerDisconnected"));
    printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, port.c_str()));

    // Callback
    argv[2] = printerOBJ;
    callback->Call(3, argv);
}

// Baud rate is read from file system, so connection can be done faster
// Without having to try each baud rate separatedly
int DeviceCenter::readBaudrate(std::string printerID)
{
    int baudrate = 57600;
    const char* homeD = getenv("HOME");

    std::string homeDir(homeD);

    // if directory exists
    if (directoryExists((homeDir + "/formide/baudrate").c_str())) {
        string filePath = homeDir + "/formide/baudrate/" + printerID.substr(5);
        ifstream myfile(filePath.c_str());
        string baudrateString;

        // if file exists
        if (myfile.is_open()) {
            getline(myfile, baudrateString);
            Logger::GetInstance()->logMessage("Baud rate found: " + baudrateString, 2, 0);
            try {
                baudrate = std::stoi(baudrateString);

                if (baudrate == 57600)
                    baudrate = 250000;
                else if (baudrate == 115200)
                    baudrate = 57600;
                else if (baudrate == 250000)
                    baudrate = 115200;
                else
                    baudrate = 57600;
            }
            catch (...) {
            }
            myfile.close();
        }
        else {
        }
    }

    return baudrate;
}

// Save used baudrate in file, to connect faster next iterations
void DeviceCenter::writeBaudrate(std::string printerID, int baudrate)
{

    // if directory exists
    const char* homeD = getenv("HOME");

    std::string homeDir(homeD);
    if (directoryExists((homeDir + "/formide/baudrate").c_str())) {
    }

    // if not, we create it.
    else {
        int i = system(("mkdir -p " + homeDir + "/formide/baudrate").c_str());
        Logger::GetInstance()->logMessage("mkdir -p " + homeDir + "/formide/baudrate\nReturn value: " + std::to_string(i), 2, 0);
    }

    string filePath = homeDir + "/formide/baudrate/" + printerID.substr(5);

    Logger::GetInstance()->logMessage("Opening file: " + filePath, 2, 0);
    ofstream myfile(filePath.c_str());

    // write baudrate to file
    if (myfile.is_open()) {
        myfile << baudrate << endl;
        myfile.close();
    }
    else {
        Logger::GetInstance()->logMessage("Error baud rate file", 1, 0);
    }

    return;
}

// Create a new Marlin Driver for printer
void DeviceCenter::newDriverForPrinter(std::string printerID, bool sendEvent)
{

	// Create new instance of PrinterData (where printer status is stored)
    PrinterData newPrinter;

    std::string printerSerial = printerID; // port
    int baudrate = readBaudrate(printerID); // baud rate

    Logger::GetInstance()->logMessage("new driver for printer: " + printerSerial, 2, 0);

    newPrinter.driver = new MarlinDriver(printerSerial, baudrate); // Marlin Driver
    newPrinter.serialPort = printerSerial;

    // Push printer to vector lists
    PrinterDataList.push_back(newPrinter);
    printerList.push_back(printerSerial);
    connectedList.push_back(true);

    // Send printerConnected event
    if (sendEvent) {
        printerConnected(printerID);
    }

    // Perform main iteration
    PrinterDataList[PrinterDataList.size() - 1].driver->mainIteration();

}

// Returns pointer to driver
MarlinDriver*
DeviceCenter::getDriverFromPrinter(std::string printerID)
{

    int a = 0;
    bool success = false;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                success = true;
                return PrinterDataList[a].driver;
            }
            else
                return NULL;
        a++;
    }
    if (!success)
        return NULL;
}

// Returns all printer data
std::vector<PrinterData>
DeviceCenter::getPrinterDataList()
{
    return PrinterDataList;
}

// Returns printer list
std::vector<std::string>
DeviceCenter::getPrinterList()
{
    std::vector<std::string> result;
    result.clear();

    int a = 0;
    for (std::vector<PrinterData>::iterator it = PrinterDataList.begin(); it != PrinterDataList.end(); it++) {
        result.push_back(PrinterDataList[a].serialPort);
        a++;
    }

    return result;
}

// Returns printer data for specific printer
PrinterData*
DeviceCenter::getPrinterInfo(std::string printerID, bool* gotit)
{
    int a = 0;
    bool gotIt = false;
    PrinterData* printer = nullptr;
    for (std::vector<PrinterData>::iterator it = PrinterDataList.begin(); it != PrinterDataList.end(); it++) {

        if (PrinterDataList[a].serialPort == printerID) {
            gotIt = true;

            if (PrinterDataList[a].extruderTargetTemperature.size() == 0) {
                int b = 0;
                for (std::vector<int>::iterator it = PrinterDataList[a].extruderTemperature.begin(); it != PrinterDataList[a].extruderTemperature.end(); it++) {
                    PrinterDataList[a].extruderTargetTemperature.push_back(0);
                    b++;
                }
            }

            printer = &PrinterDataList[a];
        }
        a++;
    }

    if (gotIt) {
        *gotit = true;
        return printer;
    }
    else
        return NULL;
}

// Connected list
std::vector<bool>
DeviceCenter::getConnected()
{
    return connectedList;
}

// Updated connected list and runs main iteration from each driver
void DeviceCenter::updateConnected(std::vector<bool> booleans, bool empty)
{

    int a = 0;
    for (std::vector<bool>::iterator it = booleans.begin(); it != booleans.end(); it++) {
        connectedList[a] = booleans[a];

        if (!empty) {
            if (connectedList[a]) {
                PrinterDataList[a].driver->mainIteration();
            }
        }

        a++;
    }
}

/**********************************
 * UPDATE PRINTER STATUS FUNCTIONS
 *********************************/

void DeviceCenter::updatePrintJobID(std::string printerID, int printjobid)
{
    int a = 0;
    int id;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].printjobID = printjobid;

                PrinterDataList[a].timeStarted = currentDateTime();

                break;
            }

        a++;
    }
}

void DeviceCenter::resetBuffer(std::string printerID)
{
    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a]) {

            Logger::GetInstance()->logMessage("RESET BUFFER", 2, 0);
            mtxBuffer.lock();
            PrinterDataList[a].bufferedLines = 0;
            mtxBuffer.unlock();
        }

        a++;
    }
}

void DeviceCenter::updateState(std::string printerID, std::string state)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a]) {
            if ((PrinterDataList[a].state == "online" && state == "printing") || (PrinterDataList[a].state == "printing" && state == "online")) {
                Logger::GetInstance()->logMessage("RESET BUFFER", 2, 0);
                mtxBuffer.lock();
                PrinterDataList[a].bufferedLines = 0;
                mtxBuffer.unlock();
            }

            PrinterDataList[a].state = state;

            if (state == "disconnected")
                PrinterDataList[a].driver->closeConnection();

            if (state == "online") {
                PrinterDataList[a].totalLines = 0;
                PrinterDataList[a].printingTime = 0;
                PrinterDataList[a].remainingPrintingTime = 0;
            }
        }

        a++;
    }
}
void DeviceCenter::updateTimeNow(std::string printerID, std::string time)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            PrinterDataList[a].timeNow = currentDateTime();

        a++;
    }
}

void DeviceCenter::updateProgress(std::string printerID, int progress)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].progress = progress;

        a++;
    }
}

void DeviceCenter::updateCurrentLine(std::string printerID, int currentLine)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].currentLine = currentLine;

                totalLinesMtx.lock();
                if (PrinterDataList[a].totalLines > 0) {
                    float progress = currentLine * 100 / PrinterDataList[a].totalLines;
                    float progressLeft = std::abs(100 - progress);

                    if (PrinterDataList[a].printingTime > 0) {
                        float totalSec = PrinterDataList[a].printingTime * progressLeft / 100;

                        if (PrinterDataList[a].speedMultiplier > 0) {
                            totalSec = totalSec * 100 / PrinterDataList[a].speedMultiplier;
                            PrinterDataList[a].remainingPrintingTime = totalSec;
                        }
                    }
                }
                totalLinesMtx.unlock();
            }
        a++;
    }
}

int DeviceCenter::GetTotalLines(std::string printerID)
{
    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                totalLinesMtx.lock();
                int returnValue = PrinterDataList[a].totalLines;
                totalLinesMtx.unlock();
                return returnValue;
            }

        a++;
    }
    return -1;
}

void DeviceCenter::updatePrintingTime(std::string printerID, float totalSeconds)
{

    int a = 0;

    Logger::GetInstance()->logMessage("Updating total seconds: " + std::to_string(totalSeconds), 2, 0);
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].printingTime = totalSeconds;

        a++;
    }
}

void DeviceCenter::updateTotalLines(std::string printerID, int totalLines)
{

    int a = 0;

    Logger::GetInstance()->logMessage("Updating total lines: " + std::to_string(totalLines), 2, 0);

    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                totalLinesMtx.lock();
                PrinterDataList[a].totalLines = totalLines;
                totalLinesMtx.unlock();
            }

        a++;
    }
}

void DeviceCenter::updateRatio(std::string printerID, int ratio)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].ratio = ratio;

        a++;
    }
}

void DeviceCenter::updateFanSpeed(std::string printerID, double fanSpeed)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].fanSpeed = fanSpeed * 100 / 255;
            }

        a++;
    }
}

void DeviceCenter::updateFlowRate(std::string printerID, int flowRate)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].flowRate = flowRate;

        a++;
    }
}

void DeviceCenter::updateSpeedMultiplier(std::string printerID, int speedMultiplier)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].speedMultiplier = speedMultiplier;

        a++;
    }
}

void DeviceCenter::updateFirmwareName(std::string printerID, std::string firmwareName)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].firmwareName = firmwareName;

        a++;
    }
}

void DeviceCenter::updateFirmwareFilament(std::string printerID, std::string firmwareFilament)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].firmwareFilament = firmwareFilament;

        a++;
    }
}

void DeviceCenter::updateFirmwareTime(std::string printerID, std::string firmwareTime)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].firmwareTime = firmwareTime;

        a++;
    }
}

void DeviceCenter::homeX(std::string printerID)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].coordinateX = 0;
            }

        a++;
    }
}

void DeviceCenter::homeY(std::string printerID)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].coordinateY = 0;
            }

        a++;
    }
}

void DeviceCenter::homeZ(std::string printerID)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].coordinateZ = 0;
            }

        a++;
    }
}

void DeviceCenter::updateCoordinateX(std::string printerID, double coordinate)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].coordinateX = coordinate;

        a++;
    }
}

void DeviceCenter::updateCoordinateY(std::string printerID, double coordinate)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].coordinateY = coordinate;

        a++;
    }
}

void DeviceCenter::updateCoordinateZ(std::string printerID, double coordinate)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].coordinateZ = coordinate;
            }

        a++;
    }
}

void DeviceCenter::updateCoordinateE(std::string printerID, double coordinate)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].coordinateE = coordinate;

        a++;
    }
}

void DeviceCenter::updateSpeed(std::string printerID, int speed)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].speed = speed;

        a++;
    }
}

void DeviceCenter::updateCurrentlyPrinting(std::string printerID, std::string currently)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].currentlyPrinting = currently;

        a++;
    }
}

void DeviceCenter::updateCurrentLayer(string printerID, int layer)
{
    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].currentLayer = layer;
        a++;
    }
}


void DeviceCenter::updateUsedExtruder(string printerID, int usedExtruder)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                PrinterDataList[a].usedExtruder = usedExtruder;

        a++;
    }
}

void DeviceCenter::updateExtrudersTemp(std::string printerID, std::vector<int> extrudersTemp)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                int b = 0;
                PrinterDataList[a].extruderTemperature.clear();
                for (std::vector<int>::iterator it = extrudersTemp.begin(); it != extrudersTemp.end(); it++) {
                    PrinterDataList[a].extruderTemperature.push_back(extrudersTemp[b]);
                    b++;
                }
            }

        a++;
    }
}

void DeviceCenter::updateExtrudersTargetTemp(std::string printerID, std::vector<int> extrudersTargetTemp)
{

    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                int b = 0;
                PrinterDataList[a].extruderTargetTemperature.clear();
                for (std::vector<int>::iterator it = extrudersTargetTemp.begin(); it != extrudersTargetTemp.end(); it++) {
                    Logger::GetInstance()->logMessage("Updating EXT" + std::to_string(b) + " target temp to " + std::to_string(extrudersTargetTemp[b]), 6, 0);
                    PrinterDataList[a].extruderTargetTemperature.push_back(extrudersTargetTemp[b]);
                    b++;
                }
            }

        a++;
    }
}
void DeviceCenter::updateBedTemperature(std::string printerID, std::vector<int> bedTemp)
{
    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a]) {
                PrinterDataList[a].bedTemperature = bedTemp[0];
                if (bedTemp.size() > 1) {
                    PrinterDataList[a].targetBedTemperature = bedTemp[1];
                }
            }

        a++;
    }
}

void DeviceCenter::reconnectPrinter(int id)
{

    int baudrate = readBaudrate(printerList[id]);
    totalLinesMtx.lock();
    PrinterDataList[id].totalLines = 0;
    totalLinesMtx.unlock();

    PrinterDataList[id].driver = new MarlinDriver(PrinterDataList[id].serialPort, baudrate);
}

bool DeviceCenter::isConnected(std::string printerID)
{

    bool connected = false;
    int a = 0;
    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a])
            if (connectedList[a])
                connected = true;

        a++;
    }
    return connected;
}

void DeviceCenter::addBufferedLines(std::string printerID, int numberOfLines)
{

    int a = 0;

    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a]) {
            if (connectedList[a]) {
                mtxBuffer.lock();
                Logger::GetInstance()->logMessage("Lines buffered: " + std::to_string(PrinterDataList[a].bufferedLines) + "(+" + std::to_string(numberOfLines) + ") ", 5, 0);
                PrinterDataList[a].bufferedLines += numberOfLines;
                Logger::GetInstance()->logMessage("= " + std::to_string(PrinterDataList[a].bufferedLines), 5, 0);
                mtxBuffer.unlock();
            }
        }
        a++;
    }
}

int DeviceCenter::getBufferedLines(std::string printerID)
{

    int a = 0;
    int buff = -1;

    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a]) {
            if (connectedList[a] && (PrinterDataList[a].state == "printing" || PrinterDataList[a].state == "paused" || PrinterDataList[a].state == "heating")) {

                mtxBuffer.lock();
                buff = PrinterDataList[a].bufferedLines;
                mtxBuffer.unlock();
                return buff;
            }
            else {

                return -1;
            }
        }
        a++;
    }

    return -1;
}

// Returns 1 if buffer is available to be inserted more gcode lines
// Returns 0 when buffer has enough lines to keep printing
// Returns -1 when printer is stopped and other thread should stop reading file
int DeviceCenter::isBufferAvailable(std::string printerID)
{

    int a = 0;

    for (std::vector<std::string>::iterator it = printerList.begin(); it != printerList.end(); it++) {
        if (printerID == printerList[a]) {

            if (connectedList[a] && (PrinterDataList[a].state == "printing" || PrinterDataList[a].state == "paused" || PrinterDataList[a].state == "heating") && PrinterDataList[a].driver->state != 6) {
                mtxBuffer.lock();

                if (PrinterDataList[a].bufferedLines < 4000) {
                    mtxBuffer.unlock();
                    return 1;
                }
                else {
                    mtxBuffer.unlock();
                    return 0;
                }
            }
            else {
                Logger::GetInstance()->logMessage("Stop buffering", 1, 0);
                return -1;
            }
        }
        a++;
    }
    return -1;
}

std::string
DeviceCenter::getLastLines(std::string const& filename, int lineCount)
{

    try {
        size_t const granularity = 100 * lineCount;
        std::ifstream source(filename.c_str(), std::ios_base::binary);
        source.seekg(0, std::ios_base::end);
        size_t size = static_cast<size_t>(source.tellg());
        std::vector<char> buffer;
        int newlineCount = 0;
        while (source
            && buffer.size() != size
            && newlineCount < lineCount) {
            buffer.resize(std::min(buffer.size() + granularity, size));
            source.seekg(-static_cast<std::streamoff>(buffer.size()),
                std::ios_base::end);
            source.read(buffer.data(), buffer.size());
            newlineCount = std::count(buffer.begin(), buffer.end(), '\n');
        }
        std::vector<char>::iterator start = buffer.begin();
        while (newlineCount > lineCount) {
            start = std::find(start, buffer.end(), '\n') + 1;
            --newlineCount;
        }
        std::vector<char>::iterator end = remove(start, buffer.end(), '\r');
        return std::string(start, end);
    }
    catch (...) {
        return "";
    }
}

string
DeviceCenter::skipLines(std::string lastLines, int lineCount)
{

    string result = lastLines;
    try {

        int i = 0;
        std::size_t positionNewLine = 0;
        positionNewLine = result.find("\n", positionNewLine);
        while (i < lineCount - 1) {
            positionNewLine = result.find("\n", positionNewLine + 1);
            i++;
        }

        result = result.substr(0, positionNewLine);

        return result;
    }
    catch (...) {
        return lastLines;
    }
}

// Read communication logs from file system
int DeviceCenter::readCommunicationLog(string printerID, int lineCount, int skipCount, string& res)
{

    // Check if directory exists
    const char* homeD = getenv("HOME");
    std::string homeDir(homeD);

    if (directoryExists((homeDir + "/formide/communication").c_str())) {

        string filePath = homeDir + "/formide/communication/" + printerID.substr(5)+".log";
        ifstream myfile(filePath.c_str());
        string baudrateString;

        // Check if file exists
        if (!std::ifstream(filePath)) {
            // Return error: File does not exist
            Logger::GetInstance()->logMessage("Error(-1) while reading communication logs", 3, 0);
            return -1;
        }

        // if file exists
        if (myfile.is_open()) {
            try {

                string result = getLastLines(filePath, lineCount + skipCount);
                result = skipLines(result, lineCount);

                if (result.size() > 0) {
                    res = result;
                    return lineCount; // Return number of lines read
                }
                else {
                    Logger::GetInstance()->logMessage("Error(-4) while reading communication logs", 3, 0);
                    return -4;
                }
            }
            catch (...) {
                Logger::GetInstance()->logMessage("Error(-3) while reading communication logs", 3, 0);
                myfile.close();
                return -3;
            }
            myfile.close();
        }
        else {
            // Return error: Could not open file
            Logger::GetInstance()->logMessage("Error(-2) while reading communication logs", 3, 0);
            return -2;
        }
    }

    else
    {
    	// Return error: Could not open file
		Logger::GetInstance()->logMessage("Error(-5) while reading communication logs", 3, 0);
		return -5;
    }
}
