#ifndef AWNING_H
#define AWNING_H

#include "pico/stdlib.h"

typedef enum {
    AWNING_CMD_UP,
    AWNING_CMD_STOP,
    AWNING_CMD_DOWN
} awning_cmd_t;

void awning_init(void);
void awning_send_cmd(awning_cmd_t cmd);

#endif /* AWNING_H */