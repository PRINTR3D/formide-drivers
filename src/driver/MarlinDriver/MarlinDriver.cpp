/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


#include <cmath>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MarlinDriver.h"
#include "../../utils/Logger/Logger.h"
#include "../../handler/Blocker/Blocker.h"
#include "../../handler/DeviceCenter/DeviceCenter.h"

using std::string;
using std::size_t;

const int MarlinDriver::ITERATION_INTERVAL = 200;

const std::string currentDateTim() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    return buf;
}

MarlinDriver::MarlinDriver(const std::string& port, const uint32_t& baudrate){

	if (port == "/dev/ttyS1")
	{
		Logger::GetInstance()->logMessage("ttyS1 detected",1,0);
		connected=false;
	}
	else
	{
		Logger::GetInstance()->logMessage("New Marlin driver",1,0);
		connected=true;
	}


	checksumControl=new ChecksumControl();
	communicationLogger=new CommLogger(port);


	// Serial variables
	serialPortPath_=port;
	openSerialAttempt=0;
	maxOpenSerialAttempt=5;
	printerNotValid=false;

	// Baud Rate variables
	baudrate_=baudrate;
	baudRateAttempt=0;
	maxBaudRateAttempt=9;

	// Raw buffer
	rawLineBuffer.clear();

	// State variables
	paused_=false;
	state=DISCONNECTED;
	blocked_unit = NONE;

	// Temperature timer variables
	temperatureTimer_.start();
	temperatureOrCoordinate=true;
	checkTemperatureInterval_=2800;
	checkConnection_=true;
	checkTemperatureAttempt_=0;
	maxCheckTemperatureAttempts_=2;

	// Key message variables
	formatErrorReceived=false;
	waitReceived=false;
	skipMessageReceived=false;

	// Resend logic variables
	resendReceived=false;
	previousMessageSentWasAResend=false;
	resendAttempt=0;
	maxResendAttempt=400;
	resendCounter=0;
	resendMessage="";

	// Checksum variables
	skipFormat=false;

	targetZeroPatch = 0;
	targetExtruderPatch= 0;

	//Action control variables
	usedExtruder=0;
	temperatureCommandNeedsToBeSent=false;
	receivedOkWithoutNewTemperatureCommand=false;
	iterationsWithoutCheckingTemperatureWhileHeating=0;
	maxIterationsWithoutCheckingTemperatureWhileHeating=30;
	printResumedAfterPause=true;

	// Serial communication error
	previousMessageSerialError=false;
	serialCommand="";


	// Resume position and E value
	mustReturnToResumePosition=false;
	resumeEvalue=0;

	/* PATCHS */
	// Builder patch: Filled buffer with "Echo: Unknown command"
	numEchos=0;

	// 3DGence patch: Printer sends "A:ok" instead of "ok"
	gence3d=false;

}

int
MarlinDriver::openConnection() {

	Logger::GetInstance()->logMessage("Using port "+serialPortPath_,1,0);
	int rv = serial_.open(serialPortPath_.c_str());
	if (rv < 0) {
		if(state != CONNECTING) setState(CONNECTING);
		return rv;
	}
	setBaudrate(baudrate_);
}


int
MarlinDriver::closeConnection() {
	setState(DISCONNECTED);
	 serial_.close();
	 return 0;
}

int
MarlinDriver::reconnect() {

	Logger::GetInstance()->logMessage("reconnecting...",1,0);
	while (closeConnection() < 0){
		Logger::GetInstance()->logMessage("Closing connection...",1,0);
		usleep(200000);
	}

	while (openConnection() < 0){
		usleep (200000);
	}
	DeviceCenter::GetInstance()->printerOnline(serialPortPath_, baudrate_);
	setState(ONLINE);


	return 0;

}

bool
MarlinDriver::isConnected() const {
	return serial_.isOpen();
}

bool
MarlinDriver::isPrinterOnline() const {
	STATE s = getState();
	return s != OFFLINE && s != DISCONNECTED && s != CONNECTING;
}

uint32_t
MarlinDriver::getBaudrate(){
	return baudrate_;
}

void
MarlinDriver::setBaudrate(uint32_t baudrate) {

	baudrate_ = baudrate;
	Serial::ESERIAL_SET_SPEED_RESULT ssr = serial_.setSpeed(baudrate_);
	if(ssr == Serial::SSR_OK) {
		setState(CONNECTING);
	} else {
		Logger::GetInstance()->logMessage("Invalid baudrate",1,0);
	}
}

void
MarlinDriver::switchBaudrate() {

	if(baudrate_ == 250000){ setBaudrate(57600); Logger::GetInstance()->logMessage("Switching baudrate to 57600",1,0);}
	else if(baudrate_== 115200){ setBaudrate(250000);Logger::GetInstance()->logMessage("Switching baudrate to 250000",1,0);}
	else if(baudrate_==57600){ setBaudrate(115200); Logger::GetInstance()->logMessage("Switching baudrate to 115200",1,0);  }

	baudRateAttempt++;

	if(baudRateAttempt > maxBaudRateAttempt)
	{
		printerNotValid=true;
		setState(DISCONNECTED);
		if (serialPortPath_ != "/dev/ttyS1")
			DeviceCenter::GetInstance()->printerDisconnected(serialPortPath_);
	}
}

int32_t MarlinDriver::getCurrentLine() const {
	return gcodeBuffer_.getCurrentLine();
}

int32_t MarlinDriver::getBufferedLines() const {
	return gcodeBuffer_.getBufferedLines();
}

int32_t MarlinDriver::getTotalLines() const {
	return gcodeBuffer_.getTotalLines();
}

void MarlinDriver::setGCode(const string& gcode, bool buff) {
	gcodeBuffer_.set(gcode);
}

void MarlinDriver::appendGCode(const string& gcode, bool thread) {
	gcodeBuffer_.append(gcode);
}

void MarlinDriver::clearGCode() {
	gcodeBuffer_.clear();
	STATE s = getState();
	if (s == PRINTING || s == HEATING || s == STOPPING) setState(ONLINE);
}

MarlinDriver::STATE MarlinDriver::getState() const {
	return state;
}


void MarlinDriver::setStatePrinting(){
	Logger::GetInstance()->logMessage("Setting_state: "+std::to_string(5),1,0);
	state = PRINTING;
	DeviceCenter::GetInstance()->updateState(serialPortPath_, "printing");

}

void MarlinDriver::setStateOnline(){
	Logger::GetInstance()->logMessage("Setting_state: "+std::to_string(3),1,0);
	state = ONLINE;
	DeviceCenter::GetInstance()->updateState(serialPortPath_, "online");

}

void MarlinDriver::setState(STATE state_) {

	if(state == state_)
	{
		return;
	}
	Logger::GetInstance()->logMessage("Setting state: "+std::to_string(state_),1,0);
	state = state_;

	int nothing = 0;
	string state_s;
	switch (state){
	case 0:
		state_s = "disconnected";
		break;
	case 1:
		state_s = "disconnected";
		break;
	case 2:
		state_s = "connecting"; //ok
		break;
	case 3:
		state_s = "online"; //ok
		break;
	case 4:
		state_s = "printing"; //ok
		paused_ = false;
		break;
	case 5:
		state_s = "printing"; //ok
		break;
	case 6:
		state_s = "printing";
		break;
	case 7:
		state_s = "paused"; //ok
		break;
	case 8:
		state_s = "heating"; //ok
		paused_ = false;
		break;
	case 10:
		state_s = "heating";
		break;
	default:
		nothing = 0;
	}
	DeviceCenter::GetInstance()->updateState(serialPortPath_, state_s);

}

