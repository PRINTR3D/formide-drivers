/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */

#ifndef HANDLER_DEVICECENTER_H_
#define HANDLER_DEVICECENTER_H_

#include <map>
#include <nan.h>
#include "../Blocker/Blocker.h"
#include "../PrinterData/PrinterData.h"
#include "../../driver/MarlinDriver/MarlinDriver.h"

class DeviceCenter {
public:

	// Constructor / Deconstructor
    DeviceCenter();
    virtual ~DeviceCenter();

    // NodeJS Callback
    void setCallback(Nan::Callback* call);
    Nan::Callback* getCallback();
    Nan::Callback* callback;

    int milisecUpdate;

    // Unique instance
    static DeviceCenter*
    GetInstance();

    // Error Events
    void extruderJam(std::string port);
    void printerErrorEvent(std::string port, std::string key, std::string message);
    void printerWarningEvent(std::string port, std::string key, std::string message);
    void printerInfoEvent(std::string port, std::string key, std::string message);

    // Info events
    void printerInfoEvent(std::string port, std::string key, int message);

    // Printer events
    void printerOnline(std::string port, int baudrate);
    void printerConnected(std::string port);
    void printerDisconnected(std::string port);
    void printFinished(std::string port);

    // Driver functions
    MarlinDriver* getDriverFromPrinter(std::string printerID);
    void newDriverForPrinter(std::string printerID, bool sendEvent);
	void updateConnected(std::vector<bool> booleans, bool empty);
    bool isConnected(std::string printerID);
    void reconnectPrinter(int id);

    // Buffer
	int GetTotalLines(std::string printerID);
	void resetBuffer(std::string printerID);
	void addBufferedLines(std::string printerID, int numberOfLines);
	int getBufferedLines(std::string printerID);
	int isBufferAvailable(std::string printerID);

	// Printer Data
	std::vector<PrinterData> getPrinterDataList();
	PrinterData* getPrinterInfo(std::string printerID, bool* gotit);
	std::vector<std::string> getPrinterList();
	std::vector<bool> getConnected();
	std::vector<PrinterData> PrinterDataList;
	std::vector<std::string> printerList;
	std::vector<bool> connectedList;

	// Read communication logs
	std::string getLastLines(std::string const& filename, int lineCount);
	std::string skipLines(std::string lastLines, int lineCount);
	int readCommunicationLog(std::string printerID, int lineCount, int skipCount, std::string& res);

    // Printer Status Update
    void updatePrintJobID(std::string printerID, int printjobid);
    void updateExtrudersTemp(std::string printerID, std::vector<int> extrudersTemp);
    void updateExtrudersTargetTemp(std::string printerID, std::vector<int> extrudersTargetTemp);
    void updateBedTemperature(std::string printerID, std::vector<int> bedTemp);
    void updateState(std::string printerID, std::string state);
    void updateTimeNow(std::string printerID, std::string time);
    void updateProgress(std::string printerID, int progress);
    void updateCurrentLine(std::string printerID, int currentLine);
    void updateTotalLines(std::string printerID, int totalLines);
    void updatePrintingTime(std::string printerID, float totalSeconds);
    void updateRatio(std::string printerID, int ratio);
    void updateFanSpeed(std::string printerID, double fanSpeed);
    void updateFlowRate(std::string printerID, int flowRate);
    void updateSpeedMultiplier(std::string printerID, int speedMultiplier);
    void updateFirmwareName(std::string printerID, std::string firmwareName);
	void updateFirmwareFilament(std::string printerID, std::string firmwareFilament);
	void updateFirmwareTime(std::string printerID, std::string firmwareTime);
	void updateCoordinateX(std::string printerID, double coordinate);
	void updateCoordinateY(std::string printerID, double coordinate);
	void updateCoordinateZ(std::string printerID, double coordinate);
	void updateCoordinateE(std::string printerID, double coordinate);
	void updateSpeed(std::string printerID, int speed);
	void updateCurrentlyPrinting(std::string printerID, std::string currently);
	void updateCurrentLayer(std::string printerID, int layer);
    void updateUsedExtruder(std::string printerID, int usedExtruder);
    void homeX(std::string printerID);
    void homeY(std::string printerID);
    void homeZ(std::string printerID);


private:
    int readBaudrate(std::string printerID);
    void writeBaudrate(std::string printerID, int baudrate);
    static DeviceCenter* instance; // Unique instance
};

#endif /* HANDLER_DEVICECENTER_H_ */
