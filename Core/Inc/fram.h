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
#include "main.h"
#include "status.h"

#define BOARD_STATUS_START 				(0x000000)													// Address to save board status

#define COMPRESSION_TABLE_START 		((BOARD_STATUS_START) + (sizeof(board_status_t)))			// compressed space starts after status information, which starts at 0x00000000

#define PHOTO_DATA_START				((COMPRESSION_TABLE_START) + ((sizeof(compression_index_entry_t))*(MAX_COMPRESSED_PHOTOS)))		// Space for compressions

#define FIRMWARE_BACKUP_SIZE			(0x40000)		// 256 kB for FW backup image
#define FIRMWARE_IMAGE_SIZE				((FIRMWARE_BACKUP_SIZE) - sizeof(fw_backup_info_t))			// ~256 kB for FW backup image

#define END_OF_FRAM						(0x200000)		// 2 MB

#define FIRMWARE_BACKUP_START 			((END_OF_FRAM) - (FIRMWARE_BACKUP_SIZE))
#define FIRMWARE_IMAGE_START            ((FIRMWARE_BACKUP_START) + (sizeof(fw_backup_info_t)))

// Pin PB12
#define FRAM_CS_LOW()   				HAL_GPIO_WritePin(CS_N_GPIO_Port, CS_N_Pin, GPIO_PIN_RESET)
#define FRAM_CS_HIGH()  				HAL_GPIO_WritePin(CS_N_GPIO_Port, CS_N_Pin, GPIO_PIN_SET)

#define FRAM_CMD_WREN   				(0x06U)  	// Write Enable
#define FRAM_CMD_WRITE  				(0x02U)  	// Write Memory
#define FRAM_CMD_READ   				(0x03U)  	// Read Memory


/********************************************************************************
 * @brief  Writes a single byte to the FRAM at the specified address.
 *
 * @note   Issues a Write Enable (WREN) command before the write operation,
 *         as required by the CY15B108QSN protocol. The address is 24-bit,
 *         transmitted MSB first. CS is asserted and deasserted around each
 *         SPI transaction.
 *
 * @param  addr   24-bit memory address to write to.
 * @param  data   Byte value to write.
 ********************************************************************************/
void FRAM_WriteByte(uint32_t addr, uint8_t data);


/********************************************************************************
 * @brief  Reads a single byte from the FRAM at the specified address.
 *
 * @note   Transmits the READ opcode followed by the 24-bit address MSB first,
 *         then clocks in one byte of data. CS is asserted for the full
 *         transaction and deasserted on completion.
 *
 * @param  addr   24-bit memory address to read from.
 *
 * @retval uint8_t   Byte value read from the specified address.
 ********************************************************************************/
uint8_t FRAM_ReadByte(uint32_t addr);


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
 * @note   Serializes the entire board_status_t struct into FRAM starting at
 *         BOARD_STATUS_START, then serializes the compression table
 *         (MAX_COMPRESSED_PHOTOS entries of compression_index_entry_t) starting
 *         at COMPRESSION_TABLE_START, writing one byte at a time via
 *         FRAM_WriteByte(). Assumes both global variables are up to date
 *         before calling.
 ********************************************************************************/
void SaveBoardStatusFRAM(void);


/********************************************************************************
 * @brief  Loads board_status and compression_table from FRAM on bootup.
 *
 * @note   Calls TestFRAM() first. On success, reads sizeof(board_status_t)
 *         bytes from BOARD_STATUS_START into board_status,
 *         MAX_COMPRESSED_PHOTOS * sizeof(compression_index_entry_t) bytes
 *         from COMPRESSION_TABLE_START into compression_table (one byte at
 *         a time via FRAM_ReadByte()), and fw_backup_size + fw_backup_crc32
 *         from FIRMWARE_BACKUP_START into fw_backup_info via
 *         ReadBufferFRAM(). Sets board_status.fram_ok accordingly.
 *
 *         On TestFRAM() failure, board_status is not loaded from FRAM but
 *         execution continues with the default zero-initialized state.
 *
 *         boot_count is incremented and SaveBoardStatusFRAM() is called
 *         unconditionally after the load attempt, regardless of whether
 *         TestFRAM() passed or failed.
 ********************************************************************************/
void LoadBoardStatusFRAM(void);


/********************************************************************************
 * @brief  Erases the entire FRAM writable region and resets all system state.
 *
 * @note   Writes 0x00 to every byte from address 0x00 to FIRMWARE_BACKUP_START
 *         via FRAM_WriteByte(). The IWDG is kicked on every byte write to
 *         prevent a watchdog reset during the erase. This operation takes
 *         approximately 40 seconds. Does not touch the firmware backup region
 *         (FIRMWARE_BACKUP_START and above).
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
 * @note   Writes 0x00 to every byte from PHOTO_DATA_START to
 *         FIRMWARE_BACKUP_START via FRAM_WriteByte(). The IWDG is kicked on
 *         every byte write to prevent a watchdog reset during the erase.
 *         This operation takes approximately 40 seconds.
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


#endif
