/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */

#include <node.h>
#include <node_object_wrap.h>
#include <nan.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <map>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <fstream>
#include "driver/utilities/utilities.h"
#include "FormiThread.hpp"
#include "utils/Logger/Logger.h"
#include "utils/find_devices.hpp"

using std::string;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;

using namespace v8;
using namespace node;

struct ListBaton {
	public:
		Nan::Callback* callback;
};

std::vector<std::string> connectedDevices;
std::vector<bool> deviceConnected;

void GetPrinterList (const FunctionCallbackInfo<Value>& args){


	// Initialize NodeJS variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	Local<Array> arrayOBJ;
	arrayOBJ.Clear();
	arrayOBJ = Array::New(isolate);
	Local<Object> errorOBJ = Object::New(isolate);

	Local<Function> cb = Local<Function>::Cast(args[0]);


	// Get printer list
	std::vector<std::string> result = DeviceCenter::GetInstance()->getPrinterList();

	// Get connected printer list
	std::vector<bool> connected = DeviceCenter::GetInstance()->getConnected();


	int a = 0;
	int positionArray = 0;

	// Create array with connected printers
	for (std::vector<string>::iterator it = result.begin(); it != result.end(); it++)
	{
		if (connected[a])
		{
			arrayOBJ->Set(positionArray,String::NewFromUtf8(isolate,result[a].c_str()));
			positionArray++;
		}
		a++;

	}

	// Prepare callback
	const unsigned numberOfArguments = 2;
	Local<Value> callbackArguments[numberOfArguments] = {v8::Null(isolate), arrayOBJ };

	// Callback
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);

}

// Printer status function
void GetPrinterInfo (const FunctionCallbackInfo<Value>& args){


	// Define NodeJS Variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);
	Local<Function> cb = Local<Function>::Cast(args[1]);

	// Parse printerID parameter
	String::Utf8Value v8String(args[0]->ToString());
	std::string printerID = std::string(*v8String);

	// Declare callback variables
	const unsigned numberOfArguments = 2;
	Local<Value> callbackArguments[numberOfArguments] = {errOBJ, resultOBJ };


	std::string jsonResult;

	// If specified printer is connected
	if (DeviceCenter::GetInstance()->isConnected(printerID))
	{
		bool gotit = false;

		// Retrieve printer data
		PrinterData* data = DeviceCenter::GetInstance()->getPrinterInfo(printerID, &gotit);

		// No printer data
		if (data == NULL){
			errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,601));
			errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer not found"));
			errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
			callbackArguments[1] = v8::Null(isolate);
		}


		if(gotit)
		{
			int a = 0;
			Local<Array>arrayExtruders = Array::New(isolate); // NodeJS array

			// Add an object for each extruder in an array
			for (std::vector<int>::iterator it = data->extruderTemperature.begin(); it!=data->extruderTemperature.end(); it++)
			{

				Local<Object> extrObject = Object::New(isolate);
				extrObject->Set(String::NewFromUtf8(isolate, "id"), Integer::New(isolate, a));
				extrObject->Set(String::NewFromUtf8(isolate, "temp"), Integer::New(isolate, data->extruderTemperature[a]));
				extrObject->Set(String::NewFromUtf8(isolate, "targetTemp"), Integer::New(isolate, data->extruderTargetTemperature[a]));

				arrayExtruders->Set(a,extrObject);
				a++;
			}
			// Add extruder array to result object
			resultOBJ->Set(String::NewFromUtf8(isolate, "extruders"), arrayExtruders);

			// Add a bed object to result object
			Local<Object> bedObject = Object::New(isolate);
			bedObject->Set(String::NewFromUtf8(isolate, "temp"), Integer::New(isolate, data->bedTemperature));
			bedObject->Set(String::NewFromUtf8(isolate, "targetTemp"), Integer::New(isolate, data->targetBedTemperature));
			resultOBJ->Set(String::NewFromUtf8(isolate, "bed"), bedObject);

			//port
			resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, data->serialPort.c_str()));
			//state
			resultOBJ->Set(String::NewFromUtf8(isolate, "status"), String::NewFromUtf8(isolate, data->state.c_str()));
			//timeStarted
			resultOBJ->Set(String::NewFromUtf8(isolate, "timeStarted"), String::NewFromUtf8(isolate, data->timeStarted.c_str()));
			//timeNow
			resultOBJ->Set(String::NewFromUtf8(isolate, "timeNow"), String::NewFromUtf8(isolate, data->timeNow.c_str()));
			//baudrate
			resultOBJ->Set(String::NewFromUtf8(isolate, "baudrate"), Integer::New(isolate, data->driver->getBaudrate()));
			//printjobID
			resultOBJ->Set(String::NewFromUtf8(isolate, "queueItemId"), Integer::New(isolate, data->printjobID));
			//progress
			resultOBJ->Set(String::NewFromUtf8(isolate, "progress"), Integer::New(isolate, data->progress));
			//currentLine
			resultOBJ->Set(String::NewFromUtf8(isolate, "currentLine"), Integer::New(isolate, data->currentLine));
			//totalLines
			resultOBJ->Set(String::NewFromUtf8(isolate, "totalLines"), Integer::New(isolate, data->totalLines));
			//printingTime
			resultOBJ->Set(String::NewFromUtf8(isolate, "printingTime"), Integer::New(isolate, data->printingTime));
			//remainingPrintingTime
			resultOBJ->Set(String::NewFromUtf8(isolate, "remainingPrintingTime"), Integer::New(isolate, data->remainingPrintingTime));
			//ratio
			resultOBJ->Set(String::NewFromUtf8(isolate, "ratio"), Integer::New(isolate, data->ratio));

			callbackArguments[0]=v8::Null(isolate);

			// Coordinates object
			Local<Object> coordinatesObject = Object::New(isolate);
			coordinatesObject->Set(String::NewFromUtf8(isolate, "x"), Number::New(isolate, data->coordinateX));
			coordinatesObject->Set(String::NewFromUtf8(isolate, "y"), Number::New(isolate, data->coordinateY));
			coordinatesObject->Set(String::NewFromUtf8(isolate, "z"), Number::New(isolate, data->coordinateZ));
			resultOBJ->Set(String::NewFromUtf8(isolate, "coordinates"), coordinatesObject);


			// Firmware (Repetier) object
			Local<Object> firmwareObject = Object::New(isolate);
			firmwareObject->Set(String::NewFromUtf8(isolate, "name"), String::NewFromUtf8(isolate, data->firmwareName.c_str()));
			firmwareObject->Set(String::NewFromUtf8(isolate, "filament"), String::NewFromUtf8(isolate, data->firmwareFilament.c_str()));
			firmwareObject->Set(String::NewFromUtf8(isolate, "printingTime"), String::NewFromUtf8(isolate, data->firmwareTime.c_str()));
			resultOBJ->Set(String::NewFromUtf8(isolate, "firmware"), firmwareObject);


			// printing status override (extracted from gcode comments)
			if(data->state == "printing")
			{
				//Type
				resultOBJ->Set(String::NewFromUtf8(isolate, "currentlyPrinting"), String::NewFromUtf8(isolate, data->currentlyPrinting.c_str()));
				//Layer
				resultOBJ->Set(String::NewFromUtf8(isolate, "currentLayer"), Integer::New(isolate, data->currentLayer));
				//Speed
				resultOBJ->Set(String::NewFromUtf8(isolate, "printSpeed"), Integer::New(isolate, data->speed));
				//Material
				resultOBJ->Set(String::NewFromUtf8(isolate, "materialAmount"), Integer::New(isolate, data->coordinateE));
			}


			// Multipliers
			resultOBJ->Set(String::NewFromUtf8(isolate, "fanSpeed"), Integer::New(isolate, data->fanSpeed));
			resultOBJ->Set(String::NewFromUtf8(isolate, "flowRate"), Integer::New(isolate, data->flowRate));
			resultOBJ->Set(String::NewFromUtf8(isolate, "speedMultiplier"), Integer::New(isolate, data->speedMultiplier));


		}
	}

	// Error object
	else
	{
	errOBJ->Set(String::NewFromUtf8(isolate, "code"),Integer::New(isolate,602));
	errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"printer not connected or not found"));
	errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
	callbackArguments[1] = v8::Null(isolate);
	}


	// Calls callback
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);

}