int MarlinDriver::mainIteration() {

	// No logic when printer is not valid
	if(printerNotValid)
		return -1;

	int nothing = 0;

	// Serial communication logic
	if (!isConnected())
	{
		if(!checkConnection_ || (checkConnection_ && temperatureTimer_.getElapsedTimeInMilliSec() > checkTemperatureInterval_))
		{

			serial_.open(serialPortPath_.c_str());

			if (!isConnected())
			{
				Logger::GetInstance()->logMessage("Can not open serial connection with: "+serialPortPath_,1,0);
				if(openSerialAttempt > maxOpenSerialAttempt)
				{
					openSerialAttempt = 0;
					switchBaudrate();
				}
				openSerialAttempt++;
				return -1;
			}
		}
	}

	// If the previous iteration gave a serial communication error
	// Message is resent
	if(previousMessageSerialError)
	{
		writeToSerial(serialCommand);
		previousMessageSerialError=false;
	}

	// Send next line if printer is resumed and no new command was sent
	else if(!printResumedAfterPause)
	{
		sendNextMessage();
	}

	else
	{
		// Temperature timer
		if (temperatureTimer_.getElapsedTimeInMilliSec() > checkTemperatureInterval_) {

				temperatureTimer_.start();

				// CONNECTING LOGIC
				// Check temperature at the beginning of a connection
				// Printer becomes online after a temperature message has been received and parsed
				if (checkConnection_) {

					if (checkTemperatureAttempt_ < maxCheckTemperatureAttempts_)
					{
						Logger::GetInstance()->logMessage("Sending temperature message to test connection",1,0);
						sendNextMessage();
						checkTemperatureAttempt_++;
					}
					else
					{
						// Increase check attempts to four
						// For more printer compatibility
						maxCheckTemperatureAttempts_=4;
						checkTemperatureAttempt_ = 0;

						// Switch baud rate and test again
						switchBaudrate();
					}
				}

				// ONLINE LOGIC
				else
				{
					// Do not send message when printing or heating (blocked)
					if(state != PRINTING && state != HEATING && state != STOPPING)
					{
						sendNextMessage();
					}

				}
		}

		// PRINTING LOGIC
		else
		{
			// When printing, do not wait until timer is triggered to read from serial
			if (state == PRINTING || state == HEATING || state == STOPPING || timer_.getElapsedTimeInMilliSec() > ITERATION_INTERVAL) {

				int rv = readFromSerial();
				int secure = 0;

				if (rv > 0) {

					if(!connected)
					{
						connected=true;
						Logger::GetInstance()->logMessage("ttyS1 received data",3,0);
						DeviceCenter::GetInstance()->printerConnected(serialPortPath_);
					}

					string* line;
					bool echo = false;
					while((line = serial_.extractLine()) != NULL) {

						// Save communication logs
						communicationLogger->logCommunicationMessage(*line,true);

						// Parse response to get printer status and perform actions
						parseResponseFromPrinter(*line,&echo);

						// Free memory
						delete line;


						// Patch: Some firmwares get stuck in a loop of sending messages non-stop
						// exit the while loop
						secure++;
						if (secure > 200)
						{
							Logger::GetInstance()->logMessage("Endless loop alert: 1",1,0);
							break;
						}
					}

					// Virtual heat and wait logic
					if(state == HEATING_BLOCKED)
					{
						checkIfTemperaturesAreReached();
					}
				}
				else
				{
					if(resendReceived)
					{
						// Patch: Some firmwares get stuck sending Resend without sending OK
						// This forces sending the next command when no OK is received
						resendAttempt++;
						if(resendAttempt > maxResendAttempt)
						{
							Logger::GetInstance()->logMessage("Force: Sending next command after not receiving OK",3,0);
							sendNextMessage();
						}
					}
				}

				// ONLINE LOGIC
				if( state == ONLINE && rawLineBuffer.size() > 0)
				{
					sendNextMessage();
				}

				timer_.start(); //restart timer
			}

			//request to be called again after the time that's left of the update interval
			return ITERATION_INTERVAL - timer_.getElapsedTimeInMilliSec();
		}
	}
}


// FEATURE: Virtual heat and wait, replace command
// When sending M109/M190, no other messages can be sent to the printer, and a print can't be cancel or paused.
// This feature simulates the blocking status of those commands, replacing M109/M190 with M104/M140
// allowing other commands to be sent in between, as well as pausing/stopping the current print

string
MarlinDriver::replaceTemperatureCommandIfNecessary(string code)
{

	string returnCode = code;

	if(code.find("M109") != std::string::npos)
	{
		try
		{
			std::size_t positionCode = code.find("M109",0);
			std::string stringBeginning = code.substr(0,positionCode);

			std::size_t positionFirstSpace = code.find(" ",1);
			int sequenceNumber = std::stoi(code.substr(1,positionFirstSpace-1));

			std::size_t positionSecondSpace = code.find(" ",positionCode);

			std::string leftString;
			std::string convertedCode;

			if(positionSecondSpace != std::string::npos)
			{
				leftString = code.substr(positionSecondSpace);
				convertedCode=stringBeginning+"M104"+leftString;
				Logger::GetInstance()->logMessage("Converted extruder heating command: "+convertedCode,3,0);

				string finalCode = checksumControl->updateChecksum(sequenceNumber,convertedCode);
				Logger::GetInstance()->logMessage("Converted extruder heating command with UPDATED checksum: "+finalCode,3,0);

				targetZeroPatch = 0;
				temperatureTimer_.start();
				setState(HEATING_BLOCKED);
				checkTemperatureInterval_=200;


				// Return replaced code to be sent to the printer
				returnCode=finalCode;

				// Parse new temperature
				std::size_t positionS = leftString.find("S");
				int newTemperature=200;
				if(positionS != std::string::npos)
				{
					std::size_t positionSpace = leftString.find(" ",positionS);
					if(positionSpace != std::string::npos)
					{
						std::string newTemperatureString = leftString.substr(positionS+1,positionSpace-positionS-1);
						newTemperature = std::stoi(newTemperatureString);
					}
				}
				std::vector<int>targetVector;

				// Check extruder again
				if(code.find("T0") != std::string::npos)
					usedExtruder=0;
				if(code.find("T1") != std::string::npos)
					usedExtruder=1;

				if(usedExtruder==0)
				{
					blocked_unit = EXTRUDER0;
					bool gotIt;
					PrinterData *printer = DeviceCenter::GetInstance()->getPrinterInfo(serialPortPath_,&gotIt);
					if(gotIt)
					{
						if(printer->extruderTargetTemperature.size()>1)
						{
							targetVector.push_back(newTemperature);
							targetVector.push_back(printer->extruderTargetTemperature[1]);
						}
						else
						{
							targetVector.push_back(newTemperature);
						}
						Logger::GetInstance()->logMessage("Target temperature T0 changed",4,0);

						DeviceCenter::GetInstance()->updateExtrudersTargetTemp(serialPortPath_,  targetVector);
					}

					temperatureCommandNeedsToBeSent=false;
				}
				if(usedExtruder==1)
				{
					blocked_unit = EXTRUDER1;

					bool gotIt;
					PrinterData *printer = DeviceCenter::GetInstance()->getPrinterInfo(serialPortPath_,&gotIt);
					if(gotIt)
					{
						if(printer->extruderTargetTemperature.size()>1)
						{
							targetVector.push_back(printer->extruderTargetTemperature[0]);
							targetVector.push_back(newTemperature);
						}
						else
						{
							targetVector.push_back(newTemperature);
						}
						Logger::GetInstance()->logMessage("Target temperature T1 changed",4,0);
						DeviceCenter::GetInstance()->updateExtrudersTargetTemp(serialPortPath_,  targetVector);
					}

					temperatureCommandNeedsToBeSent=false;
				}

				// In state heating_blocked, all gcodes sent are from raw buffer, not gcode buffer
				if (rawLineBuffer.size() == 0)
				{
					Logger::GetInstance()->logMessage("Pushing temperature line to raw buffer: 2",4,0);
					rawLineBuffer.push_back("M105");
				}
				else
				{
					Logger::GetInstance()->logMessage("Not pushing anything: 2",4,0);

				}
			}
		}

		catch(...)
		{
			Logger::GetInstance()->logMessage("Problem parsing blocking heating command",2,0);

			// If there is a problem with parsing, gcode is not converted
			returnCode=code;
		}
	}

	else if(code.find("M190") != std::string::npos)
	{
		try
		{
			std::size_t positionCode = code.find("M190",0);
			std::string stringBeginning = code.substr(0,positionCode);


			std::size_t positionFirstSpace = code.find(" ",1);
			int sequenceNumber = std::stoi(code.substr(1,positionFirstSpace-1));

			std::size_t positionSecondSpace = code.find(" ",positionCode);

			std::string leftString;
			std::string convertedCode;

			if(positionSecondSpace != std::string::npos)
			{
				leftString = code.substr(positionSecondSpace);
				convertedCode=stringBeginning+"M140"+leftString;
				Logger::GetInstance()->logMessage("Converted extruder heating command: "+convertedCode,4,0);

				string finalCode = checksumControl->updateChecksum(sequenceNumber,convertedCode);
				Logger::GetInstance()->logMessage("Converted extruder heating command with UPDATED checksum: "+finalCode,4,0);

				targetZeroPatch = 0;
				temperatureTimer_.start();
				setState(HEATING_BLOCKED);
				checkTemperatureInterval_=200;
				blocked_unit = BED;
				temperatureCommandNeedsToBeSent=false;

				// Return replaced code to be sent to the printer
				returnCode=finalCode;

				if (rawLineBuffer.size() == 0)
				{
					Logger::GetInstance()->logMessage("Pushing temperature line to raw buffer: 3",4,0);
					rawLineBuffer.push_back("M105");
				}
				else
				{
					Logger::GetInstance()->logMessage("Not pushing anything: 3",4,0);

				}
			}

		}

		catch(...)
		{
			Logger::GetInstance()->logMessage("Problem parsing blocking heating command",2,0);

			// If there is a problem with parsing, gcode is not converted
			returnCode = code;
		}
	}

	else
	{
		returnCode=code;
	}

	return returnCode;

}


