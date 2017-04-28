/*
 *	This code was created for Printr B.V. It is open source under the formide-drivers package.
 *	Copyright (c) 2017, All rights reserved, Printr B.V.
 */
// This class contains all functions that are run in a separated thread

#include <map>
#include <node.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "handler/DeviceCenter/DeviceCenter.h"

#ifndef FORMITHREAD_H_
#define FORMITHREAD_H_

struct thread_event {
    v8::Local<v8::Object> arg;
    v8::Isolate* isolate;
};

struct thread_data {
    std::string serialDevice;
    std::string gcodeFile;
};
class FormiThread {

public:
    FormiThread();
    ~FormiThread();

    static void* countLines(void*);
    static void* calculatePrintingTime(void*);
    static void* appendGcode(void*);
    static void* workers(void*);
    static void* insertLinesManually(void*);
};

#endif /* FORMITHREAD_H_ */
