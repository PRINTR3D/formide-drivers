/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */
#ifndef HANDLER_BLOCKER_H_
#define HANDLER_BLOCKER_H_

#include <vector>
#include <map>
#include <iostream>

class Blocker {
public:
    static Blocker*
    GetInstance();
    std::map<std::string, int> deviceMap;
    std::vector<bool> blocked;
    std::vector<bool> printing;

    std::vector<std::string> printers;

    int printerIndex(std::string printer);

    void updateDeviceMap(std::string dev, int num);

    void blockM(std::string devID, bool print);
    void unblockM(std::string devID);
    int isBlockedM(std::string devID);

    Blocker();
    virtual ~Blocker();

private:
    static Blocker* instance;
};

#endif /* HANDLER_BLOCKER_H_ */
