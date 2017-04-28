/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */

#include "Blocker.h"
#include <iostream>

Blocker* Blocker::instance = nullptr;
Blocker::Blocker()
{
    std::cout << "blocker initialized" << std::endl;
    blocked.push_back(false);
    printing.push_back(false);
    printers.clear();
}

int Blocker::printerIndex(std::string printer)
{

    int a = 0;
    bool gotit = false;
    int chosen = -1;
    for (std::vector<std::string>::iterator it = printers.begin(); it != printers.end(); it++) {
        if (printers[a] == printer) {
            gotit = true;
            chosen = a;
        }
        a++;
    }
    return chosen;
}

Blocker*
Blocker::GetInstance()
{
    if (instance == nullptr) {
        instance = new Blocker();
    }
    return instance;
}

void Blocker::blockM(std::string devID, bool print)
{
    int index = printerIndex(devID);
    if (index == -1) {
        printers.push_back(devID);
        index = printers.size() - 1;
    }

    blocked[index] = true;
    blocked[index] = true;
}
void Blocker::unblockM(std::string devID)
{
    int index = printerIndex(devID);
    if (index == -1) {
        printers.push_back(devID);
        index = printers.size() - 1;
    }
    blocked[index] = false;
    printing[index] = false;
}

int Blocker::isBlockedM(std::string devID)
{
    int index = printerIndex(devID);
    if (index == -1) {
        printers.push_back(devID);
        index = printers.size() - 1;
    }

    if (blocked[index] && printing[index]) {
        return 2;
    }
    else if (blocked[index] == 1) {
        return 1;
    }
    else {
        return 0;
    }
}

Blocker::~Blocker()
{
}
