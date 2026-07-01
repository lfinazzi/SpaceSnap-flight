/**
  ******************************************************************************
  * @file           : fram.h
  * @brief          : FRAM driver interface — persistent storage API
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __FRAM_H__
#define __FRAM_H__

#include <stdint.h>
#include "status.h"

#define BOARD_STATUS_START 				(0x000000)													// Address to save board status

#define BOARD_STATUS_CRC    			((BOARD_STATUS_START) + (sizeof(board_status_t)))

#define COMPRESSION_TABLE_START 		((BOARD_STATUS_CRC) + (sizeof(uint32_t)))	// compressed space starts after status information, which starts at 0x00000000

#define COMPRESSION_TABLE_CRC 			((COMPRESSION_TABLE_START) + ((sizeof(compression_index_entry_t))*(MAX_COMPRESSED_PHOTOS)))

#define PHOTO_DATA_START				((COMPRESSION_TABLE_CRC) + (sizeof(uint32_t)))				// Space for compressions

#define FIRMWARE_BACKUP_SIZE			(0x40000UL)		// 256 kB for FW backup image
#define FIRMWARE_COPY_SIZE           	(FIRMWARE_BACKUP_SIZE / 2)   /* 128 KB per copy */

#define END_OF_FRAM						(0x200000)		// 2 MB
#define FIRMWARE_BACKUP_START 			((END_OF_FRAM) - (FIRMWARE_BACKUP_SIZE))

// Two firmware copies are saved in FRAM for extra safety

// Copy A
#define FIRMWARE_BACKUP_A_START      (FIRMWARE_BACKUP_START)
#define FIRMWARE_IMAGE_A_START       (FIRMWARE_BACKUP_A_START + sizeof(fw_backup_info_t))
#define FIRMWARE_IMAGE_A_SIZE        (FIRMWARE_COPY_SIZE - sizeof(fw_backup_info_t))

// Copy B
#define FIRMWARE_BACKUP_B_START      (FIRMWARE_BACKUP_A_START + FIRMWARE_COPY_SIZE)
#define FIRMWARE_IMAGE_B_START       (FIRMWARE_BACKUP_B_START + sizeof(fw_backup_info_t))
#define FIRMWARE_IMAGE_B_SIZE        (FIRMWARE_COPY_SIZE - sizeof(fw_backup_info_t))

// FRAM control Macros
#define FRAM_CMD_WREN   				(0x06U)  	// Write Enable
#define FRAM_CMD_WRITE  				(0x02U)  	// Write Memory
#define FRAM_CMD_READ   				(0x03U)  	// Read Memory

// FRAM write Macros
#define FRAM_WRITE_RETRY				(3U)		// Tries to retry on FRAM write failure before fram_ok = 0


typedef char photo_data_start_check[
    (PHOTO_DATA_START < FIRMWARE_BACKUP_START) ? 1 : -1
];


/********************************************************************************
 * @brief  Reads the device ID from the FRAM via the RDID command (0x9F).
 *
 * @note   Transmits the RDID opcode followed by one dummy byte, then clocks
 *         in len bytes of ID data. CS is asserted for the full transaction.
 *         Returns 0 on success, 1 if either SPI transaction fails.
 *
 * @param  id_buf  Pointer to destination buffer for the ID bytes.
 * @param  len     Number of ID bytes to read.
 *
 * @return 0 on success (both SPI transactions returned HAL_OK).
 *         1 if either the transmit or receive SPI transaction failed.
 ********************************************************************************/
uint8_t FRAM_ReadDeviceID(uint8_t *id_buf, uint8_t len);


/********************************************************************************
 * @brief  Reads the FRAM device ID and verifies it against the expected value.
 *
 * @note   Calls FRAM_ReadDeviceID() to read 4 ID bytes and compares them
 *         against the expected ID {0x51, 0x82, 0x06, 0x00} via memcmp().
 *         Logs the raw ID bytes and pass/fail result over UART4. Should be
 *         called during boot after a minimum 1ms delay from power-up to
 *         satisfy the CY15B108QSN tPU requirement (450us), as the chip ignores
 *         all commands until tPU has elapsed.
 *
 * @return 1 if the device ID matches the expected value.
 *         0 if the device ID does not match or the SPI transaction failed.
 ********************************************************************************/
uint8_t TestFRAM(void);


