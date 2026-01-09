#ifndef PARSE_H
#define PARSE_H

#include "cJSON.h"

typedef struct {
    double pitch;
    double roll;
} MotionData;

MotionData parse_motion_data(const char *json_str);

#endif