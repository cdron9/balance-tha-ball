
#include "cJSON.h"
#include "parse.h"
#include <stdlib.h>
#include <stdio.h>


MotionData parse_motion_data(const char *json_str) {
    MotionData data = {0};
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        printf("JSON parse failed!\n"); 
        return data;
    }

    cJSON *pitch = cJSON_GetObjectItemCaseSensitive(root, "motionPitch");
    cJSON *roll  = cJSON_GetObjectItemCaseSensitive(root, "motionRoll");

    if (pitch && cJSON_IsString(pitch)) data.pitch = atof(pitch->valuestring);
    if (roll  && cJSON_IsString(roll))  data.roll  = atof(roll->valuestring);

    cJSON_Delete(root);
    return data;
}
