/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */


// Header
#ifndef MARLIN_DRIVER_H_SEEN
#define MARLIN_DRIVER_H_SEEN

// Header files
#include <iostream>
#include <string>
#include <vector>
#include "../Serial/Serial.h"
#include "../../utils/Timer/Timer.h"
#include "../../handler/Buffer/GCodeBuffer.h"
#include "../../handler/Checksum/ChecksumControl.h"
#include "../../handler/CommunicationLogger/CommLogger.h"


// MarlinDriver Class
class MarlinDriver {

// Public
public:

	// Serial variables
	Serial serial_;
	std::string serialPortPath_;

	// Baud Rate variables
	uint32_t baudrate_;
	int baudRateAttempt;
	int maxBaudRateAttempt;

	// Gcode Buffer
	GCodeBuffer gcodeBuffer_; // not initialized

	// Raw Buffer for GCODE or TUNE commands
	std::vector<std::string> rawLineBuffer;

	/*
	Name: Constructor
	Purpose: Initializes the variable
	*/
	MarlinDriver(const std::string& port, const uint32_t& baudrate);

	// CONNECTION WITH THE PRINTER
	/*
	Name: Open Connection
	Purpose: Initializes and opens serial connection
	*/
	int openConnection();

	/*
	Name: Close Connection
	Purpose: Closes serial connection
	*/
	int closeConnection();

	/*
	Name: Reconnect
	Purpose: Reconnects serial connection
	*/
	int reconnect();

	/*
	Name: is Connected?
	Purpose: Returns true if serial is open
	*/
	bool isConnected() const;

	/*
	Name: Is Printer Online?
	Purpose: Returns true if printer is online
	*/
	bool isPrinterOnline() const;

	/*
	Name: Get Baud Rate
	Purpose: Returns baudrate
	*/
	uint32_t getBaudrate();

	/*
	Name: Set Baud Rate
	Purpose: Sets baud rate
	*/
	void setBaudrate(uint32_t baudrate);

	/*
	Name: Switch Baud Rate
	Purpose: Changes baud rate of serial communication
	*/
	void switchBaudrate();



	// BUFFER FUNCTIONS
	/*
	Name: Get Current Line
	Purpose: Get current buffer line
	*/
	int32_t getCurrentLine() const;

	/*
	Name: Get buffered Lines
	Purpose: Get number of buffered lines
	*/
	int32_t getBufferedLines() const;

	/*
	Name: Get total lines
	Purpose: Get total numbers of gcode lines
	*/
	int32_t getTotalLines() const;

	/*
	Name: Set GCODE
	Purpose: Clears buffer and sets new gcode (used in stopPrint)
	*/
	void setGCode(const std::string& gcode, bool buff);

	/*
	Name: Append GCODE
	Purpose: Appends GCODE at the end of the buffer
	*/
	void appendGCode(const std::string& gcode, bool thread);

	/*
	Name: Clear GCODE
	Purpose: Clears GCODE Buffer
	*/
	void clearGCode();


	// Printer State
	typedef enum STATE{ OFFLINE, DISCONNECTED, CONNECTING, ONLINE, WAITING, PRINTING, STOPPING, PAUSED, HEATING, BUSY, HEATING_BLOCKED } STATE;
	STATE state;
	bool paused_;

	// Blocked Unit
	typedef enum BLOCKED_UNIT{ NONE, EXTRUDER0, EXTRUDER1, BED} BLOCKED_UNIT;
	BLOCKED_UNIT blocked_unit;


	/*
	Name: Get State
	Purpose: Returns printer state
	*/
	STATE getState() const;
	void setStatePrinting();
	void setStateOnline();
	void setState(STATE state);


	/* MAIN LOGIC */
	// MAIN Iteration
	/* This loop continuously runs since it's called by a uv_queue_worker non-stop */
	int mainIteration();

	string replaceTemperatureCommandIfNecessary(string code);
	void checkIfTemperaturesAreReached();

	/*
	Name: Push Raw Command
	Purpose: Sends a command directly to the printer, or queues it to be sent as a Tune command
	*/
	void pushRawCommand(const std::string& gcode);

	/*
	Name: Extra Status From Gcode
	Purpose: Get extra printer status from gcode commands such as fan speed, flow rate, speed multiplier, extruder ratio used, etc.
	*/
	bool extraStatusFromGcode(const std::string& gcode);


	// Printing Actions

	/*
	Name: Start Print
	Purpose: Starts printing a GCODE file
	*/
	bool startPrint(bool printing, STATE state = PRINTING);