// Name: Send Command
// Purpose: Send raw gcode command to printer
// Arguments: COMMAND, PRINTERID, CALLBACK
void SendCommand(const FunctionCallbackInfo<Value>& args) {

	// Declare NodeJS Variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// Declare Callback variables
	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);
	Local<Function> cb = Local<Function>::Cast(args[2]);

	// Parse GCODE Command from arguments
	String::Utf8Value v8String(args[0]->ToString());
	std::string RAWCOMMAND = std::string(*v8String);

	// Parse Printer ID from arguments
	String::Utf8Value v8String1(args[1]->ToString());
	std::string PRINTERID = std::string(*v8String1);

	// Declare Callback arguments
	Local<Value> callbackArguments[2] = { errOBJ, resultOBJ };

	Logger::GetInstance()->logMessage("Sending raw command",2,0);

	if (DeviceCenter::GetInstance()->isConnected(PRINTERID))
	{

		try
		{
			// Retrieve driver from Device Center
			MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(PRINTERID);
			if(driver->isConnected())
			{
				// Prevent thread from use shared variables
				while(Blocker::GetInstance()->isBlockedM(PRINTERID) != 0)
				{
					usleep(100000);
				}
				Blocker::GetInstance()->blockM(PRINTERID,false);


				int state = driver->getState();

				// If state = disconnected
				if (state == 0 || state == 1)
				{
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,401));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not online"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);

				}

				// If state = connecting
				else if (state == 2)
				{
					Logger::GetInstance()->logMessage("Can not send commands when printer is still connecting",1,0);
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,402));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not ready yet"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);
				}
				// If state = online
				else if (state == 3)
				{

					// Make multiple commands separating by \n
					size_t pos0 = 0;
					size_t posT = (RAWCOMMAND.find("\n",pos0)) +1;
					std::string gcodeChunk;

					Logger::GetInstance()->logMessage("Appending gcode",4,0);
					int count =0;
					while (posT != std::string::npos)
					{
						gcodeChunk = RAWCOMMAND.substr(pos0,posT-pos0-1);
						pos0 = posT;
						Logger::GetInstance()->logMessage("Appending: ",5,0);
						Logger::GetInstance()->logMessage(gcodeChunk,5,0);


						// Add each single command in raw line buffer
						driver->pushRawCommand(gcodeChunk);
						count++;

						posT = (RAWCOMMAND.find("\n",posT));
						if (posT != std::string::npos)
						{
							posT+=1;
						}
					}



					// Create result object (for callback)
					resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
					resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[0] = v8::Null(isolate);

				}

				// if printer is printing gcode commands are not sent. Use sendTuneCommand instead.
				else if (state > 3){
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,404));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is busy"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);
				}

				Blocker::GetInstance()->unblockM(PRINTERID);
			} //if

			// Create error object for callback
			else
			{
				errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,405));
				errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Serial port is not open"));
				errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
				callbackArguments[1] = v8::Null(isolate);
			}


		}
		catch (const std::exception& e)
		{
			 Logger::GetInstance()->logMessage("An exception occurred",1,0);
			 std::cout << e.what() << std::endl;
		}


	}

	// Create error object for callback
	else
	{
		errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,406));
		errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not online"));
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
		callbackArguments[1] = v8::Null(isolate);
	}

	// Calls callback
	const unsigned numberOfArguments = 2;
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);

}

