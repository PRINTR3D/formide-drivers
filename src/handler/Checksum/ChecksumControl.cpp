/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */

#include "ChecksumControl.h"
#include "../DeviceCenter/DeviceCenter.h"
#include "../../utils/Logger/Logger.h"

using namespace std;

ChecksumControl* ChecksumControl::instance = nullptr;

ChecksumControl::ChecksumControl()
{
    currentPosition = 0;

    // First we fill buffer up with M110 N0, to make sure there are no undefined positions
    // These will be replaced with new commands.
    for (int j = 0; j < 1000; j++) {
        checksumVector.push_back(addChecksum("M110 N0"));
    }
}

ChecksumControl*
ChecksumControl::GetInstance()
{
    if (instance == nullptr) {
        instance = new ChecksumControl();
    }
    return instance;
}

ChecksumControl::~ChecksumControl()
{
}

// Adds sequence number + checksum to message
string
ChecksumControl::getFormattedMessageOnly(int sequenceNumber, string message)
{
    string formattedMessage = "N" + std::to_string(sequenceNumber) + " " + message;
    formattedMessage = addChecksum(formattedMessage);
    return formattedMessage;
}

// Adds sequence number + checksum to message (and it is stored in checksumVector)
// http://reprap.org/wiki/G-code#N:_Line_number
string
ChecksumControl::getFormattedMessage(string message)
{

    int currentPos = currentPosition + 1;
    string formattedMessage = "N" + std::to_string(currentPos) + " " + message + " ";

    formattedMessage = addChecksum(formattedMessage);

    addToVector(formattedMessage);

    return formattedMessage;
}

// Get previously sent message by sequence number
string
ChecksumControl::getOldMessage(int sequenceNumber)
{
    int position = (sequenceNumber - 1) % 1000;

    if (position > currentPosition) {
        return addChecksum("M110 N0");
    }

    return checksumVector[position];
}

// This replaces a previously sent (wrongly formated) message with M114
string
ChecksumControl::getOldMessageFixFormat(int sequenceNumber)
{
    int position = (sequenceNumber - 1) % 1000;

    string fixFormat = "N" + std::to_string(sequenceNumber) + " M114";
    fixFormat = addChecksum(fixFormat) + "\n";

    checksumVector[position] = fixFormat;

    return checksumVector[position];
}

// Replace previously sent message with new message (using same sequence number)
string
ChecksumControl::updateChecksum(int sequenceNumber, string message)
{

    int position = (sequenceNumber - 1) % 1000;

    size_t positionChecksum = message.find("*", 0);

    if (positionChecksum != std::string::npos) {
        string messageWithoutChecksum = message.substr(0, positionChecksum);
        string newMessage = addChecksum(messageWithoutChecksum);

        checksumVector[position] = newMessage;
        return newMessage;
    }
    else
    {
        return message;
    }
}

// Reset sequence number counter and previously sent buffer
void ChecksumControl::reset(bool everything)
{

    int a = 0;
    Logger::GetInstance()->logMessage("Reseting checksum vector", 3, 0);
    for (std::vector<string>::iterator it = checksumVector.begin(); it != checksumVector.end(); it++) {
        checksumVector[a] = addChecksum("M110 N0");
        a++;
    }

    if (everything)
        currentPosition = 0;
}

// Adds checksum to string: using
// http://reprap.org/wiki/G-code#.2A:_Checksum
string
ChecksumControl::addChecksum(string message)
{

    string command = std::string(message);
    size_t posT = (command.find(";"));

    try {
        if (posT != std::string::npos) {
            command = command.substr(0, posT);
        }
    }
    catch (...) {
        std::cout << "substr exception at checksum" << std::endl;
    }

    const char* cmd = command.c_str();

    int cs = 0;

    for (int i = 0; cmd[i] != '*' && cmd[i] != NULL; i++) {
        cs = cs ^ cmd[i];
    }

    cs &= 0xff; // defensive programming

    string finalMessage = command + "*" + std::to_string(cs);

    return finalMessage;
}

// Adds message to checksumVector (previously sent messages)
// So messages can be recovered in case they are needed again
void ChecksumControl::addToVector(string message)
{
    int position;

    if (currentPosition == 0)
        position = 0;
    else
        position = currentPosition % 1000;

    checksumVector[position] = message;

    currentPosition++;
}
