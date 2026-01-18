/*
 * Command Processor Header
 */

#ifndef CMD_PROCESSOR_H
#define CMD_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

// Initialize command processor
void cmd_processor_init(void);

// Evaluate command processor (call in main loop)
void cmd_processor_eval(void);

#endif // CMD_PROCESSOR_H