// Name: Send Tune Command
// Purpose: Send raw gcode command to printer. It can be used even when printer is printing. Commands are queued in raw buffer
// Arguments: COMMAND, PRINTERID, CALLBACK
void SendTuneCommand(const FunctionCallbackInfo<Value>& args) {

	// Declare NodeJS variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// Declare Callback variables
	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);
	Local<Function> cb = Local<Function>::Cast(args[2]);

	// Parse command from arguments
	String::Utf8Value v8String(args[0]->ToString());
	std::string RAWCOMMAND = std::string(*v8String);

	// Parse printer ID from arguments
	String::Utf8Value v8String1(args[1]->ToString());
	std::string PRINTERID = std::string(*v8String1);

	// Declare callback arguments
	Local<Value> callbackArguments[2] = { errOBJ, resultOBJ };

	Logger::GetInstance()->logMessage("Sending TUNE command",2,0);

	if (DeviceCenter::GetInstance()->isConnected(PRINTERID))
	{
		try
		{
			MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(PRINTERID);

			if(driver->isConnected())
			{
				while(Blocker::GetInstance()->isBlockedM(PRINTERID) != 0)
				{
					usleep(100000);
				}

				Blocker::GetInstance()->blockM(PRINTERID,false);
				int state = driver->getState();
				if (state == 0 || state == 1)
				{
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,401));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not online"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);

				}
				else if (state == 2)
				{
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,402));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not ready yet"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);
				}
				else if (state >= 3) //WHICH MEANS IT'S CONNECTED AND IDLE or PRITING OR PAUSED OR WHATEVER (TUNE)
				{

					// Make multiple commands, dividing with \n
					size_t pos0 = 0;
					size_t posT = (RAWCOMMAND.find("\n",pos0)) +1;
					std::string gcodeChunk;
					std::string callback;

					Logger::GetInstance()->logMessage("Appending gcode",4,0);
					int count =0;
					while (posT != std::string::npos)
					{
						gcodeChunk = RAWCOMMAND.substr(pos0,posT-pos0-1);

						pos0 = posT;

						Logger::GetInstance()->logMessage("Appending: ",4,0);
						Logger::GetInstance()->logMessage(gcodeChunk,4,0);

						driver->pushRawCommand(gcodeChunk);
						count++;

						driver->extraStatusFromGcode(gcodeChunk);

						posT = (RAWCOMMAND.find("\n",posT));
						if (posT != std::string::npos)
						{
							posT+=1;
						}
					}

					// Create result object for callback
					resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
					resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[0] = v8::Null(isolate);

				}
				Blocker::GetInstance()->unblockM(PRINTERID);

			} //if

			// Create error object for callback
			else
			{
				errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,405));
				errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Serial port is not open"));
				errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
				callbackArguments[1] = v8::Null(isolate);
			}
		}
			 catch (const std::exception& e)
			  {
				 Logger::GetInstance()->logMessage("An exception occurred",1,0);
				 std::cout << e.what() << std::endl;
			  }
	}

	// Create error object for callback
	else
	{
		errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,406));
		errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not online"));
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
		callbackArguments[1] = v8::Null(isolate);
	}

	//Return callback
	const unsigned numberOfArguments = 2;
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);

}