// FEATURE: Virtual heat and wait, check temperatures
void
MarlinDriver::checkIfTemperaturesAreReached(){

	bool gotit;
	PrinterData* data = DeviceCenter::GetInstance()->getPrinterInfo(serialPortPath_, &gotit);

	if(blocked_unit == EXTRUDER0)
	{
		Logger::GetInstance()->logMessage("Heating extruder 0. T: "+
				std::to_string(data->extruderTemperature[0])+"/"+std::to_string(data->extruderTargetTemperature[0]),4,0);

		if(data->extruderTargetTemperature[0] == 0)
		{
			targetZeroPatch++;
		}
		if ((data->extruderTargetTemperature[0] - data->extruderTemperature[0] ) < 4)
		{
			targetExtruderPatch++;
		}
		if( targetExtruderPatch>4 || targetZeroPatch>4 )
		{

			Logger::GetInstance()->logMessage("Temperature T0 OK",3,0);
			blocked_unit = NONE;
			targetZeroPatch=0;
			targetExtruderPatch=0;
			printResumedAfterPause=false;
			setState(PRINTING);

			// A temperature message is sent, in order to receive an OK and continue printing
			if(temperatureCommandNeedsToBeSent)
			{
				sendCode(checksumControl->getFormattedMessage("M105"));
				checkTemperatureInterval_ = 3000;
				temperatureTimer_.start();
				temperatureCommandNeedsToBeSent=false;
			}
		}
	}
	if(blocked_unit == EXTRUDER1)
	{
		Logger::GetInstance()->logMessage("Heating extruder 1. T: "+
			std::to_string(data->extruderTemperature[1])+"/"+std::to_string(data->extruderTargetTemperature[1]),4,0);

		if(data->extruderTargetTemperature[1] == 0)
		{
			targetZeroPatch++;
		}
		if ((data->extruderTargetTemperature[1] - data->extruderTemperature[1] ) < 4)
		{
			targetExtruderPatch++;
		}
		if( targetExtruderPatch>4 || targetZeroPatch>4 )
		{
			Logger::GetInstance()->logMessage("Temperature T1 OK",3,0);
			blocked_unit = NONE;
			targetExtruderPatch=0;
			targetZeroPatch=0;
			printResumedAfterPause=false;
			setState(PRINTING);

			// A temperature message is sent, in order to receive an OK and continue printing
			if(temperatureCommandNeedsToBeSent)
			{
				sendCode(checksumControl->getFormattedMessage("M105"));
				checkTemperatureInterval_ = 3000;
				temperatureTimer_.start();
				temperatureCommandNeedsToBeSent=false;
			}
		}
	}
	if(blocked_unit == BED)
	{

		Logger::GetInstance()->logMessage("Heating Bed. T: "+
			std::to_string(data->bedTemperature)+"/"+std::to_string(data->targetBedTemperature),4,0);

		if(data->targetBedTemperature == 0)
		{
			targetZeroPatch++;
		}
		if((data->targetBedTemperature - data->bedTemperature ) < 3)
		{
			Logger::GetInstance()->logMessage("Temperature BED OK",3,0);
			blocked_unit = NONE;
			targetZeroPatch=0;
			printResumedAfterPause=false;
			setState(PRINTING);

			// A temperature message is sent, in order to receive an OK and continue printing
			if(temperatureCommandNeedsToBeSent)
			{
				sendCode(checksumControl->getFormattedMessage("M105"));
				checkTemperatureInterval_ = 3000;
				temperatureTimer_.start();
				temperatureCommandNeedsToBeSent=false;
			}
		}
	}

}

//TODO: Get rid of callback
void
MarlinDriver::pushRawCommand (const std::string& gcode) {

	rawLineBuffer.push_back(gcode);
}