/********************************************************************************
 * @brief  Saves board_status and compression_table to FRAM.
 *
 * @note   Checks BoardStatusIntact() and CompTableIntact() first; if either
 *         RAM shadow CRC fails, calls RecoverBoardStatusFromFRAM() and
 *         returns without writing to prevent persisting corrupt data.
 *
 *         If RAM is intact, serializes board_status_t and then the
 *         compression table using SaveBufferFRAM() (burst SPI writes).
 *         Each region is followed by its CRC32, which is read back
 *         immediately to verify the write. On verify failure the write is
 *         retried up to FRAM_WRITE_RETRY times; if all retries fail,
 *         board_status.fram_ok is set to 0. On success (possibly after
 *         a retry), board_status.fram_corruption_write_recovery is
 *         incremented if a retry was needed.
 ********************************************************************************/
void SaveBoardStatusFRAM(void);


/********************************************************************************
 * @brief  Loads board_status and compression_table from FRAM on boot.
 *
 * @note   Called once at startup. Validates FRAM hardware via TestFRAM(),
 *         then loads and CRC-verifies both regions independently. On a
 *         CRC failure, resets the affected region to safe defaults and
 *         increments fram_corruption_defaulted. Always increments boot_count,
 *         restores state (filtered to only preserve STATE_DELAYED_PICTURE
 *         across the load), and commits both shadow CRCs.
 *
 * @retval None
 ********************************************************************************/
void LoadBoardStatusFRAM(void);


/********************************************************************************
 * @brief  Restores board_status and compression_table from FRAM at
 *         runtime, in response to a detected RAM shadow CRC mismatch.
 *
 * @note   Called from SaveBoardStatusFRAM() when BoardStatusIntact() or
 *         CompTableIntact() fails. Unlike LoadBoardStatusFRAM(), this
 *         does not run TestFRAM() (FRAM hardware was already confirmed
 *         working this session) and does not increment boot_count.
 *         Uses the same state-restore policy as LoadBoardStatusFRAM():
 *         STATE_DELAYED_PICTURE is preserved; all other states are
 *         forced to STATE_IDLE. Increments ram_corruption_recovery.
 *
 * @retval None
 ********************************************************************************/
void RecoverBoardStatusFromFRAM(void);


/********************************************************************************
 * @brief  Erases the entire FRAM writable region and resets all system state.
 *
 * @note   Writes 0x00 from address 0x00 to FIRMWARE_BACKUP_START in
 *         256-byte chunks via SaveBufferFRAM(). The IWDG is kicked once
 *         per chunk to prevent a watchdog reset during the erase. Does
 *         not touch the firmware backup region (FIRMWARE_BACKUP_START
 *         and above).
 *
 *         After erasing, zeroes board_status and compression_table in RAM
 *         via memset(), resets board_status.compression_ptr_address to
 *         PHOTO_DATA_START, recalculates board_status.fram_bytes_left, then
 *         re-initializes board_status.cam_params and
 *         board_status.delayed_params to their compile-time default values
 *         (*_DEFAULT macros). Note: unlike EraseCompressions(), this also
 *         zeroes board_status itself, losing all persistent counters and
 *         state.
 ********************************************************************************/
void EraseFRAM(void);


/********************************************************************************
 * @brief  Saves a buffer from SRAM into FRAM using a single burst write.
 *
 * @note   Issues a WREN command before writing, as required by the
 *         CY15B108QSN protocol. Transmits the WRITE opcode (0x02) followed
 *         by the 24-bit address MSB first, then streams the full buffer with
 *         CS held LOW for the entire burst. The FRAM's internal address
 *         counter auto-increments after each byte.
 *
 *         Silently returns without writing if fram_address + size would
 *         exceed FIRMWARE_BACKUP_START, to protect the firmware backup region.
 *
 * @param  buffer       Pointer to source data in SRAM (uint8_t array).
 * @param  size         Number of bytes to write.
 * @param  fram_address Starting FRAM address.
 ********************************************************************************/
void SaveBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address);


 /********************************************************************************
  * @brief  Reads a buffer from FRAM into SRAM using a single burst read.
  *
  * @note   Transmits the READ opcode (0x03) followed by the 24-bit address
  *         MSB first, then clocks in size bytes with CS held LOW for the
  *         entire burst. The FRAM's internal address counter auto-increments
  *         after each byte. No dummy cycles are required for standard READ
  *         per the CY15B108QSN datasheet.
  *
  * @param  buffer       Destination buffer in SRAM (uint8_t array).
  * @param  size         Number of bytes to read.
  * @param  fram_address Starting FRAM address.
  ********************************************************************************/
 void ReadBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address);