// Name: Print File
// Purpose: Prints a GCODE file from file system
// Arguments: FILENAME, PRINTJOBID, PRINTERID, CALLBACK
void PrintFile(const FunctionCallbackInfo<Value>& args) {


	// Declare NodeJS Variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// Declare callback arguments
	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);
	Local<Function> cb = Local<Function>::Cast(args[3]);
	const unsigned numberOfArguments = 2;
	Local<Value> callbackArguments[2] = { errOBJ, resultOBJ };

	// Parse file name from arguments
	String::Utf8Value v8String(args[0]->ToString());
	std::string FILENAME = std::string(*v8String);


	Logger::GetInstance()->logMessage("File to print: "+FILENAME,1,0);

	// Parse printer ID from arguments
	String::Utf8Value v8String1(args[2]->ToString());
	std::string PRINTERID = std::string(*v8String1);

	// Parse print job ID from arguments
	String::Utf8Value v8String2(args[1]->ToString());
	std::string PRINTJOBID = std::string(*v8String2);


	//Check if printer is connected
	bool connected = DeviceCenter::GetInstance()->isConnected(PRINTERID);
	int printjobID = atoi(PRINTJOBID.c_str());

	Logger::GetInstance()->logMessage("queueItemId/printJobId: "+std::to_string(printjobID),2,0);

	if(connected)
	{
		if (!isAbsolutePath(FILENAME.c_str()))
		{
			Logger::GetInstance()->logMessage("Please supply an absolute path for the file",1,0);
			errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,508));
			errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"File path is not absolute"));
			errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
			callbackArguments[1] = v8::Null(isolate);
		}
		try
		{
			MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(PRINTERID);
			Logger::GetInstance()->logMessage("The state of the printer is: " + std::to_string(driver->getState()),5,0);

			if(driver->isConnected())
			{
				int state = driver->getState();

				// Disconnected
				if (state == 0 || state == 1)
				{
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,501));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not online"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);
				}

				// Connecting
				else if (state == 2)
				{
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,502));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not ready yet"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);
				}

				// Online
				else if (state == 3)
				{
					// Clear GCODE
					Logger::GetInstance()->logMessage("Clear gcode",1,0);
					driver->clearGCode();
					int fileSize;

					// Read file contents
					Logger::GetInstance()->logMessage("Read file contents",1,0);

					// Prepare thread data
					thread_data* threadData = new thread_data();
					threadData->serialDevice = PRINTERID;
					threadData->gcodeFile = FILENAME;

					// Reset buffers
					driver->clearGCode();
					DeviceCenter::GetInstance()->resetBuffer(PRINTERID);


					DeviceCenter::GetInstance()->getDriverFromPrinter(PRINTERID)->setStatePrinting();
					// Check if file exists
					if (!std::ifstream(FILENAME))
					{
						Logger::GetInstance()->logMessage("File does NOT exist",1,0);
						errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,507));
						errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"File not found"));
						errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
						callbackArguments[1] = v8::Null(isolate);
						cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);
						DeviceCenter::GetInstance()->getDriverFromPrinter(PRINTERID)->setStateOnline();
						return;
					}

					// Create threads

					// Count the total lines that there are in a gcode
					pthread_t linesThread;
					pthread_create(&linesThread, NULL, &FormiThread::countLines, (void*)threadData);

					// Calculate the printing time based on speeds and times
					pthread_t printingTimeThread;
					pthread_create(&printingTimeThread, NULL, &FormiThread::calculatePrintingTime, (void*)threadData);

					// Append the gcode lines and temperature commands in a separated thread to start print earlier
					pthread_t driverThread;
					pthread_create(&driverThread, NULL, &FormiThread::appendGcode, (void*)threadData);



					//While buffer is empty, print is not started
					int bufferCounter=0;
					while (driver->getTotalLines() < 1)
					{
						Logger::GetInstance()->logMessage("Buffer is still empty",1,0);
						bufferCounter++;

						// If buffer is not filled (could be that file was empty)
						if(bufferCounter>5)
						{
							// Create error object
							Logger::GetInstance()->logMessage("Driver could not print file (buffer forever empty)",1,0);
							errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,509));
							errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Could not print file (buffer empty)"));
							errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
							callbackArguments[1] = v8::Null(isolate);

							// Set printer state back to "online"
							DeviceCenter::GetInstance()->getDriverFromPrinter(PRINTERID)->setStateOnline();

							//Send callback to Node application
							Logger::GetInstance()->logMessage("Sending callback",5,0);
							cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);
							return;
						}
						sleep(1);
					}

					// Start print
					Logger::GetInstance()->logMessage("Sending start signal",1,0);
					if(driver->startPrint(true))
					{
						Logger::GetInstance()->logMessage("Printing",1,0);
						DeviceCenter::GetInstance()->updatePrintJobID(PRINTERID, printjobID);
						resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
						resultOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"File sent and printing"));
						resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
						callbackArguments[0] = v8::Null(isolate);

					}
					// Create error object if print is not started
					else
					{
						Logger::GetInstance()->logMessage("Driver could not print file",1,0);
						errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,503));
						errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Could not print file"));
						errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
						callbackArguments[1] = v8::Null(isolate);
					}

				}

				// Create error object if printer is busy
				else if (state > 3)
				{
					Logger::GetInstance()->logMessage("Printer is printing already",1,0);
					errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,504));
					errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is busy"));
					errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
					callbackArguments[1] = v8::Null(isolate);
				}
			}

			// Create error object if printer is not connected
			else
			{
				Logger::GetInstance()->logMessage("Printer is not connected",1,0);
				errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,505));
				errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Serial port is not open"));
				errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
				callbackArguments[1] = v8::Null(isolate);
			}

		}// Try

		catch (const std::exception& e)
		{
			Logger::GetInstance()->logMessage("An exception occurred",1,0);
			std::cout << e.what() << std::endl;
		}
	}

	else
	{
		Logger::GetInstance()->logMessage("Printer is not online",1,0);
		errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,506));
		errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Printer is not online"));
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,PRINTERID.c_str()));
		callbackArguments[1] = v8::Null(isolate);
	}

	// Calls callback
	Logger::GetInstance()->logMessage("Sending callback",5,0);
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);

}