// Get extra printer status from gcode commands such as fan speed, flow rate, speed multiplier, extruder ratio used, etc.
bool MarlinDriver::extraStatusFromGcode(const std::string& gcode)
{
			bool checksumMessage = gcode.find("N") == 0;
			std::string code;

			if(checksumMessage)
			{
				std::size_t positionFirstSpace = gcode.find(" ",0);
				code = gcode.substr(positionFirstSpace+1);
			}
			else
			{
				code=gcode;
			}

			//Handlers to update printer status
			// Printer coordinates

			bool extrusion = code.find("G1") != std::string::npos;
			bool nonExtrusion = code.find("G0") != std::string::npos;

			bool coordinateX = false;// code.find("X") != std::string::npos ;
			bool coordinateY = false;// code.find("Y") != std::string::npos ;
			bool coordinateE = code.find("E") != std::string::npos ;

			// BUILDER UGLY PATCH
			bool coordinateZ = code.find("Z") != std::string::npos ;
			if(state!=PRINTING)
				coordinateZ=false;



			bool speedF = code.find("F") != std::string::npos;

			bool isComment = code.find(";") == 0 ;
			bool layerComment = code.find("LAYER:") != std::string::npos;
			bool isEmpty = code.size()<2;


			bool typeType = code.find(";TYPE:") != std::string::npos ;

			bool home = code.find("G28") != std::string::npos ;



			bool ratioFound = code.find("G93 R") != std::string::npos ;


			//new more printer status

			bool fanSpeedFound = code.find("M106 S") != std::string::npos ;
			bool flowRateFound = code.find("M221 S") != std::string::npos ;
			bool speedMultiplierFound = code.find("M220 S") != std::string::npos ;

			bool targetTempBlockingFound = code.find("M109") != std::string::npos;
			bool targetTempFound = code.find("M104") != std::string::npos;
			bool targetBedTempFound = code.find("M140") != std::string::npos;
			bool targetBedTempBlockingFound = code.find("M190") != std::string::npos;

			bool T0Found = code.find("T0") != std::string::npos;
			bool T1Found = code.find("T1") != std::string::npos;

			bool M401Found = code.find("M401") != std::string::npos;


			if(isEmpty) Logger::GetInstance()->logMessage("WARNING: EMPTY GCODE LINE",3,0);


	try
		{

			if(isComment && layerComment)
			{
				std::size_t layerPosition = code.find("LAYER:");
				layerPosition+=6;

				if(code.substr(layerPosition,1) == " ")
					layerPosition++;

				std::size_t positionSpace = code.find(" ",layerPosition);
				std::string layerString;

				if(positionSpace != std::string::npos)
				{
					layerString = code.substr(layerPosition, positionSpace-layerPosition);
				}
				else
				{
					layerString = code.substr(layerPosition);
				}

				if(layerString.size() > 0)
				{
					Logger::GetInstance()->logMessage("Updating layer number to "+layerString,2,0);
					DeviceCenter::GetInstance()->updateCurrentLayer(serialPortPath_,std::stoi(layerString));
				}
			}
			if(M401Found)
			{
				mustReturnToResumePosition=true;
				bool nothing=false;
				resumeEvalue=DeviceCenter::GetInstance()->getPrinterInfo(serialPortPath_,&nothing)->coordinateE;
				std::cout << "Saving last extrusion value: " << resumeEvalue <<std::endl;

			}
			if(T0Found && !targetTempFound)
			{
				usedExtruder=0;
				DeviceCenter::GetInstance()->updateUsedExtruder(serialPortPath_,0);
			}

			if(T1Found && !targetTempFound)
			{
				usedExtruder=1;
				DeviceCenter::GetInstance()->updateUsedExtruder(serialPortPath_,1);
			}

			if(targetTempBlockingFound)
			{
				if(state!=STOPPING && state!=PAUSED)
					{
					Logger::GetInstance()->logMessage("Heating: 2",3,0);
					setState(HEATING);//2
					}
			}


			if (targetTempFound )
			{
				size_t pos = code.find("S");

				if (pos != std::string::npos)
				{
					pos++;
					std::string targetTemperature = code.substr(pos);

					std::vector<int>targetVector;

					int used_extruder = usedExtruder;
					// Check extruder again
					if(code.find("T0") != std::string::npos)
						used_extruder=0;
					if(code.find("T1") != std::string::npos)
						used_extruder=1;


					if(used_extruder==0)
					{
						bool gotIt;
						PrinterData *printer = DeviceCenter::GetInstance()->getPrinterInfo(serialPortPath_,&gotIt);
						if(gotIt)
						{
							if(printer->extruderTargetTemperature.size()>1)
							{
								targetVector.push_back(std::stoi(targetTemperature));
								targetVector.push_back(printer->extruderTargetTemperature[1]);
							}
							else
							{
								targetVector.push_back(std::stoi(targetTemperature));
							}
							Logger::GetInstance()->logMessage("Updating ext temperatures from extra status",4,0);

							DeviceCenter::GetInstance()->updateExtrudersTargetTemp(serialPortPath_,  targetVector);
						}

					}
					if(used_extruder==1)
					{
						bool gotIt;
						PrinterData *printer = DeviceCenter::GetInstance()->getPrinterInfo(serialPortPath_,&gotIt);
						if(gotIt)
						{
							if(printer->extruderTargetTemperature.size()>1)
							{
								targetVector.push_back(printer->extruderTargetTemperature[0]);
								targetVector.push_back(std::stoi(targetTemperature));
							}
							else
							{
								targetVector.push_back(std::stoi(targetTemperature));
							}
							Logger::GetInstance()->logMessage("Updating ext temperatures from extra status",4,0);

							DeviceCenter::GetInstance()->updateExtrudersTargetTemp(serialPortPath_,  targetVector);
						}

					}
				}
			}

			if(targetBedTempFound || targetBedTempBlockingFound)
			{
				size_t pos = code.find("S");

				if (pos != std::string::npos)
				{
					pos++;
					std::string targetTemperature = code.substr(pos);

					std::vector<int>targetVector;
					bool gotit;
					PrinterData* data = DeviceCenter::GetInstance()->getPrinterInfo(serialPortPath_, &gotit);

					if(gotit)
					{
						targetVector.push_back(data->bedTemperature);
					}
					else
					{
						targetVector.push_back(std::stoi(targetTemperature));
					}
					targetVector.push_back(std::stoi(targetTemperature));
					DeviceCenter::GetInstance()->updateBedTemperature(serialPortPath_,  targetVector);

					if(targetBedTempBlockingFound)
						if(state!=STOPPING && state!=PAUSED)
						{
							Logger::GetInstance()->logMessage("Heating: 3",3,0);
							setState(HEATING);//3
						}



				}
			}

			if (home)
			{
				bool homeX = code.find("X") != std::string::npos ;
				bool homeY = code.find("Y") != std::string::npos ;
				bool homeZ = code.find("Z") != std::string::npos ;

				if(homeX)
					DeviceCenter::GetInstance()->homeX(serialPortPath_);
				if(homeY)
					DeviceCenter::GetInstance()->homeY(serialPortPath_);
				if(homeZ)
					DeviceCenter::GetInstance()->homeZ(serialPortPath_);

				if(!homeX && !homeY && !homeZ)
				{
					DeviceCenter::GetInstance()->homeX(serialPortPath_);
					DeviceCenter::GetInstance()->homeY(serialPortPath_);
					DeviceCenter::GetInstance()->homeZ(serialPortPath_);
				}

				if ( (homeZ || (!homeX && !homeY && !homeZ)) && state==ONLINE )
				{
					DeviceCenter::GetInstance()->printerInfoEvent(serialPortPath_,"home_command","home command was sent to the printer");
				}
			}

			if (ratioFound)
			{
				std::size_t ratioPosition = code.find("G93 R");
				ratioPosition+=5;

				if(code.substr(ratioPosition,1) == " ")
					ratioPosition++;

				std::size_t positionSpace = code.find(" ",ratioPosition);
				std::string ratio;

				if(positionSpace != std::string::npos)
				{
					ratio = code.substr(ratioPosition, positionSpace-ratioPosition);
				}
				else
				{
					ratio = code.substr(ratioPosition);
				}

				if(ratio.size() > 0)
				{
					Logger::GetInstance()->logMessage("Updating Ratio to "+ratio,2,0);
					DeviceCenter::GetInstance()->updateRatio(serialPortPath_,std::stoi(ratio));
				}
			}

			if (fanSpeedFound)
			{
				std::size_t fanSpeedPosition = code.find("M106 S");
				fanSpeedPosition+=6;

				if(code.substr(fanSpeedPosition,1) == " ")
					fanSpeedPosition++;

				std::size_t positionSpace = code.find(" ",fanSpeedPosition);
				std::string fanSpeed;

				if(positionSpace != std::string::npos)
				{
					fanSpeed = code.substr(fanSpeedPosition, positionSpace-fanSpeedPosition);
				}
				else
				{
					fanSpeed = code.substr(fanSpeedPosition);
				}

				if(fanSpeed.size() > 1)
				{
					//std::cout << "Updating fanSpeed ("<<fanSpeed<<")" << std::endl;
					DeviceCenter::GetInstance()->updateFanSpeed(serialPortPath_,std::stod(fanSpeed));
				}
			}

			if (flowRateFound)
			{
				std::size_t flowRatePosition = code.find("M221 S");
				flowRatePosition+=6;

				if(code.substr(flowRatePosition,1) == " ")
					flowRatePosition++;

				std::size_t positionSpace = code.find(" ",flowRatePosition);
				std::string flowRate;

				if(positionSpace != std::string::npos)
				{
					flowRate = code.substr(flowRatePosition, positionSpace-flowRatePosition);
				}
				else
				{
					flowRate = code.substr(flowRatePosition);
				}

				if(flowRate.size() > 1)
				{
					//std::cout << "Updating flowRate ("<<flowRate<<")" << std::endl;
					DeviceCenter::GetInstance()->updateFlowRate(serialPortPath_,std::stod(flowRate));
				}
			}

			if (speedMultiplierFound)
			{
				std::size_t speedMultiplierPosition = code.find("M220 S");
				speedMultiplierPosition+=6;

				if(code.substr(speedMultiplierPosition,1) == " ")
					speedMultiplierPosition++;

				std::size_t positionSpace = code.find(" ",speedMultiplierPosition);
				std::string speedMultiplier;

				if(positionSpace != std::string::npos)
				{
					speedMultiplier = code.substr(speedMultiplierPosition, positionSpace-speedMultiplierPosition);
				}
				else
				{
					speedMultiplier = code.substr(speedMultiplierPosition);
				}

				if(speedMultiplier.size() > 1)
				{
					//std::cout << "Updating speedMultiplier ("<<speedMultiplier<<")" << std::endl;
					DeviceCenter::GetInstance()->updateSpeedMultiplier(serialPortPath_,std::stod(speedMultiplier));
				}
			}


			if(extrusion || nonExtrusion)
			{
				if (coordinateX)
				{
					std::size_t positionStringX = code.find("X");


					positionStringX++;

					if(code.substr(positionStringX,1) == " ")
						positionStringX++;


					std::size_t positionSpace = code.find(" ",positionStringX);

					std::string strCoordinateX;

					if(positionSpace != std::string::npos)
					{
						strCoordinateX = code.substr(positionStringX, positionSpace-positionStringX);
					}
					else
					{
						strCoordinateX = code.substr(positionStringX);
					}
					//std::cout << "We have found the coordinate X ("<<strCoordinateX<<")" << std::endl;
					DeviceCenter::GetInstance()->updateCoordinateX(serialPortPath_,std::stod(strCoordinateX));
				}
				if (coordinateY)
				{
					std::size_t positionStringY = code.find("Y");

					positionStringY++;

					if(code.substr(positionStringY,1) == " ")
						positionStringY++;
					std::size_t positionSpace = code.find(" ",positionStringY);
					std::string strCoordinateY;

					if(positionSpace != std::string::npos)
					{
						strCoordinateY = code.substr(positionStringY, positionSpace-positionStringY);
					}
					else
					{
						strCoordinateY = code.substr(positionStringY);
					}
					//std::cout << "We have found the coordinate Y ("<<strCoordinateY<<")" << std::endl;
					DeviceCenter::GetInstance()->updateCoordinateY(serialPortPath_,std::stod(strCoordinateY));
				}
				if (coordinateZ)
				{
					std::size_t positionStringZ = code.find("Z");



					positionStringZ++;

					if(code.substr(positionStringZ,1) == " ")
					{
						positionStringZ++;
					}
					std::size_t positionSpace = code.find(" ",positionStringZ);

					std::string strCoordinateZ;

					if(positionSpace != std::string::npos)
					{
						strCoordinateZ = code.substr(positionStringZ, positionSpace-positionStringZ);
					}
					else
					{
						strCoordinateZ = code.substr(positionStringZ);
					}
					//std::cout << "We have found the coordinate Z ("<<strCoordinateZ<<")" << std::endl;
					DeviceCenter::GetInstance()->updateCoordinateZ(serialPortPath_,std::stod(strCoordinateZ));
				}
				if (coordinateE)
				{
					std::size_t positionStringE = code.find("E");
					positionStringE++;

					if(code.substr(positionStringE,1) == " ")
					{
						positionStringE++;
					}
					std::size_t positionSpace = code.find(" ",positionStringE);

					std::string strCoordinateE;

					if(positionSpace != std::string::npos)
					{
						strCoordinateE = code.substr(positionStringE, positionSpace-positionStringE);
					}
					else
					{
						strCoordinateE = code.substr(positionStringE);
					}
					//std::cout << "We have found the coordinate E in gcode command ("<<strCoordinateE<<")" << std::endl;
					DeviceCenter::GetInstance()->updateCoordinateE(serialPortPath_,std::stod(strCoordinateE));
				}


				// Speed
				if (extrusion)
				{
					if (speedF)
					{
						std::size_t positionStringF = code.find("F");
						positionStringF++;

						if(code.substr(positionStringF,1) == " ")
						{
							positionStringF++;
						}
						std::size_t positionSpace = code.find(" ",positionStringF);

						std::string strCoordinateF;

						if(positionSpace != std::string::npos)
						{
							strCoordinateF = code.substr(positionStringF, positionSpace-positionStringF);
						}
						else
						{
							strCoordinateF = code.substr(positionStringF);
						}
						int speed = (std::stoi(strCoordinateF))/60;
						//std::cout << "Print speed: " << speed << std::endl;
						DeviceCenter::GetInstance()->updateSpeed(serialPortPath_,speed);
					}
				}



			}

			if (typeType)
			{
				std::size_t positionStringType = code.find(";TYPE:");
				positionStringType+=6;
				std::size_t positionSpace = code.find(" ",positionStringType);

				std::string strType;

				if(positionSpace != std::string::npos)
				{
					strType = code.substr(positionStringType, positionSpace-positionStringType);
				}
				else
				{
					strType = code.substr(positionStringType);
				}
				//std::cout << "Currently printing: " << strType << std::endl;
				DeviceCenter::GetInstance()->updateCurrentlyPrinting(serialPortPath_,strType);
			}

		}

	catch(...) //or catch(...) to catch all exceptions

			{
				Logger::GetInstance()->logMessage("String exception caught",2,0);
			}

			return (isComment || isEmpty);
}


bool
MarlinDriver::startPrint(bool printing, STATE state) {

	STATE s = getState();
	if (!isPrinterOnline()) {
		return false;
	}

	if (s != PRINTING && s != HEATING && s != STOPPING && s != PAUSED)
	{
		Logger::GetInstance()->logMessage("Reseting print",1,0);
		resetPrint();
	}


	if(printing) setState(PRINTING);
	Logger::GetInstance()->logMessage("Start printing",1,0);
	DeviceCenter::GetInstance()->milisecUpdate = 2500;
	sendNextMessage();
	return true;
}

