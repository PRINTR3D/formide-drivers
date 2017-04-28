/*
 * This file is modified, and part of the Doodle3D project (http://doodle3d.com).
 *
 * Original can be found here: https://github.com/Doodle3D/print3d
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef UTILS_FIND_DEVICES_HPP_
#define UTILS_FIND_DEVICES_HPP_

#include <iostream>
#include <vector>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/types.h>
#include <dirent.h>

char**
printr_find_devices()
{
    static const char* DEV_DIR = "/dev";

#ifdef __APPLE__
    static const char* names[] = { "tty.usbmodem", "tty.usbserial-", "tty.wch", "tty.SLAB_USBtoUART", NULL };
#elif __linux
    static const char* names[] = { "ttyACM", "ttyUSB", "ttyS1", NULL };
#endif

    int result_len = 6; //make room for 1 device path plus sentinel (common case)
    char** result = (char**)malloc(result_len * sizeof(char*));
    if (!result)
        return NULL;

    DIR* dir = opendir(DEV_DIR);
    if (!dir) {
        free(result);
        return NULL;
    }

    result[0] = result[1] = result[2] = result[3] = result[4] = result[5] = NULL;
    int result_pos = 0;
    struct dirent* ent = 0;
    while ((ent = readdir(dir)) != 0) {
        for (int nidx = 0; names[nidx] != 0; nidx++) {
            const char* name = names[nidx];
            if (ent->d_type == DT_CHR && strncmp(ent->d_name, name, strlen(name)) == 0) {
                if (result_pos + 1 >= result_len) {
                    result_len++;
                    result = (char**)realloc(result, result_len * sizeof(char*));
                    if (!result) {
                        closedir(dir);
                        return NULL;
                    }
                }

                result[result_pos] = strdup(ent->d_name);
                result_pos++;
            }
        }
    }

    closedir(dir);
    return result;
}

void printr_free_device_list(char** list)
{
    if (!list)
        return;
    for (int i = 0; list[i] != NULL; i++)
        free(list[i]);
    free(list);
}

#endif /* UTILS_FIND_DEVICES_HPP_ */
