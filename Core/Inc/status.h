/**
  ******************************************************************************
  * @file           : status.h
  * @brief          : System status interface — board_status_t and shared state
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __STATUS_H__
#define __STATUS_H__

#include "command.h"
#include "comms.h"
#include "sram.h"

#define MAX_COMPRESSED_PHOTOS           (100U)				// Maximum number of compressions possible

#define RESET_CAUSE_UNKNOWN   			(0U)
#define RESET_CAUSE_POR       			(1U)
#define RESET_CAUSE_PIN       			(2U)
#define RESET_CAUSE_SOFTWARE       		(3U)
#define RESET_CAUSE_IWDG       			(4U)
#define RESET_CAUSE_LOWPOWER       		(5U)

/* In status.h - redundant state shadows, NOT in board_status_t */
extern uint8_t state_shadow_b;
extern uint8_t state_shadow_c;

/* In status.h - important crc shadows, NOT in board_status_t. Check integrity of program memory */
extern uint32_t shadow_board_status_crc;		// Integrity of board_status
extern uint32_t shadow_compression_table_crc;	// Integrity of compression_table

typedef struct __attribute__((packed)) {
    uint32_t fw_backup_size;   		// Size in bytes of main application backup in FRAM
    uint32_t fw_backup_crc32;		// CRC32 of main application backup in FRAM
    uint32_t fw_backup_version;		// Version of main application backup in FRAM
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

    // NON-VOLATILE
    uint8_t state_vote_fail_count;					// Increments by one on app_state majority vote count
    uint16_t ram_corruption_recovery;				// Times board_status in program memory was recovered from FRAM
    uint8_t fram_corruption_write_recovery;			// Times board_status in program memory was recovered from FRAM
    uint8_t fram_corruption_defaulted;				// Times board_status or compression table were defaulted due to FRAM CRC mismatch
    uint8_t fw_backup_mismatch;						// Flag goes to one when there is mismatch between FW backup A and B. Modified live on CMD_GetStatus()

    // Last command - NON-VOLATILE
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

    // SRAM memory status - VOLATILE
    uint8_t raw_buffer_occupied[RAW_PHOTO_COUNT];  // is raw buffer with index i in use?
    uint8_t compression_buffer_occupied;      		// is compression buffer in use?

    // NON-Volatile
    uint32_t fram_bytes_left;						// Bytes left to save compressions in FRAM

    // Last address written in FRAM, to catch errors. TODO: Anything else? This variable gets overwritten every main loop to where compression table is
    // so it is not incredibly useful - VOLATILE
    uint32_t last_fram_write_address;

    // ADC readings
    uint32_t  mcu_temp;								// MCU Temp reading of MCU with ADC
    uint32_t  vrefint;								// VREF reading of MCU with ADC

    // Photo taking params
    cam_params_t cam_params;						// Parameters that can be changed for photo capturing, all have default values, but can be changed

    // Delayed photo params
    delayed_params_t delayed_params;				// Parameters that can be changed for delayed photo burst capturing

    uint8_t delayed_intervals;						// Minutes after start to execute photo capture (mins)
    uint8_t delayed_flag;							// Indicates if instruction was not requested in this boot session
    uint32_t delayed_start;							// Board timestamp when delayed request started (seconds)

    // State tracking
    app_state_t state;								// Tracks actual board state, system can recover in STATE_SCHEDULED in case of power off

} board_status_t;

typedef char board_status_t_size[	// Static assert to be sure to erase FRAM after changing board_status_t struct
    (sizeof(board_status_t) == 104) ? 1 : -1
];


// Board status
extern board_status_t board_status;

// Compression table
extern compression_index_entry_t compression_table[MAX_COMPRESSED_PHOTOS];

// Fw backup info A
extern fw_backup_info_t fw_backup_info_a;

// Fw backup info B
extern fw_backup_info_t fw_backup_info_b;


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


/********************************************************************************
 * @brief  Updates the RAM shadow CRC of board_status to reflect its
 *         current contents.
 *
 * @note   Must be called after every intentional modification of
 *         board_status so the shadow stays in sync. Call sites:
 *         end of ExecuteCommand(), start of program loop after UpdateStatus(),
 *         and end of LoadBoardStatusFRAM() after a successful
 *         load. Also called after erasing compressions or FRAM to match when saved
 *         in main loop. Failure to call this after a legitimate modification
 *         will cause BoardStatusIntact() to falsely detect corruption
 *         on the next check.
 *
 * @retval None
 ********************************************************************************/
void CommitBoardStatus(void);


/********************************************************************************
 * @brief  Updates the RAM shadow CRC of compression_table to reflect
 *         its current contents.
 *
 * @note   Must be called after every intentional modification of
 *         compression_table. Call sites: end of SaveCompressionToFRAM()
 *         and end of EraseCompressions(). Failure to call this after a
 *         legitimate modification will cause CompTableIntact() to
 *         falsely detect corruption on the next check.
 *
 * @retval None
 ********************************************************************************/
void CommitCompressionTable(void);


/********************************************************************************
 * @brief  Checks whether board_status in RAM matches its shadow CRC,
 *         indicating no unintended modification since the last
 *         CommitBoardStatus() call.
 *
 * @note   Recomputes CRC32 over the live board_status struct and
 *         compares against shadow_board_status_crc. A mismatch
 *         indicates either an SRAM SEU or a code path that modified
 *         board_status without calling CommitBoardStatus(). Called
 *         at the top of SaveBoardStatusFRAM() before every FRAM write
 *         to prevent persisting corrupt data.
 *
 * @retval uint8_t   1 if intact, 0 if CRC mismatch detected.
 ********************************************************************************/
uint8_t BoardStatusIntact(void);


/********************************************************************************
 * @brief  Checks whether compression_table in RAM matches its shadow
 *         CRC, indicating no unintended modification since the last
 *         CommitCompressionTable() call.
 *
 * @note   Recomputes CRC32 over the live compression_table and compares
 *         against shadow_compression_table_crc. Called before writing
 *         the compression table to FRAM to prevent persisting corrupt
 *         index data.
 *
 * @retval uint8_t   1 if intact, 0 if CRC mismatch detected.
 ********************************************************************************/
uint8_t CompTableIntact(void);


#endif	/* __STATUS_H__ */