bool MarlinDriver::resetPrint() {
	if (!isPrinterOnline()) {
		return false;
	}

	paused_=false;
	if(state!=STOPPING)
		setState(ONLINE);
	gcodeBuffer_.setCurrentLine(0);
	gcodeBuffer_.clear();

	Logger::GetInstance()->logMessage("Buffer reset.",1,0);
	return true;
}

bool
MarlinDriver::pausePrint(){

	if (!printPaused() && (getState()==PRINTING || getState() == HEATING || getState() == HEATING_BLOCKED) )
			{
			paused_ = true;
			printf("paused = true\n");
			setState(PAUSED);
			return true;
			}
		else
			return false;

}


bool
MarlinDriver::pausePrintForError(std::string key, std::string message)
{
	Logger::GetInstance()->logMessage("Error: Pausing print",1,0);
	DeviceCenter::GetInstance()->printerErrorEvent(serialPortPath_,key,message);

	if (!printPaused() && (getState()==PRINTING || getState() == HEATING || getState() == HEATING_BLOCKED) )
		{
		paused_ = true;
		printf("paused = true\n");
		setState(PAUSED);
		return true;
		}
	else
		return false;
}

bool
MarlinDriver::printPaused(){

	return paused_;
}
bool
MarlinDriver::resumePrint(){

	if (printPaused() && getState() == PAUSED){
			paused_ = false;
			printf("paused = false\n");
			temperatureTimer_.start();

			if(mustReturnToResumePosition)
			{
				rawLineBuffer.push_back("G90");
				rawLineBuffer.push_back("G92 E"+std::to_string(resumeEvalue));
				rawLineBuffer.push_back("G1 F6000 X5 Y5");
				rawLineBuffer.push_back("M402");
				mustReturnToResumePosition=false;
			}

			if(blocked_unit == NONE)
			{
				printResumedAfterPause=false;
				setState(PRINTING);
			}
			else
			{
				temperatureTimer_.start();
				setState(HEATING_BLOCKED);
				checkTemperatureInterval_=200;
			}
			//timer_
			return true;
		}
		else
			return false;

}

bool MarlinDriver::stopPrint(const std::string& endcode) {
	setState(STOPPING);
	checksumControl->reset(false);
	resetPrint();
	setGCode(endcode,false);

	blocked_unit=NONE;
	temperatureCommandNeedsToBeSent=false;
	mustReturnToResumePosition=false;

	STATE s = getState();
	if (!isPrinterOnline()) {
		return false;
	}

	if (s != PRINTING && s != STOPPING && s != HEATING){ resetPrint();}

	sendNextMessage();
	return true;
}


int MarlinDriver::readFromSerial() {
	int rv = serial_.readData();
	return rv;
}

void MarlinDriver::sendCode(const std::string& code) {

	bool success=false;
	printResumedAfterPause=true;
	receivedOkWithoutNewTemperatureCommand=false;
	iterationsWithoutCheckingTemperatureWhileHeating=0;


	if (isConnected()) {

		// Get extra printer status from gcode command
		extraStatusFromGcode(code);

		// Convert code if necessary (virtual heat and wait)
		string convertedCode = replaceTemperatureCommandIfNecessary(code);
		serialCommand=(convertedCode+"\n");

		// Send via serial communication
		success=writeToSerial((convertedCode+"\n"));

		// Serial communication error flag
		if(!success)
		{
			previousMessageSerialError=true;
		}

	}
}


bool
MarlinDriver::writeToSerial(const std::string& code)
{
	communicationLogger->logCommunicationMessage(code,false);
	return serial_.send(code.c_str());
}

void
MarlinDriver::sendMessageFromRawBuffer()
{
	if(rawLineBuffer.size() > 0)
	{
		sendCode(checksumControl->getFormattedMessage(rawLineBuffer[0]));
		rawLineBuffer.erase(rawLineBuffer.begin());
		temperatureTimer_.start();
	}
}

int
MarlinDriver::extractMessageSequenceNumberFromCode(string code)
{
	int sequenceNumber = -1;

	try{
		std::size_t positionSpace = code.find(" ",0);
		std::string resendNumberString;

		if(positionSpace != std::string::npos)
		{
			resendNumberString = code.substr(1, positionSpace-1);
		}
		else
		{
			resendNumberString = code.substr(1);
		}

		sequenceNumber = stoi(resendNumberString);

	}

	catch(...)
	{
		Logger::GetInstance()->logMessage("Exception caught extracting message line number",1,0);
		sequenceNumber=-1;
	}

	return sequenceNumber;

}

void
MarlinDriver::parseResponseFromPrinter(string& code, bool* echo) {


	// Read extruder JAM error
	bool extruderJam = (code.find("RequestPause:Extruder Jam Detected") != string::npos);

	// Read Repetier EEPROM settings for X and Y offset
	bool eepromType2ValueFound = (code.find("EPR:2") != string::npos);

	// Bed sensor error
	bool bedSensorError = (code.find("heated bed: temp sensor defect marked defect") != string::npos);

	// Extruder sensor error
	bool extruderSensorError = (code.find("Other:: temp sensor defect marked defect") != string::npos);

	// Extruder heater error
	bool extruderHeaterError = (code.find("Error:One heater seems decoupled from thermistor - disabling all for safety!") != string::npos);

	// Bed leveling error
	bool bedLevelingFailed = (code.find("fatal:G32 leveling failed!") != string::npos) || (code.find("Error:Probing had returned errors - autoleveling canceled") != string::npos);

	// Z sensor error
	bool zSensorError = (code.find("Error:z-probe triggered before starting probing") != string::npos);

	// Read coordinate message
	bool coordinateMessage = ( (code.find("C:") != string::npos) || ( code.find("X:") != string::npos && code.find("Y:") != string::npos && code.find("Z:") != string::npos && code.find("E:") != string::npos ) );

	// Read Repetier firmware data
	bool firmware = ( (code.find("FIRMWARE_NAME:") != string::npos) || code.find("Printed filament:") != string::npos || code.find("Printing time:") != string::npos );

	// Read Wait message
	bool foundWait = (code.find("wait") == 0);

	// Read Skip message
	bool foundSkip = ( (code.find("skip") == 0) || (gence3d && code.find("A:skip") == 0) );

	// Read Temperature message
	bool temperatureMessage = (code.find("ok T:") == 0);

	// Read Temperature message without "ok"
	bool blockHeatingMessage = (code.find("T:") == 0);

	// PATCH: Builder3d wrong formatted coordinate message
	bool BUILDER_PATCH_coordinateMessage = ( (code.find("Count X: ") != string::npos) &&  (code.find("R:") != string::npos) );

	// Builder3d initial communication patch
	if (code.find("Unknown") != string::npos)
	{
		Logger::GetInstance()->logMessage("Echo found",5,0);
		*echo = true;

	}

	// Repetier sends "wait" when printer is IDLE and waiting for next command
	if(foundWait)
	{
		if(state == PAUSED)
		{
			// Send next command in RAW buffer
			if (rawLineBuffer.size() > 0)
			{
				sendMessageFromRawBuffer();
			}
		}
		else
		{
			waitReceived++;
		}
	}
	else
	{
		waitReceived=0;
	}

	if(waitReceived > 2 && state == PRINTING)
	{
		Logger::GetInstance()->logMessage("Three wait's received. Printing next line.",1,0);
		sendNextMessage();
	}


	if(foundSkip)
	{
		skipMessageReceived=true;
	}


	// When repetier reports an error, an event is sent and printer is paused
	if(extruderJam)
	{
		pausePrintForError("extruder_jam","Extruder is jammed");
	}

	if (bedSensorError)
	{
		pausePrintForError("bed_sensor_error","heated bed: temp sensor defect marked defect");
	}

	if (extruderSensorError)
	{
		pausePrintForError("extruder_sensor_error","Other:: temp sensor defect marked defect");
	}
	if (extruderHeaterError)
	{
		pausePrintForError("extruder_heater_error","Error:One heater seems decoupled from thermistor - disabling all for safety");
	}

	if (bedLevelingFailed)
	{
		pausePrintForError("bed_leveling_failed","Error:Probing had returned errors - autoleveling canceled");
	}

	if (zSensorError)
	{
		pausePrintForError("z_sensor_error","Error:z-probe triggered before starting probing");
	}


	if (temperatureMessage || blockHeatingMessage ) {

		// After receiving a temperature message, printer state is set to "online"
		if (state==CONNECTING || state == DISCONNECTED)
			setState(ONLINE);

		// When printer state is online, an event is emitted
		// And firmware information is retrieved
		if (checkConnection_) {
			checkConnection_ = false;
			setState(ONLINE);
			DeviceCenter::GetInstance()->printerOnline(serialPortPath_,baudrate_);
			checksumControl->reset(true);
			checkFirmware();
		}

		// In state "heating_blocked", temperature is retrieved every 200ms
		if (state == HEATING_BLOCKED)
			checkTemperatureInterval_ = 200;

		// In state "printing" temperature is not retrieved with timer
		// Since temperature commands are inserted in the gcode file
		else if (state == PRINTING || state == HEATING || state == STOPPING || state == PAUSED || state == HEATING_BLOCKED)
			checkTemperatureInterval_ = 3000;

		// In state "online" temperature and coordinates are retrieved
		else checkTemperatureInterval_ = 1400; // normal

	}

	// Update printer data
	if(temperatureMessage || blockHeatingMessage || coordinateMessage || BUILDER_PATCH_coordinateMessage || firmware || eepromType2ValueFound)
	{
		updatePrinterStatus(code);
	}

	// Retrieve temperature when a message is received from firmware updating temperature
	if ( (code.find("TargetExtr") != string::npos )|| code.find("TargetBed") != string::npos)/* && state==PRINTING ) */
		{

			if (!( (code.find("TargetExtr0:0") != string::npos) || (code.find("TargetExtr1:0") != string::npos) || (code.find("TargetBed:0") != string::npos) ))
			{
				rawLineBuffer.push_back("M105");
			}

		}

	// Some custom sliced gcodes have format errors
	// It can interfere with some firmwares which stay in a loop asking for the same message non-stop
	// To patch this, that message is replaced for M105 to keep the line number logic
	if (code.find("Error:Format error") != string::npos)
	{
		Logger::GetInstance()->logMessage("Format error. Replace flag activated",3,0);
		formatErrorReceived=true;
	}

	// Confirmation that previous message was received ok
	if (code.find("ok") == 0 || code.find("A:ok") == 0 )
	{
		// Disable flag since message was already replaced at "Resend"
		if(formatErrorReceived) formatErrorReceived=false;

		// 3D Gence Patch: Printer sends "A:ok" instead of "ok"
		if(code.find("A:ok") == 0)
		{
			gence3d=true;
		}

		// When skip message is received, a new message does not need to be sent after receiving "ok"
		if(skipMessageReceived)
		{
			Logger::GetInstance()->logMessage("Skipping message",3,0);
			skipMessageReceived=false;
		}
		else
		{

			// Send next message
			// It can be next gcode line from GcodeBuffer, or the next line in raw buffer, a previously sent (resend) message, or messages following the previous resend messages


			if(blocked_unit != NONE && !temperatureCommandNeedsToBeSent)
			{
				temperatureCommandNeedsToBeSent=true;
			}
			if(state == HEATING)
			{
				setState(PRINTING);
			}

			if(state==STOPPING && rawLineBuffer.size() == 0)
			{
				setState(ONLINE);
			}

			if(state==HEATING_BLOCKED && rawLineBuffer.size()==0)
			{
				Logger::GetInstance()->logMessage("Warning: Received ok without a new command",3,0);
				receivedOkWithoutNewTemperatureCommand=true;
			}

			if(state == PRINTING || state == HEATING || state == STOPPING || rawLineBuffer.size() > 0)
				sendNextMessage();
		}
	}

	else if (code.find("Resend:") != string::npos)
	{
		// Printer asks for a specific line number to be resent
		int resendLine = atoi(strstr(code.c_str(), "Resend:") + 7);

		// When printer is asking for first resend line, checksum control is reseted
		if(resendLine==1)
		{
			checksumControl->reset(true);
		}

		//
		if(checkConnection_)
		{
			sendCode(checksumControl->getOldMessage(resendLine)+'\n');
		}

		else
		{
			if(formatErrorReceived)
			{
				Logger::GetInstance()->logMessage("Resend standard message",3,0);
				resendMessage= checksumControl->getOldMessageFixFormat(resendLine);
				formatErrorReceived=false;
			}
			else
				resendMessage = checksumControl->getOldMessage(resendLine);


			resendReceived=true;
			resendAttempt=0;
			previousMessageSentWasAResend=false;

		}

	}
	if (code.find("Unknown") != string::npos)
	{

		if( !(state == PRINTING || state == HEATING || state == HEATING_BLOCKED || state == STOPPING) )
		{
			numEchos++;

			if(numEchos > 10)
			{
				serial_.clearBuffer();
				Logger::GetInstance()->logMessage("This is probably not the correct baudrate",1,0);
				checkConnection_=true;
				numEchos = 0;
				switchBaudrate();
			}

		}
	}
}


