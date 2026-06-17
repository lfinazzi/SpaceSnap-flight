#ifndef __STATUS_H__
#define __STATUS_H__

#include "command.h"
#include "fram.h"

#define MAX_COMPRESSED_PHOTOS           (40U)				// Maximum number of compressions possible

#define RESET_CAUSE_UNKNOWN   			(0U)
#define RESET_CAUSE_POR       			(1U)
#define RESET_CAUSE_PIN       			(2U)
#define RESET_CAUSE_SOFTWARE       		(3U)
#define RESET_CAUSE_IWDG       			(4U)
#define RESET_CAUSE_LOWPOWER       		(5U)


typedef struct __attribute__((packed)) {
    uint32_t fram_address;   // where the compression header starts in FRAM
    uint32_t total_size;     // header_size + jpeg_size for this entry
    uint8_t  valid;          // 1 if this slot holds a real entry
} compression_index_entry_t;


// Useful parameters that can be requested in flight to assess board status
// VOLATILE members are updated once on boot
typedef struct {

    // Power-on info - VOLATILE
    uint32_t uptime_session;             			// Uptime of current boot session [ms]

    // boot count and total uptime - NON_VOLATILE
    uint32_t uptime_total; 				 			// Total historic uptime [s]
    uint32_t boot_count;                 			// incremented on every reset, stored in FRAM
    uint16_t requested_power_downs;					// Number of power-downs requested

    // power down errors - NON-VOLATILE
    uint16_t iwdg_reset_count;						// IWDG reset
    uint16_t lowpwr_reset_count;					// Low power reset
    uint16_t sftw_reset_count;						// Software reset
    uint16_t por_reset_count;						// POR reset
    uint16_t pin_reset_count;						// pin reset
    uint16_t unk_reset_count;						// Unknown reset

    // VOLATILE - Gets updated on every boot
    uint8_t last_reset_cause;
    //uint16_t last_fault_pc;						// TODO: This needs a special linker script to be loaded correctly on next startup before it is stepped by MCU

    // memory ok? VOLATILE
    uint8_t fram_ok;					 			// FRAM init ok
    uint8_t sram_ok;					 			// SRAM init ok

    // Last command - VOLATILE
    uint8_t  last_instruction;           			// last instruction number received
    uint8_t  last_cmd_status;            			// CMD_OK, CMD_ERROR, etc.
    uint8_t  last_opcode[OPCODE_SIZE];   			// Last opcode received

    // Operation counters - NON-VOLATILE
    uint16_t photos_taken;               			// total raw photos taken
    uint16_t compressions_done;          			// total compressions performed
    uint16_t images_rejected_black;					// TODO when implementing Black Filtering!

    // Compression FRAM memory tracking - NON-VOLATILE
    uint32_t compression_ptr_address; 				// Address for next compression in FRAM
    uint16_t compression_count;         			// compressions saved in FRAM
    uint32_t fram_bytes_left;						// Bytes left to save compressions in FRAM

    // SRAM memory status - VOLATILE
    uint8_t  raw_buffer_1_occupied;      			// is raw buffer 1 in use?
    uint8_t  raw_buffer_2_occupied;      			// is raw buffer 2 in use?
    uint8_t  raw_buffer_3_occupied;      			// is raw buffer 3 in use?
    uint8_t  raw_buffer_4_occupied;     			// is raw buffer 4 in use?
    uint8_t  raw_buffer_5_occupied;      			// is raw buffer 5 in use?

    // ADC readings
    uint32_t  mcu_temp;								// MCU Temp reading of MCU with ADC
    uint32_t  vrefint;								// VREF reading of MCU with ADC

    // Firmware backup integrity
    uint32_t backup_fw_crc;							// TODO, will need final CRC to compare. FW crc will be saved in FRAM and this will be calculated on boot


} board_status_t;

// Saves current board status
extern board_status_t board_status;
extern compression_index_entry_t compression_table[MAX_COMPRESSED_PHOTOS];		// Compression table

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

/********************************************************************************
 * @brief  Logs a full summary of the restored board status on request.
 * TODO: Comment
 ********************************************************************************/
void LogBoardStatusFull(void);

#endif
