#ifndef __STATUS_H__
#define __STATUS_H__

#include "command.h"

// Useful parameters that can be requested in flight to assess board status
typedef struct {
    // Power-on info
    uint32_t uptime_ms;                  // HAL_GetTick()
    uint32_t total_uptime; 				 // Total cumulative uptime
    uint32_t boot_count;                 // incremented on every reset, stored in FRAM

    // TODO: SRAM and FRAM boot status

    // Last command
    uint8_t  last_instruction;           // last instruction number received			// TODO: Check status save and dump
    uint8_t  last_cmd_status;            // CMD_OK, CMD_ERROR, etc.
    uint8_t  last_opcode[OPCODE_SIZE];   // Last opcode received

    // Operation counters
    uint16_t photos_taken;               // total raw photos taken
    uint16_t compressions_done;          // total compressions performed
    uint16_t compressed_count;           // compressions currently in memory

    // TODO: Add: Current memory pointer to save next compression, other?

    // Memory status
    uint8_t compressed_metadata_slot;    // current address of compressed metadata. Advance by one on successful compression
    uint32_t compressed_data_sram_ptr;   // current write pointer in compressed space
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


/********************************************************************************
 * @brief  Updates the global board_status struct with the latest system state.
 *
 * @note   Reads current values from system variables and writes them into the
 *         global board_status_t instance. Should be called before any operation
 *         that transmits or saves board status, such as responding to a
 *         telemetry request or calling SaveBoardStatusFRAM().
 ********************************************************************************/
void UpdateStatus(void);

/********************************************************************************
 * @brief  Logs a summary of the restored board status on bootup.
 *
 * @note   Should be called once during initialization after LoadBoardStatusFRAM().
 *         Prints boot count, uptime from last session, total photos taken and
 *         compressions performed.
 ********************************************************************************/
void LogBoardStatus(void);


#endif
