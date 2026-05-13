#ifndef __STATUS_H__
#define __STATUS_H__

#include "command.h"

// Useful parameters that can be requested in flight to assess board status
typedef struct {
    // Power-on info
    uint32_t uptime_ms;                  // HAL_GetTick()
    uint32_t boot_count;                 // incremented on every reset, stored in FRAM, TODO: implement boot count

    // Last command
    uint8_t  last_instruction;           // last instruction number received
    uint8_t  last_cmd_status;            // CMD_OK, CMD_ERROR, etc.
    uint8_t  last_opcode[OPCODE_SIZE];   // Last opcode received

    // Operation counters
    uint16_t photos_taken;               // total raw photos taken
    uint16_t compressions_done;          // total compressions performed
    uint16_t compressed_count;           // compressions currently in memory

    // Memory status
    uint32_t compressed_data_ptr;        // current write pointer in compressed space
    uint8_t  raw_buffer_1_occupied;      // is raw buffer 1 in use?
    uint8_t  raw_buffer_2_occupied;      // is raw buffer 2 in use?
    uint8_t  raw_buffer_3_occupied;      // is raw buffer 3 in use?

    // TODO: error tracking

} board_status_t;

// Saves current board status
extern board_status_t board_status;

extern uint32_t timestamp;				  // Variable to save timestamp (seconds) since power on
extern uint32_t total_seconds;			  // Total seconds since power on
extern char timestamp_string[15];		  // String to hold timestamp. Format: "[HH:MM:SS] "
extern uint32_t hours;					  // Hours elapsed
extern uint32_t minutes;				  // Minutes elapsed
extern uint32_t seconds;				  // Seconds elapsed

#endif
