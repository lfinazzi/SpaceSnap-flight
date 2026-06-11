#ifndef __STATUS_H__
#define __STATUS_H__

#include "command.h"

// Useful parameters that can be requested in flight to assess board status
typedef struct {

	//																						Implemented?

    // Power-on info - VOLATILE
    uint32_t uptime_ms;                  // HAL_GetTick()									OK

    // boot count and total uptime - NON_VOLATILE
    uint32_t total_uptime; 				 // Total cumulative uptime	(seconds)				OK, TODO: Counts kind of bad. Why?
    uint32_t boot_count;                 // incremented on every reset, stored in FRAM		OK

    // memory ok? VOLATILE
    uint8_t fram_ok;					 // FRAM init ok									OK
    uint8_t sram_ok;					 // SRAM init ok									NO

    // Last command - VOLATILE
    uint8_t  last_instruction;           // last instruction number received				OK
    uint8_t  last_cmd_status;            // CMD_OK, CMD_ERROR, etc.							OK
    uint8_t  last_opcode[OPCODE_SIZE];   // Last opcode received							OK

    // Operation counters - NON-VOLATILE
    uint16_t photos_taken;               // total raw photos taken							OK
    uint16_t compressions_done;          // total compressions performed					OK

    // Compression memory tracking - NON-VOLATILE
    uint32_t compression_ptr_address; 	// Address for next compression in FRAM				NO, TODO
    uint16_t compression_count;         // compressions currently in FRAM					NO

    // Memory status - VOLATILE
    uint8_t  raw_buffer_1_occupied;      // is raw buffer 1 in use?							OK
    uint8_t  raw_buffer_2_occupied;      // is raw buffer 2 in use?							OK
    uint8_t  raw_buffer_3_occupied;      // is raw buffer 3 in use?							OK
    uint8_t  raw_buffer_4_occupied;      // is raw buffer 4 in use?							OK
    uint8_t  raw_buffer_5_occupied;      // is raw buffer 5 in use?							OK

    // This can be VOLATILE for current session only
    // TODO: error tracking
    //       GPIO status, other status?
    //		 Other peripheral status?

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
