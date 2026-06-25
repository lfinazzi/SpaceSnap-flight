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
    uint32_t fw_backup_size;   		// Size in bytes of main application backup in FRAM
    uint32_t fw_backup_crc32;		// CRC32 of main application backup in FRAM
} fw_backup_info_t;


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
    uint16_t images_rejected_black;					// total black images rejected

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
    uint8_t  compression_buffer_occupied;      		// is compression buffer in use?

    // ADC readings
    uint32_t  mcu_temp;								// MCU Temp reading of MCU with ADC
    uint32_t  vrefint;								// VREF reading of MCU with ADC

    // Photo taking params
    cam_params_t cam_params;						// Parameters that can be changed for photo capturing, all have default values, but can be changed

} board_status_t;

typedef char board_status_t_size[	// Static assert to be sure to erase FRAM after changing board_status_t struct
    (sizeof(board_status_t) == 84) ? 1 : -1
];


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
 * @brief  Updates volatile fields in board_status with the latest system
 *         state.
 *
 * @note   Updates board_status.uptime_session from HAL_GetTick() and reads
 *         ADC values (MCU temperature and VREFINT) via Read_MCU_ADC_Vals().
 *         Should be called before any operation that transmits or saves
 *         board_status, such as CMD_GetStatus() or SaveBoardStatusFRAM().
 ********************************************************************************/
void UpdateStatus(void);


/********************************************************************************
 * @brief  Logs a brief summary of board status over UART4 (debug).
 *
 * @note   Should be called once during initialization after
 *         LoadBoardStatusFRAM(). Prints boot count, total historic uptime,
 *         total photos taken, compressions performed, and FRAM/SRAM init
 *         status on two lines.
 ********************************************************************************/
void LogBoardStatus(void);


/********************************************************************************
 * @brief  Logs a full field-by-field breakdown of board_status over UART4
 *         (debug).
 *
 * @note   Prints one line per field covering all board_status_t members:
 *         uptime (session and total, where total includes the current session
 *         contribution), boot count, power-down counters, reset cause
 *         counters, last_reset_cause, FRAM/SRAM init flags, last instruction
 *         and opcode (in hex), operation counters, FRAM compression tracking,
 *         SRAM buffer occupancy flags, decoded VDD voltage and MCU temperature
 *         (computed from raw ADC values via VREFINT_CAL and TEMPSENSOR
 *         constants), and board_status_t size vs available tx_buffer payload.
 *
 *         backup_fw_crc and backup_fw_size are also included in this log.
 *
 *         Called automatically by CMD_GetStatus() on every status request.
 *         Requires -u _printf_float linker flag for float formatting support
 *         (VDD and MCU temperature fields use %.3f and %.1f respectively).
 ********************************************************************************/
void LogBoardStatusFull(void);


#endif