/*
 * When serial reads temperature, it automatically updates printer information in Device Center
 * This function parses the temperature response from the printer
 */
void MarlinDriver::updatePrinterStatus(std::string readData){

	int nothing = 0;
	try
		{
							int i = 0;
							bool extrudersAvailable=true;
							std::size_t posT;
							std::size_t posNoT;
							std::vector<int> extrudersTemperatureVector;
							std::vector<int> extruderTargetVector;

							bool gotIt=false;
							bool gotIt2 = false;
							bool simple = false;
							bool cont = false;

							int secure = 0;

							// FIRMWARE
							/*
								FIRMWARE_NAME:Repetier_0.92.8 - 02 FIRMWARE_URL:https://github.com/repetier/Repetier-Firmware/ PROTOCOL_VERSION:1.0 MACHINE_TYPE:Mendel EXTRUDER_COUNT:2 REPETIER_PROTOCOL:3"
 								"Printed filament:1322.74m Printing time:31 days 21 hours 7 min"
 								"PrinterMode:FFF"
							 */
							bool firmwareName = readData.find("FIRMWARE_NAME:") != std::string::npos ;
							bool firmwareFilament = readData.find("Printed filament:") != std::string::npos ;
							bool firmwareTime = readData.find("Printing time:") != std::string::npos ;

							// NEW: EEPROM
							/*
								EPR:2 331 9214 Extr.2 X-offset [steps]
									   ^
									  X axis type

								EPR:2 335 -61 Extr.2 Y-offset [steps]
									   ^
									  Y axis type


							 */
							bool eepromType2ValueFound = (readData.find("EPR:2") != string::npos);

							if(eepromType2ValueFound)
							{
								// Type position is 6
								std::size_t positionType = 6;
								std::size_t positionSpace;

								positionSpace = readData.find(" ",positionType);
								std::string strType;

								if(positionSpace != std::string::npos)
								{
									strType = readData.substr(positionType, positionSpace-positionType);
								}
								else
								{
									strType = readData.substr(positionType);
								}

								//std::cout << "We have found EEPROM an EEPROM value, string type: ("<<strType<<")" << std::endl;
								if(strType.size() > 0)
								{

									int type = std::stoi(strType);
									//std::cout << "Int type: " << type << std::endl;

									// Value position is 10
									size_t positionValue = 10;

									if(type==331 || type==335)
									{
										positionSpace = readData.find(" ",positionValue);
										std::string strValue;

										if(positionSpace != std::string::npos)
										{
											strValue = readData.substr(positionValue, positionSpace-positionValue);
										}
										else
										{
											strValue = readData.substr(positionValue);
										}

										//std::cout << "We have found EEPROM an EEPROM value, string type: ("<<strValue<<")" << std::endl;


										if(type==331 && strValue.size() > 0)
										{
											int value = std::stoi(strValue);
											//std::cout << "Value X Offset: " << value << std::endl;
											DeviceCenter::GetInstance()->printerInfoEvent(serialPortPath_,"eeprom_x_value",value);

										}

										if(type==335 && strValue.size() > 0)
										{
											int value = std::stoi(strValue);
											//std::cout << "Value Y Offset: " << value << std::endl;
											DeviceCenter::GetInstance()->printerInfoEvent(serialPortPath_,"eeprom_y_value",value);
										}
									}

								}
							}


							// COORDINATES
							//ok C: X:0.00 Y:0.00 Z:0.00 E:0.00
							bool coordinateX =  readData.find("X:") != std::string::npos ;
							bool coordinateY =  readData.find("Y:") != std::string::npos ;
							bool coordinateZ =  readData.find("Z:") != std::string::npos ;
							bool coordinateE =  readData.find("E:") != std::string::npos ;

							// BUILDER UGLY PATCH
							bool BUILDER_PATCH_coordinateMessage = ( (readData.find("Count X: ") != string::npos) &&  (readData.find("R:") != string::npos) );

							bool coordinateMessage = ( (readData.find("C:") != string::npos) || ( readData.find("X:") != string::npos && readData.find("Y:") != string::npos && readData.find("Z:") != string::npos && readData.find("E:") != string::npos ) );


							if(firmwareName)
							{
								std::size_t positionName;
								std::size_t positionSpace;

								positionName = readData.find("FIRMWARE_NAME:");
								positionName+=14;

								positionSpace = readData.find(" ",positionName);
								std::string strName;

								if(positionSpace != std::string::npos)
								{
									strName = readData.substr(positionName, positionSpace-positionName);
								}
								else
								{
									strName = readData.substr(positionName);
								}

								//std::cout << "We have found the Firmware name ("<<strName<<")" << std::endl;
								if(strName.size() > 0)
								{
									//std::cout << "Updating firmware name ("<<strName<<")" << std::endl;
									DeviceCenter::GetInstance()->updateFirmwareName(serialPortPath_,strName);
								}


							}

							if(firmwareFilament)
							{
								std::size_t positionFilament;
								std::size_t positionSpace;

								positionFilament = readData.find("Printed filament:");
								positionFilament+=17;

								positionSpace = readData.find(" ",positionFilament);
								std::string strFilament;

								if(positionSpace != std::string::npos)
								{
									strFilament = readData.substr(positionFilament, positionSpace-positionFilament);
								}
								else
								{
									strFilament = readData.substr(positionFilament);
								}

								//std::cout << "We have found the Firmware Filament ("<<strFilament<<")" << std::endl;
								if(strFilament.size() > 0)
								{
									//std::cout << "Updating firmware Filament ("<<strFilament<<")" << std::endl;
									DeviceCenter::GetInstance()->updateFirmwareFilament(serialPortPath_,strFilament);
								}


							}


							if(firmwareTime)
							{
								std::size_t positionTime;

								positionTime = readData.find("Printing time:");
								positionTime+=14;

								std::string strTime;

									strTime = readData.substr(positionTime);

								//std::cout << "We have found the Firmware Time ("<<strTime<<")" << std::endl;
								if(strTime.size() > 0)
								{
									//std::cout << "Updating firmware Time ("<<strTime<<")" << std::endl;
									DeviceCenter::GetInstance()->updateFirmwareTime(serialPortPath_,strTime);
								}


							}





							//X:0.00Y:0.00Z:60.00E:1.00R:0.00 Count X: 0.00Y:0.00Z:60.00
							if (BUILDER_PATCH_coordinateMessage)
							{

								std::size_t positionStringX;
								std::size_t positionSpace;
								if(state!=PRINTING)
								{
										// X
										positionStringX = readData.find("X:");
										positionStringX+=2;
										positionStringX = readData.find("X:", positionStringX);
										positionStringX+=3;

										std::string strCoordinateX;
										positionSpace = readData.find("Y",positionStringX);

										if(positionSpace != std::string::npos)
										{
											strCoordinateX = readData.substr(positionStringX, positionSpace-positionStringX);
										}
										else
										{
											strCoordinateX = readData.substr(positionStringX);
										}

										if(strCoordinateX.size() > 0)
										{
											//std::cout << "(B)We have found the coordinate X ("<<strCoordinateX<<")" << std::endl;
											DeviceCenter::GetInstance()->updateCoordinateX(serialPortPath_,std::stod(strCoordinateX));
										}



										// Y
										std::size_t positionStringY = readData.find("Y:");
										positionStringY+=2;
										positionStringY = readData.find("Y:", positionStringY);
										positionStringY+=2;

										positionSpace = readData.find("Z",positionStringY);
										std::string strCoordinateY;

										if(positionSpace != std::string::npos)
										{
											strCoordinateY = readData.substr(positionStringY, positionSpace-positionStringY);
										}
										else
										{
											strCoordinateY = readData.substr(positionStringY);
										}

										if(strCoordinateY.size() > 0)
										{
											//std::cout << "(B)We have found the coordinate Y ("<<strCoordinateY<<")" << std::endl;
											DeviceCenter::GetInstance()->updateCoordinateY(serialPortPath_,std::stod(strCoordinateY));
										}


										// Z
										std::size_t positionStringZ = readData.find("Z:");
										positionStringZ+=2;
										positionStringZ = readData.find("Z:", positionStringZ);
										positionStringZ+=2;

										positionSpace = readData.find(" ",positionStringZ);
										std::string strCoordinateZ;

										if(positionSpace != std::string::npos)
										{
											strCoordinateZ = readData.substr(positionStringZ, positionSpace-positionStringZ);
										}
										else
										{
											strCoordinateZ = readData.substr(positionStringZ);
										}

										if(strCoordinateZ.size() > 0)
										{
											//std::cout << "(B)We have found the coordinate Z ("<<strCoordinateZ<<")" << std::endl;
											DeviceCenter::GetInstance()->updateCoordinateZ(serialPortPath_,std::stod(strCoordinateZ));
										}
								}


								//R
								std::size_t positionStringR = readData.find("R:");
								positionStringR+=2;

								positionSpace = readData.find(" ",positionStringR);
								std::string strCoordinateR;

								if(positionSpace != std::string::npos)
								{
									strCoordinateR = readData.substr(positionStringR, positionSpace-positionStringR);
								}
								else
								{
									strCoordinateR = readData.substr(positionStringR);
								}

								//std::cout << "(B)We have found the coordinate R ("<<strCoordinateR<<")" << std::endl;
								if(strCoordinateR.size() > 0)
								{
									//std::cout << "Updating ratio ("<<ratio<<")" << std::endl;
									DeviceCenter::GetInstance()->updateRatio(serialPortPath_,std::stoi(strCoordinateR));
								}
							}

							else{

								if(coordinateMessage)
								{
									if (coordinateX)
									{
										std::size_t positionStringX = readData.find("X:");
										std::string strCoordinateX;

										positionStringX+=2;

										std::size_t positionSpace = readData.find(" ",positionStringX);

										if(positionSpace != std::string::npos)
										{
											strCoordinateX = readData.substr(positionStringX, positionSpace-positionStringX);
										}
										else
										{
											strCoordinateX = readData.substr(positionStringX);
										}

										//std::cout << "We have found the coordinate X ("<<strCoordinateX<<")" << std::endl;
										DeviceCenter::GetInstance()->updateCoordinateX(serialPortPath_,std::stod(strCoordinateX));
									}

									if (coordinateY)
									{
										std::size_t positionStringY = readData.find("Y:");
										std::string strCoordinateY;

										positionStringY+=2;

										std::size_t positionSpace = readData.find(" ",positionStringY);

										if(positionSpace != std::string::npos)
										{
											strCoordinateY = readData.substr(positionStringY, positionSpace-positionStringY);
										}
										else
										{
											strCoordinateY = readData.substr(positionStringY);
										}

										//std::cout << "We have found the coordinate Y ("<<strCoordinateY<<")" << std::endl;
										DeviceCenter::GetInstance()->updateCoordinateY(serialPortPath_,std::stod(strCoordinateY));
									}

									if (coordinateZ)
									{
										std::size_t positionStringZ = readData.find("Z:");
										std::string strCoordinateZ;

										positionStringZ+=2;

										std::size_t positionSpace = readData.find(" ",positionStringZ);

										if(positionSpace != std::string::npos)
										{
											strCoordinateZ = readData.substr(positionStringZ, positionSpace-positionStringZ);
										}
										else
										{
											strCoordinateZ = readData.substr(positionStringZ);
										}

										//std::cout << "We have found the coordinate Z ("<<strCoordinateZ<<")" << std::endl;
										DeviceCenter::GetInstance()->updateCoordinateZ(serialPortPath_,std::stod(strCoordinateZ));
									}

									if (coordinateE)
									{
										std::size_t positionStringE = readData.find("E:");
										std::string strCoordinateE;

										positionStringE+=2;

										std::size_t positionSpace = readData.find(" ",positionStringE);

										if(positionSpace != std::string::npos)
										{
											strCoordinateE = readData.substr(positionStringE, positionSpace-positionStringE);
										}
										else
										{
											strCoordinateE = readData.substr(positionStringE);
										}

										//std::cout << "We have found the coordinate E ("<<strCoordinateE<<")" << std::endl;
										DeviceCenter::GetInstance()->updateCoordinateE(serialPortPath_,std::stod(strCoordinateE));
									}
								}
							}

							while(extrudersAvailable)
							{
								int move = 3;

								// temperature hotend
								std::string extTemp;
								posT = readData.find("T"+std::to_string(i)+":");

								if(posT == std::string::npos)
								{

									if(gotIt)
										{
										extrudersAvailable=false;
										break;
										}
									else
									{
										posT = readData.find("T:");
										bool heatingTemp = readData.find("T:")==0;

										if (posT == std::string::npos)
										{
											extrudersAvailable=false;
											break;
										}
										else
										{
											extrudersAvailable=false;
											move = 2;
											cont=true;

											size_t posB = readData.find("/");
											if(posB == std::string::npos)
											{
												posB = readData.find(" ",posT);
												extTemp = readData.substr(posT+move,posB-posT-move);
												Logger::GetInstance()->logMessage("HEATING TEMP: " + extTemp ,4,0);
												if(state!=STOPPING && heatingTemp)
												{
													Logger::GetInstance()->logMessage("Heating: 4",3,0);
													setState(HEATING);//4
												}
												extrudersTemperatureVector.push_back(std::stoi(extTemp));
												DeviceCenter::GetInstance()->updateExtrudersTemp(serialPortPath_,  extrudersTemperatureVector);
												gotIt = false;
												gotIt2 = false;
												break;
											}


										}
									}
								}
								else{
									cont=true;
								}

								if(cont){
									std::size_t posT2 = readData.find(" ",posT+move);


									if(posT2 != std::string::npos)
									{
										extTemp = readData.substr(posT+move,posT2-posT-move);
										gotIt=true;

									}

									if (gotIt)
									{
										//std::cout << "ext"<<i<<":"<<extTemp<<std::endl;
										extrudersTemperatureVector.push_back(std::stoi(extTemp));



										//Extruder target termperature

										gotIt2=false;
										std::string targetTemp;
										posT = readData.find("/",posT);

										if(posT == std::string::npos)
										{
											extrudersAvailable=false;
										}
										std::size_t posT2 = readData.find(" ",posT+1);

										if(posT2 != std::string::npos)
										{
											targetTemp = readData.substr(posT+1,posT2-posT-1);
											gotIt2=true;

										}
										if (gotIt2){
											extruderTargetVector.push_back(std::stoi(targetTemp));
										}
											//std::cout << "target ext"<<i<<":"<<targetTemp<<std::endl;
									}
								}//cont

								i++;



								secure++;
									if (secure > 10)
									{
										Logger::GetInstance()->logMessage("Endless loop alert: 2",1,0);
										break;
									}
							}
							if(gotIt){
								DeviceCenter::GetInstance()->updateExtrudersTemp(serialPortPath_,  extrudersTemperatureVector);
							}

							if(gotIt2)
							{
								DeviceCenter::GetInstance()->updateExtrudersTargetTemp(serialPortPath_,extruderTargetVector);
							}



							//Bed temperature
							gotIt=false;
							std::string bedTemp;
							posT = readData.find("B:");

							std::vector<int>bedTempVec;
							if(posT!=std::string::npos)
							{
								std::size_t posT2 = readData.find(" ",posT+2);

								if(posT2 != std::string::npos)
								{
									bedTemp = readData.substr(posT+2,posT2-posT-2);
									gotIt=true;

								}
								else
								{
									bedTemp = readData.substr(posT+2);
									gotIt=true;

								}
								if (gotIt)
								{
									bool gotIt2 = false;

									//std::cout << "Bed Temperature:"<<bedTemp<<std::endl;
									bedTempVec.push_back(std::stoi(bedTemp));

									std::string targetBedTemp;
									size_t posAux = posT;
									posAux = readData.find("/",posT);

									if(posAux == std::string::npos)
									{
										extrudersAvailable=false;
										//break;
										//continue;
									}
									else
									{
										posT=posAux;
									}
									std::size_t posT2 = readData.find(" ",posT+1);

									if(posT2 != std::string::npos)
									{
										targetBedTemp = readData.substr(posT+1,posT2-posT-1);
										gotIt2=true;

									}
									if (gotIt2)
									{
										//std::cout << "Target Bed Temperature: "<<targetBedTemp<<std::endl;
										bedTempVec.push_back(std::stoi(targetBedTemp));
									}
								}
							}
							if(gotIt){
								//std::cout << "Updating bed temperature " << bedTempVec.size() << std::endl;
								DeviceCenter::GetInstance()->updateBedTemperature(serialPortPath_, bedTempVec);
							}


							//TIME
							if (state == 5)
							{

								std::string timeNow = currentDateTim();
								//std::cout << "updatingtime: " << timeNow << std::endl;
								DeviceCenter::GetInstance()->updateTimeNow(serialPortPath_, timeNow);
							}
							else
							{
								std::string timeNow = "";
								DeviceCenter::GetInstance()->updateTimeNow(serialPortPath_, timeNow);
							}



							//PROGRESS
							int progress, currentLine, totalLines;
							if (state == 5 || state== 7 || state== 8 || state==10 )
							{



								//std::cout << "Current line: " << getCurrentLine() << std::endl;
								currentLine=getCurrentLine();
								totalLines=DeviceCenter::GetInstance()->GetTotalLines(serialPortPath_);

								if(totalLines > 0)
									progress = 100*currentLine/totalLines;
								else
									{
										progress = 0;
										currentLine=0;
										totalLines=0;
									}

							}

							else
							{
								progress = 0;
								currentLine=0;
								totalLines=0;

							}
							//std::cout << "Printer progress: " << progress << std::endl;

							DeviceCenter::GetInstance()->updateProgress(serialPortPath_, progress);
							DeviceCenter::GetInstance()->updateCurrentLine(serialPortPath_, currentLine);

							//Not used anymore for now. Remove soon.
							//DeviceCenter::GetInstance()->updateTotalLines(serialPortPath_, totalLines);

		}

	catch(...) //or catch(...) to catch all exceptions

		{
			nothing = 0;
		}

}

