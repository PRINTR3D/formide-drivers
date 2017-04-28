/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


#ifndef HANDLER_PRINTERDATA_H_
#define HANDLER_PRINTERDATA_H_

#include <vector>
#include "../../driver/MarlinDriver/MarlinDriver.h"

class PrinterData {
public:
    PrinterData();
    virtual ~PrinterData();

    MarlinDriver* driver;
    std::string serialPort;

    int bedTemperature;
    int targetBedTemperature;

    std::vector<int> extruderTemperature;
    std::vector<int> extruderTargetTemperature;

    std::string timeStarted;
    std::string timeNow;

    int printjobID;

    int speed;

    double coordinateE;
    double coordinateX;
    double coordinateY;
    double coordinateZ;

    std::string currentlyPrinting;
    int currentLayer;

    int usedExtruder;
    int currentLine;
    int totalLines;
    float printingTime;
    float remainingPrintingTime;
    int progress;

    double ratio;
    double fanSpeed;
    double flowRate;
    double speedMultiplier;

    std::string firmwareName;
    std::string firmwareFilament;
    std::string firmwareTime;

    std::string state;

    int bufferedLines;
};

#endif /* HANDLER_PRINTERDATA_H_ */