/********************************************************************************
 * @brief  Erases the compressed photo region in FRAM and resets compression
 *         tracking state.
 *
 * @note   Writes 0x00 from PHOTO_DATA_START to FIRMWARE_BACKUP_START in
 *         256-byte chunks via SaveBufferFRAM(). The IWDG is kicked once
 *         per chunk to prevent a watchdog reset during the erase.
 *
 *         After erasing, resets compression_table[] to zero via memset(),
 *         resets board_status.compression_ptr_address to PHOTO_DATA_START,
 *         recalculates board_status.fram_bytes_left, and clears
 *         board_status.compression_count. Does not touch board_status fields
 *         outside of compression tracking, unlike EraseFRAM() which zeroes
 *         the entire board_status struct.
 ********************************************************************************/
void EraseCompressions(void);


/********************************************************************************
 * @brief  Writes a buffer to FRAM at the specified address without
 *         enforcing the reserved-region boundary.
 *
 * @note   Unlike the standard FRAM write path, this function has no
 *         lower-bound address check and is the only write function that
 *         can reach the firmware backup region (0x1C0000 - 0x1FFFFF).
 *         It must only be called from CMD_BackupFirmware(). Asserts
 *         Write Enable Latch (WREN) before each call as required by the
 *         CY15B108QSN protocol, then issues the WRITE opcode followed
 *         by the 24-bit address MSB first and bursts the full buffer in
 *         a single CS assertion. Only the upper 2 MB boundary
 *         (END_OF_FRAM) is checked — writes that would overflow the
 *         physical device are silently discarded.
 *
 * @param  buffer        Pointer to the data to write.
 * @param  size          Number of bytes to write.
 * @param  fram_address  24-bit destination address in FRAM.
 *
 * @retval None
 ********************************************************************************/
void SaveFRAM_Unlocked(uint8_t *buffer, uint32_t size, uint32_t fram_address);


/********************************************************************************
 * @brief  Saves the current SRAM compressed buffer to FRAM, updates the
 *         compression index table, and advances board_status pointers.
 *
 * @note   Saves the header (all fields before data[]) and JPEG data
 *         separately to the FRAM photo archive region. Updates
 *         compression_table[compression_count] with the new entry's
 *         address, total size, and valid flag, then increments
 *         compression_count, compressions_done, and updates
 *         fram_bytes_left. Must only be called after a successful
 *         CompressRawPhoto() — assumes COMPRESSED_BUFFER(0) holds a
 *         valid compression ready to persist.
 *
 * @retval CMD_ReturnStatus   CMD_OK on success. CMD_FRAM_FULL if the
 *                            archive region has insufficient space.
 *                            CMD_INDEX_FULL if the entry index is
 *                            exhausted.
 ********************************************************************************/
CMD_ReturnStatus SaveCompressionToFRAM(void);


/********************************************************************************
 * @brief  Writes the application flash image to a single FRAM backup copy
 *         and verifies it by reading back and recomputing the CRC.
 *
 * @param  app_start    Source flash address.
 * @param  app_size     Number of bytes to copy.
 * @param  dst          Destination FRAM address for this copy's image data.
 * @param  out_crc      Output: computed CRC32 of the written image.
 *
 * @retval CMD_ReturnStatus  CMD_OK on success, CMD_ERROR on readback mismatch.
 ********************************************************************************/
CMD_ReturnStatus WriteFirmwareCopy(uint32_t app_start, uint32_t app_size,
                                   uint32_t dst, uint32_t *out_crc);


/********************************************************************************
 * @brief  Shared implementation: loads and CRC-verifies board_status and
 *         compression_table from FRAM, applying safe defaults to either
 *         region independently on a CRC mismatch.
 *
 * @note   Internal helper used by both LoadBoardStatusFRAM() (cold boot)
 *         and RecoverBoardStatusFromFRAM() (runtime RAM-corruption
 *         recovery). Increments fram_load_failures on either region's
 *         CRC mismatch, distinct from fram_corruption_recovery which
 *         tracks write-retry successes in SaveBoardStatusFRAM().
 *
 * @note   Also reads the fw_backup_info for backup A and B.
 *
 * @retval None
 ********************************************************************************/
void RestoreBoardStatusFRAM(void);

#endif	/* __FRAM_H__ */