void MarlinDriver::checkTemperatureOrSendNextMessage() {


}



void MarlinDriver::checkFirmware() {
	sendCode("M115");
}




void MarlinDriver::printGcodeLineFromBuffer()
{
	string bufferLine;

	// Extract line from gcode buffer
	if(gcodeBuffer_.getNextLine(bufferLine)) {
		try
		{
			// Extract extra printer status from gcode line
			// function returns true if line is a comment or is empty
			if(extraStatusFromGcode(bufferLine))
			{
				int secure =0;

				// Remove lines while these are empty or comments
				do
				{
					DeviceCenter::GetInstance()->addBufferedLines(serialPortPath_,-1);
					gcodeBuffer_.eraseLine();
					gcodeBuffer_.getNextLine(bufferLine);
					secure++;
					if (secure > 1000)
					{
						Logger::GetInstance()->logMessage("Finishing print: 3",1,0);

						if(state==PRINTING || state == HEATING){
							Logger::GetInstance()->logMessage("PRINT FINISHED",1,0);
							DeviceCenter::GetInstance()->updateTotalLines(serialPortPath_, 0);
							DeviceCenter::GetInstance()->printFinished(serialPortPath_);
						}
						resetPrint();
						return;

					}
				}
				while(extraStatusFromGcode(bufferLine));

			}
		}

		catch(...)

		{
			int nothing = 0;
			Logger::GetInstance()->logMessage("stoi exception caught",4,0);
		}

		if(DeviceCenter::GetInstance()->getBufferedLines(serialPortPath_) < 50 )
		{
			Logger::GetInstance()->logMessage("Warning: printer ending",4,0);
		}


		// Do not add line number and checksum to previously formatted line
		if(skipFormat)
		{
			sendCode(bufferLine);
			skipFormat=false;
		}

		else
		{

			// Add line number and checksum
			string formattedMessage=checksumControl->getFormattedMessage(bufferLine);

			// If line number is N1, the counter in the printer side needs to be reset with M110 N0
			if(formattedMessage.find("N1 ")==0)
			{

				// And the actual command is saved in raw buffer to be sent right after receiving "ok"
				Logger::GetInstance()->logMessage("Saving command as TUNE",4,0);
				rawLineBuffer.push_back(formattedMessage);
				skipFormat=true;

				sendCode(checksumControl->addChecksum("M110 N0"));
			}

			else {
				Logger::GetInstance()->logMessage("Sending new formatted message",6,0);

				// Send current line
				sendCode(formattedMessage);
				gcodeBuffer_.setCurrentLine(gcodeBuffer_.getCurrentLine() + 1);
			}
		}



	}

	// If GcodeBuffer is empty, print is finished
	else
	{
		Logger::GetInstance()->logMessage("Finishing print: 4 ",1,0);

		if(state==PRINTING || state == HEATING){
			Logger::GetInstance()->logMessage("Finishing print: 4 ",1,0);
			DeviceCenter::GetInstance()->updateTotalLines(serialPortPath_, 0);
			DeviceCenter::GetInstance()->printFinished(serialPortPath_);
		}

		resetPrint();
	}
}