	/*
	Name: Reset Print
	Purpose: Clears buffer
	*/
	bool resetPrint();

	/*
	Name: Pause Print
	Purpose: Pauses Print
	*/
	bool pausePrint();

	/*
	Name: Pause Print for Error
	Purpose: Pauses Print and sends Error Event (callback)
	*/
	bool pausePrintForError(std::string key, std::string message);

	/*
	Name: Print Paused
	Purpose: Returns true if printer is paused
	*/
	bool printPaused();

	/*
	Name: Resume Print
	Purpose: Resumes Print
	*/
	bool resumePrint();

	/*
	Name: Stop Print
	Purpose: Stops current print and sends stop gcode
	*/
	bool stopPrint(const std::string& endcode);

	/*
	Name: Stop current print
	Purpose: Stops Current Print
	*/
	bool stopPrint();



protected:


	/*
	Name: Read from Serial
	Purpose: Read from serial communication
	*/
	int readFromSerial();

	/*
	Name: Send Code
	Purpose: Send Code
	*/
	void sendCode(const std::string& code);

	/*
	Name: Write to serial
	Purpose: Write to serial communication
	*/
	bool writeToSerial(const std::string& code);

	/*
	Name: Send message from raw buffer
	Purpose: Write to serial communication
	*/
	void sendMessageFromRawBuffer();

	/*
	Name: Extract line number
	Purpose: In order to know which sequence number (N) was sent to the printer
	*/
	int extractMessageSequenceNumberFromCode(std::string code);

	/*
	Name: Parse Response from printer
	Purpose: Parse response and extract information to perform actions
	*/
	void parseResponseFromPrinter(std::string& code, bool* echo);

	/*
	Name: Update printer status
	Purpose: Update printer data for status reporting
	*/
	void updatePrinterStatus(std::string readData);

	/*
	Name:  Check Temperature or send next message
	Purpose: Sends next message from buffers or checks for temperature
	*/
	void checkTemperatureOrSendNextMessage();

	/*
	Name: Check firmware
	Purpose: Sends M114 to printer
	*/
	void checkFirmware();

	/*
	Name: print next line
	Purpose: Print next line from buffers
	*/
	void sendNextMessage();

	/*
	Name: Print GCODE line from Buffer
	Purpose: Prints next line from GCodeBuffer*
	*/
	void printGcodeLineFromBuffer();


private:
	static const int ITERATION_INTERVAL;

	// For printer connected notification
	bool connected;

	// Checksum Control
	ChecksumControl* checksumControl;

	// Communication Logger
	CommLogger* communicationLogger;

	// Serial variables
	int openSerialAttempt;
	int maxOpenSerialAttempt;
	bool printerNotValid;

	// Temperature timers
	Timer timer_;
	Timer temperatureTimer_;
	bool temperatureOrCoordinate;
	int checkTemperatureInterval_;
	bool checkConnection_;
	int checkTemperatureAttempt_;
	int maxCheckTemperatureAttempts_;


	// Key messages received
	bool formatErrorReceived;
	int waitReceived;
	bool skipMessageReceived;

	// Resend logic
	bool resendReceived;
	bool previousMessageSentWasAResend;
	int resendAttempt;
	int maxResendAttempt;
	std::string resendMessage;

	// Resend counter;
	int resendCounter;

	// Checksum
	// Adding line number (N20) and checksum *CS is not necessary for a previously formatted message
	bool skipFormat;


	// on status HEATING_BLOCKED, if target temperature is zero, we resume the print
	int targetZeroPatch;

	// on status HEATING_BLOCKED, print is not immediately resumed after reaching target
	int targetExtruderPatch;

	// Action control variables
	int usedExtruder;
	bool temperatureCommandNeedsToBeSent;
	bool receivedOkWithoutNewTemperatureCommand;
	int iterationsWithoutCheckingTemperatureWhileHeating;
	int maxIterationsWithoutCheckingTemperatureWhileHeating;
	bool printResumedAfterPause;

	// Serial re-send
	// To resend message when previous attempt returned a serial communication error
	bool previousMessageSerialError;
	std::string serialCommand;

	// Resume position
	// After moving extruder to the corner of the print bed, position needs to be resumed
	// and extrusion value (E) needs to be set to previous value
	bool mustReturnToResumePosition;
	double resumeEvalue;


	/* PATCHS */
	// Builder patch: Filled buffer with "Echo: Unknown command"
	int numEchos;

	// 3DGence patch: Printer sends "A:ok" instead of "ok"
	bool gence3d;


};

#endif /* ! MARLIN_DRIVER_H_SEEN */