// Function that runs in separated thread when uv_queue_work is called
// uv_queue_work(uv_default_loop(), req, preRunDriver,(uv_after_work_cb) runDriver);
// This function makes it possible to ask for the printer status every 0.1 sec, without having to freeze the application
void preRunDriver(uv_work_t* req){

	int slp = 200000;

	if (DeviceCenter::GetInstance()->milisecUpdate > 0)
	{
		slp = DeviceCenter::GetInstance()->milisecUpdate;
	}
	usleep(slp);
}

// After calling:
// uv_queue_work(uv_default_loop(), req, preRunDriver,(uv_after_work_cb) runDriver);
// This function is run after the function "preRunDriver" returns, and it is run in the main thread,
// which means inside this function we can use the (isolated) callbacks or we can emit events safely to the Node application (since it is run in the same islated environment / same thread)
void runDriver(uv_work_t* req, int status){

	string newDevice;

	// Initialize callback variables
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);
	if(!isolate)
	{
		isolate = v8::Isolate::New();
		isolate->Enter();
	}
	ListBaton* data = static_cast<ListBaton*>(req->data);

	v8::Local<v8::Value> argv[3];
	argv[0]= v8::Undefined(isolate);
	argv[1]= v8::Undefined(isolate);
	argv[2]= v8::Undefined(isolate);

	Local<Object> printerOBJ;
	printerOBJ.Clear();
	printerOBJ = Object::New(isolate);


	// Printer list variables
	bool alreadyConnected = true;
	std::string serialDevice = "";

	char **devlist;
	devlist = printr_find_devices();


	// There is no list
	if (!devlist)
	{
		cerr << "Error getting device list(" << strerror(errno) << ")." << endl;
		exit(1);
	}

	// If list is empty
	if (!devlist[0])
	{
		int a = 0;
		for (std::vector<std::string>::iterator it = connectedDevices.begin(); it != connectedDevices.end() ; it++)
		{
			// If list is empty --> Disconnect every connected device
			if (deviceConnected[a])
			{
				newDevice = "/dev/"+connectedDevices[a];
				Logger::GetInstance()->logMessage("Sending event: printer disconnected",1,0);
				printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerDisconnected"));
				printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, newDevice.c_str()));

				argv[2] = printerOBJ;

				// Disconnect printer
				DeviceCenter::GetInstance()->updateState(newDevice,"disconnected");

				data->callback->Call(3,argv);
				deviceConnected[a]=false;
			}
			a++;
		}

		DeviceCenter::GetInstance()->updateConnected(deviceConnected, true);
	}

	// If there is more than one device
	else if (devlist[1] != 0)
	{
		// If there are previously connected devices
		int b = 0;
		if(!connectedDevices.empty())
		{
			for (std::vector<std::string>::iterator it = connectedDevices.begin(); it != connectedDevices.end() ; it++)
			{

				// Check if they are still connected
				bool mustKeepConnected = false;
				for (int i = 0; devlist[i] !=0; i++)
				{
					if (connectedDevices[b] == devlist[i])
					{
						mustKeepConnected = true;
					}
				}

				// In case they are disconnected. An event needs to be sent
				if (!mustKeepConnected && deviceConnected[b])
				{
					newDevice = "/dev/"+connectedDevices[b];

					// Disconnect printer
					DeviceCenter::GetInstance()->updateState(newDevice,"disconnected");

					Logger::GetInstance()->logMessage("Sending event: printer disconnected",1,0);
					printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerDisconnected"));
					printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, newDevice.c_str()));

					argv[2] = printerOBJ;

					data->callback->Call(3,argv);
					deviceConnected[b]=false;
				}
				b++;
			}
		}

		//For each one of the multiple devices detected
		for (int i = 0; devlist[i] !=0; i++)
		{

			// If list is empty, create a new driver for each one of them
			if (connectedDevices.empty()) //first time
			{
				serialDevice = devlist[i];
				newDevice = "/dev/"+ serialDevice;

				// Do not send event printer connected for ttyS1, since it could not be a printer
				// Note: In case it's a 3D Printer, event is sent later. See MarlinDriver.cpp
				bool sendEvent = serialDevice!="ttyS1";
				DeviceCenter::GetInstance()->newDriverForPrinter(newDevice, sendEvent);

				connectedDevices.push_back(serialDevice);
				deviceConnected.push_back(true);
			}

			// If there are previously connected devices
			else
			{
				int c = 0;
				bool connec=false;

				// Check each one of them
				for (std::vector<std::string>::iterator it = connectedDevices.begin(); it != connectedDevices.end() ; it++)
				{
					// If it matches
					if (connectedDevices[c] == devlist[i])
					{
						connec = true;
						if (deviceConnected[c])
						{
							// Printer was already connected
						}


						// Printer was re-connected
						else
						{
							Logger::GetInstance()->logMessage("Reconnecting printer",1,0);
							newDevice = "/dev/"+connectedDevices[c];
							//argv[2]= v8::String::NewFromUtf8(isolate,"PRINTER CONNECTED[EVENT]");
							printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerConnected"));
							printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, newDevice.c_str()));

							argv[2] = printerOBJ;

							data->callback->Call(3,argv);
							DeviceCenter::GetInstance()->reconnectPrinter(c);

							deviceConnected[c]=true;
						}
					}
					c++;
				}

				// If printer was not connected before, a new driver is created
				if (!connec)
				{
					serialDevice = devlist[i];
					newDevice = "/dev/"+ serialDevice;

					Logger::GetInstance()->logMessage("New printer +++",1,0);
					bool sendEvent = serialDevice!="ttyS1";
					DeviceCenter::GetInstance()->newDriverForPrinter(newDevice,sendEvent);

					connectedDevices.push_back(serialDevice);
					deviceConnected.push_back(true);
				}
			}
		} //For each one of the multiple devices detected
	} //multiple devices

	//One single device detected
	else
	{
		bool notFound = true;

		// If list is empty, create a new driver
		if (connectedDevices.empty())
		{
			Logger::GetInstance()->logMessage("New printer +++",1,0);
			notFound=false;

			serialDevice = devlist[0];
			newDevice = "/dev/"+ serialDevice;

			bool sendEvent = serialDevice!="ttyS1";
			DeviceCenter::GetInstance()->newDriverForPrinter(newDevice,sendEvent);

			connectedDevices.push_back(serialDevice);
			deviceConnected.push_back(true);
		}

		// If there are previously connected devices
		else
		{
			int d = 0;
			for (std::vector<std::string>::iterator it = connectedDevices.begin(); it != connectedDevices.end() ; it++)
			{
				// Disconnect those devices which do not match with the only connected device
				if (connectedDevices[d] != devlist[0])
				{
					if(deviceConnected[d])
					{
						newDevice = "/dev/"+ connectedDevices[d];

						Logger::GetInstance()->logMessage("Sending event: printer disconnected",1,0);
						printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerDisconnected"));
						printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, newDevice.c_str()));

						argv[2] = printerOBJ;

						// Disconnect printer
						DeviceCenter::GetInstance()->updateState(newDevice,"disconnected");

						// Send event
						data->callback->Call(3,argv);
						deviceConnected[d]=false;
					 }
				}
				d++;
			}

			int e =0;

			// Check each one of the devices connected
			for (std::vector<std::string>::iterator it = connectedDevices.begin(); it != connectedDevices.end() ; it++)
			{

				// If the connected device was already in the list --> reconnect printer
				if (devlist[0] == connectedDevices[e])
				{
					notFound=false;
					if (deviceConnected[e])
					{
						alreadyConnected=true;
					}
					else
					{
						serialDevice=devlist[0];
						newDevice = "/dev/"+ serialDevice;

						//printer reconnected
						printerOBJ->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, "printerConnected"));
						printerOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate, newDevice.c_str()));

						argv[2] = printerOBJ;

						data->callback->Call(3,argv);

						DeviceCenter::GetInstance()->reconnectPrinter(e);

						deviceConnected[e]=true;
						alreadyConnected = true;
					}

				}
				e++;
			}

			// If the connected device was not in the list --> Create new driver
			if (notFound)
			{
				serialDevice = devlist[0];
				newDevice = "/dev/"+ serialDevice;

				bool sendEvent = serialDevice!="ttyS1";

				DeviceCenter::GetInstance()->newDriverForPrinter(newDevice, sendEvent);

				connectedDevices.push_back(serialDevice);
				deviceConnected.push_back(true);
			}
		}

	} //one single device

	DeviceCenter::GetInstance()->updateConnected(deviceConnected, false);


	// Release memory
	printr_free_device_list(devlist);


	// Schedule another uv_queue_worker (relaunch thread). Equivalent to having a while(true) loop
	uv_queue_work(uv_default_loop(), req, preRunDriver,(uv_after_work_cb) runDriver);

}