void MarlinDriver::sendNextMessage() {


	// If a resend message is ready to be sent
	if(resendReceived)
	{

		resendCounter++;
		if(resendCounter>1000)
		{
			resendCounter=0;
			resendAttempt=0;
			previousMessageSentWasAResend=false;
			sendNextMessage();
		}

		else
		{
			resendAttempt=0;
			previousMessageSentWasAResend=true;
			sendCode(resendMessage);
		}
		resendReceived=false;

	}

	// If previus message sent was a resend, the next message needs to be the following (previously sent) message number N+1
	else if(previousMessageSentWasAResend && resendMessage.find("N")==0)
	{
		int resendNumber = extractMessageSequenceNumberFromCode(resendMessage);

		if (resendNumber >= 0)
		{
			if(checksumControl->currentPosition > resendNumber)
			{
				resendMessage=checksumControl->getOldMessage(resendNumber+1);
				sendCode(resendMessage);
			}

			// Unless there are no more resend messages in the buffer
			else
			{
				previousMessageSentWasAResend=false;
				sendNextMessage();
			}

		}

		else
		{
			previousMessageSentWasAResend=false;
			sendNextMessage();
		}

	}

	// If no resend message needs to be sent
	else
	{
		resendCounter=0;

		// Raw buffer needs to be empty
		if (rawLineBuffer.size() > 0)
		{
			sendMessageFromRawBuffer();
		}

		// Then, the next GCodeBuffer line is printed
		else if(state == PRINTING || state == HEATING || state == STOPPING )
		{
			DeviceCenter::GetInstance()->addBufferedLines(serialPortPath_,-1);
			gcodeBuffer_.eraseLine();
			printGcodeLineFromBuffer();
		}
		else if (state == ONLINE || state == PAUSED || (state == HEATING_BLOCKED && temperatureCommandNeedsToBeSent) )
		{
			// When state is heating_blocked, temperature is always retrieved
			if(temperatureOrCoordinate || state==HEATING_BLOCKED)
			{
				if(state==HEATING_BLOCKED)
				{
					if (rawLineBuffer.size() == 0)
					{
						Logger::GetInstance()->logMessage("Pushing temperature line to raw buffer: 1",4,0);
						rawLineBuffer.push_back("M105");
					}
					else
					{
						if(receivedOkWithoutNewTemperatureCommand)
						{
							iterationsWithoutCheckingTemperatureWhileHeating++;
							if(iterationsWithoutCheckingTemperatureWhileHeating > maxIterationsWithoutCheckingTemperatureWhileHeating)
							{
								Logger::GetInstance()->logMessage("Warning: Temperature checking timeout",3,0);
								sendCode(checksumControl->getFormattedMessage("M105"));
							}
						}

					}
				}
				else
				{
					sendCode(checksumControl->getFormattedMessage("M105"));
				}

				temperatureOrCoordinate=false;
			}

			// Otherwise, coordinates are also retrieved
			else
			{
				sendCode(checksumControl->getFormattedMessage("M114"));
				temperatureOrCoordinate=true;
			}
		}
		else
		{
			int lines =DeviceCenter::GetInstance()->GetTotalLines(serialPortPath_);

			if (checkConnection_)
			{
				sendCode("M105\n");
			}
			else if( (state !=PRINTING && lines == 0) || state==STOPPING )
			{

				Logger::GetInstance()->logMessage("Asking for temperature",3,0);
				sendCode(checksumControl->getFormattedMessage("M105"));
			}
		}

	}
}
