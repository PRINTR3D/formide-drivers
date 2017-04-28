/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


#include "PrinterData.h"

// Printer Data class for Printer Status
PrinterData::PrinterData()
{

    driver = NULL;
    serialPort = "";
    bedTemperature = 0;
    targetBedTemperature = 0;
    printjobID = 0;

    timeStarted = "";
    timeNow = "";

    extruderTemperature.clear();
    extruderTargetTemperature.clear();

    state = "disconnected";
    currentLine = 0;
    totalLines = 0;
    printingTime = 0;
    remainingPrintingTime = 0;
    progress = 0;

    coordinateX = 0;
    coordinateY = 0;
    coordinateZ = 0;

    coordinateE = 0;

    currentlyPrinting = "";
    currentLayer = 0;

    usedExtruder = 0;

    ratio = 0;
    speed = 0;
    fanSpeed = 100;
    flowRate = 100;
    speedMultiplier = 100;

    bufferedLines = 0;

    firmwareName = "";
    firmwareFilament = "";
    firmwareTime = "";
}

PrinterData::~PrinterData()
{
}