//Start driver:
// This function starts the threads for usb pulling and driver controllers
void Start(const FunctionCallbackInfo<Value>& args)
{

	std::cout << "Starting Formide drivers v6.6.7 (modified)" << std::endl;

	std::cout << "Debug level: " << Logger::GetInstance()->getLogLevel() << std::endl;

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	Local<Function> cb = Local<Function>::Cast(args[0]);

	Local<Object> errOBJ = Object::New(isolate);
	errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,400));
	errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Could not activate Driver Emitter"));

	Local<Object> resultOBJ = Object::New(isolate);
	resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
	resultOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Driver started"));


	// Callback function (v8 object) needs to be saved so events can be sent through that channel
	v8::Local<v8::Function> callback;
	callback = args[0].As<v8::Function>();

	// This is the only way to send the callback function as an argument
	ListBaton* baton = new ListBaton();
	baton->callback = new Nan::Callback(callback);

	// Save callback in DeviceCenter so it can be used through the whole application
	DeviceCenter::GetInstance()->setCallback(baton->callback);

	uv_work_t* req = new uv_work_t();
	req->data = baton;

	// Launch uv_queue_work
	// This runs two functions: preRunDriver (in a separated thread), and immediately after it runs runDriver (in the main thread).
	// We use preRunDriver to "sleep" our application without freezing the main NodeJS using this addon.
	// While most actions are done in runDriver
	// Reason to run the application in main thread is that the callback function can not be called from another thread.
	// It needs to be called from the same "isolated environment", which means, same thread
	uv_queue_work(uv_default_loop(), req, preRunDriver,(uv_after_work_cb) runDriver);


	// Return success
	const unsigned numberOfArguments = 2;
	Local<Value> callbackArguments[numberOfArguments] = { v8::Null(isolate), resultOBJ};
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);

	return;

}

