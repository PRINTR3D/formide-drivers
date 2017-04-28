/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */

#ifndef HANDLER_CHECKSUMCONTROL_H_
#define HANDLER_CHECKSUMCONTROL_H_

#include <iostream>
#include <vector>

using namespace std;

class ChecksumControl {
public:
    ChecksumControl();
    virtual ~ChecksumControl();

    static ChecksumControl*
    GetInstance();

    string getFormattedMessage(string message);
    string getFormattedMessageOnly(int sequenceNumber, string message);
    string getOldMessage(int sequenceNumber);
    string getOldMessageFixFormat(int sequenceNumber);
    string updateChecksum(int sequenceNumber, string message);
    string addChecksum(string message);
    void reset(bool everything);
    int currentPosition;

private:
    static ChecksumControl* instance; // singleton instance
    vector<string> checksumVector;
    void addToVector(string message);
};

#endif /* HANDLER_CHECKSUMCONTROL_H_ */
