#ifndef LBJ_TASK_STATE_HPP
#define LBJ_TASK_STATE_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum task_states {
    TASK_INIT = 0,
    TASK_CREATED = 1,
    TASK_RUNNING = 2,
    TASK_DONE = 3,
    TASK_TERMINATED = 4,
    TASK_CREATE_FAILED = 5,
    TASK_RUNNING_SCREEN = 6
};

extern TaskHandle_t task_fd;
extern task_states fd_state;

#endif