//Pause print function. Args: printerID, pauseGCode, callback
void PausePrint(const FunctionCallbackInfo<Value>& args) {

	Logger::GetInstance()->logMessage("Pause print received",1,0);

	// Declare NodeJS variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// Declare callback variables
	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);

	const int numberOfArguments=2;
	Local<Value> callbackArguments[numberOfArguments] = { errOBJ, resultOBJ};

	// Parse printerID from arguments
	String::Utf8Value v8String(args[0]->ToString());
	std::string printerID = std::string(*v8String);

	// Parse pauseGcode from arguments
	std::string pauseGCode="";

	// Parse Callback from arguments
	Local<Function> cb;

	// This is writen this way to keep backwards compatibility
	if(args[1]->IsFunction())
	{
		cb = Local<Function>::Cast(args[1]);
	}
	else
	{
		String::Utf8Value v8String(args[1]->ToString());
		pauseGCode = std::string(*v8String);

		cb = Local<Function>::Cast(args[2]);
	}


	if(!pauseGCode.size()>0)
		pauseGCode="G91\nG1 E-6\nG1 Z10\nG90\n";


	MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(printerID);

	// Pause print
	bool success = driver->pausePrint();

	if (success)
	{

		try
		{
			// Make multiple commands separating by \n
			size_t pos0 = 0;
			size_t posT = (pauseGCode.find("\n",pos0)) +1;
			std::string gcodeChunk;

			Logger::GetInstance()->logMessage("Appending gcode",4,0);

			// If only one command (which means, no '\n' found)
			if(pauseGCode.size()>0 && posT == std::string::npos)
			{

				char* newLine = new char('\n');
				pauseGCode.append(newLine);
			}

			while (posT != std::string::npos)
			{
				gcodeChunk = pauseGCode.substr(pos0,posT-pos0-1);
				pos0 = posT;
				Logger::GetInstance()->logMessage("Appending: ",5,0);
				Logger::GetInstance()->logMessage(gcodeChunk,5,0);

				std::string callback;

				// Add each single command in raw line buffer
				driver->pushRawCommand(gcodeChunk);

				posT = (pauseGCode.find("\n",posT));
				if (posT != std::string::npos)
				{
					posT+=1;
				}
			}
		}
		catch(...)
		{
			Logger::GetInstance()->logMessage("Exception while parsing pause gcode",1,0);

		}

		// Return success response
		resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
		resultOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Print paused"));
		resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[0] = v8::Null(isolate);
	}
	else
	{
		// Return error response
		errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,400));
		errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Could not pause print"));
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[1] = v8::Null(isolate);
	}


	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);

	return;
}

//Resume print function. Args: printerID, resumeGCode, callback
void ResumePrint(const FunctionCallbackInfo<Value>& args) {

	// Declare NodeJS variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// Declare callback variables
	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);

	const int numberOfArguments=2;
	Local<Value> callbackArguments[numberOfArguments] = { errOBJ, resultOBJ};

	// Parse printer ID from arguments
	String::Utf8Value v8String(args[0]->ToString());
	std::string printerID = std::string(*v8String);

	// Parse resumeGcode from arguments
	std::string resumeGCode="";

	// Parse Callback from arguments
	Local<Function> cb;


	// This is writen this way to keep backwards compatibility
	if(args[1]->IsFunction())
	{
		cb = Local<Function>::Cast(args[1]);
	}
	else
	{
		// Parse Resume Gcode
		String::Utf8Value v8String(args[1]->ToString());
		resumeGCode = std::string(*v8String);

		cb = Local<Function>::Cast(args[2]);
	}


	if(!resumeGCode.size()>0)
		resumeGCode="G91\nG1 Z-10\nG90\n";


	// Resume print
	Logger::GetInstance()->logMessage("Resume print received",1,0);
	MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(printerID);
	bool success = driver->resumePrint();

	if (success)
	{

		try
		{

			// Make multiple commands separating by \n
			size_t pos0 = 0;
			size_t posT = (resumeGCode.find("\n",pos0)) +1;
			std::string gcodeChunk;

			Logger::GetInstance()->logMessage("Appending gcode",4,0);

			// If only one command (which means, no '\n' found)
			if(resumeGCode.size()>0 && posT == std::string::npos)
			{
				char* newLine = new char('\n');
				resumeGCode.append(newLine);
			}

			while (posT != std::string::npos)
			{
				gcodeChunk = resumeGCode.substr(pos0,posT-pos0-1);
				pos0 = posT;
				Logger::GetInstance()->logMessage("Appending: ",5,0);
				Logger::GetInstance()->logMessage(gcodeChunk,5,0);

				std::string callback;

				// Add each single command in raw line buffer
				driver->pushRawCommand(gcodeChunk);

				posT = (resumeGCode.find("\n",posT));
				if (posT != std::string::npos)
				{
					posT+=1;
				}
			}

		}
		catch(...)
		{
			Logger::GetInstance()->logMessage("Exception while parsing resume gcode",1,0);
		}
		resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
		resultOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Print resumed"));
		resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[0] = v8::Null(isolate);
	}

	else
	{
		errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,400));
		errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Could not resume print"));
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[1] = v8::Null(isolate);
	}

	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);
	return;

}

//Stop print function
void StopPrint(const FunctionCallbackInfo<Value>& args) {

	Logger::GetInstance()->logMessage("Stop print received",1,0);

	// Declare NodeJS variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// Declare callback variables
	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);

	const int numberOfArguments=2;
	Local<Value> callbackArguments[numberOfArguments] = { errOBJ, resultOBJ};


	// Parse printer ID from arguments
	String::Utf8Value v8String(args[0]->ToString());
	std::string printerID = std::string(*v8String);

	// Parse stop gcode from arguments
	String::Utf8Value v8String2(args[1]->ToString());
	std::string stopGcode = std::string(*v8String2);

	MarlinDriver* driver = DeviceCenter::GetInstance()->getDriverFromPrinter(printerID);

	if (!stopGcode.size()>0)
		stopGcode="G92 E0\nG28 X\nM104 S0\nM104 S0 T0\nM104 S0 T1\n M140 S0\nM84\n";


	//Divide stop GCODE in lines
	size_t pos0 = 0;
	size_t posT = (stopGcode.find("\n",pos0)) +1;
	std::string gcodeChunk;

	while (posT != std::string::npos)
	{

		gcodeChunk = stopGcode.substr(pos0,posT-pos0-1);
		pos0 = posT;
		std::string callback;

		Logger::GetInstance()->logMessage("Sending stop gcode",2,0);
		Logger::GetInstance()->logMessage(gcodeChunk,2,0);

		// Queue each GCODE command in raw line buffer
		driver->pushRawCommand(gcodeChunk);

		posT = (stopGcode.find("\n",posT));

		if (posT != std::string::npos)
		{
			posT+=1;
		}
	}

	// Stop print
	bool success = driver->stopPrint("");

	if (success)
	{
		resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
		resultOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Print stopped"));
		resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[0] = v8::Null(isolate);
	}
	else
	{
		errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,400));
		errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Could not stop print"));
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[1] = v8::Null(isolate);
	}

	// Return
	Local<Function> cb = Local<Function>::Cast(args[2]);
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);
	return;

}

//Get Communication Logs function
void GetCommunicationLogs(const FunctionCallbackInfo<Value>& args)
{

	Logger::GetInstance()->logMessage("Communication Logs endpoint",3,0);

	// Declare NodeJS variables
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// Declare callback variables
	Local<Object> resultOBJ = Object::New(isolate);
	Local<Object> errOBJ = Object::New(isolate);

	const int numberOfArguments=2;
	Local<Value> callbackArguments[numberOfArguments] = { errOBJ, resultOBJ};

	// parse printer port
	String::Utf8Value v8String(args[0]->ToString());
	std::string printerID = std::string(*v8String);

	// parse line count
	String::Utf8Value v8String2(args[1]->ToString());
	std::string lineCountS = std::string(*v8String2);

	// parse skip count
	String::Utf8Value v8String3(args[2]->ToString());
	std::string skipCountS = std::string(*v8String3);

	int lineCount,skipCount;
	try{
		lineCount = atoi(lineCountS.c_str());
		skipCount = atoi(skipCountS.c_str());
	}

	catch(...)
	{
		Logger::GetInstance()->logMessage("Error: Could not get lineCount and skipCount",1,0);

		errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,400));
		errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Missing lineCount and skipCount"));
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[1] = v8::Null(isolate);

		Local<Function> cb = Local<Function>::Cast(args[3]);
		cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);
		return;
	}

	std::string res;
	int status = DeviceCenter::GetInstance()->readCommunicationLog(printerID,lineCount,skipCount,res);

	if(status < 0)
	{
		// Possible errors
		if(status == -1)
		{
			errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,801));
			errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Communication log file not found"));
		}
		if(status == -2)
		{
			errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,802));
			errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Could not open log file"));
		}
		if(status == -3)
		{
			errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,803));
			errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Internal Error"));
		}
		if(status == -4)
		{
			errOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,804));
			errOBJ->Set(String::NewFromUtf8(isolate, "msg"), String::NewFromUtf8(isolate,"Communication log file is empty"));
		}
		errOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		callbackArguments[1] = v8::Null(isolate);
	}

	else
	{
		// Create success response
		resultOBJ->Set(String::NewFromUtf8(isolate, "code"), Integer::New(isolate,200));
		resultOBJ->Set(String::NewFromUtf8(isolate, "port"), String::NewFromUtf8(isolate,printerID.c_str()));
		resultOBJ->Set(String::NewFromUtf8(isolate, "communicationLogs"), String::NewFromUtf8(isolate,res.c_str()));
		callbackArguments[0] = v8::Null(isolate);
	}

	Local<Function> cb = Local<Function>::Cast(args[3]);
	cb->Call(isolate->GetCurrentContext()->Global(), numberOfArguments, callbackArguments);
	return;

}

// Init function to link functions with Node application
// Expose driver functions (API)
void init(Handle<Object> exports) {

	NODE_SET_METHOD(exports, "start", Start);
	NODE_SET_METHOD(exports, "sendGcode", SendCommand);
	NODE_SET_METHOD(exports, "sendTuneGcode", SendTuneCommand);
	NODE_SET_METHOD(exports, "getPrinterList", GetPrinterList);
	NODE_SET_METHOD(exports, "getPrinterInfo", GetPrinterInfo);
	NODE_SET_METHOD(exports, "printFile", PrintFile);
	NODE_SET_METHOD(exports, "pausePrint", PausePrint);
	NODE_SET_METHOD(exports, "resumePrint", ResumePrint);
	NODE_SET_METHOD(exports, "stopPrint", StopPrint);
	NODE_SET_METHOD(exports, "getCommunicationLogs", GetCommunicationLogs);
}

NODE_MODULE(Formidriver, init)
