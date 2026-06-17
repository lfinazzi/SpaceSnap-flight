#ifndef __FRAM_H__
#define __FRAM_H__

#include <stdint.h>
#include "main.h"
#include "status.h"

#define BOARD_STATUS_START 				(0x000000)							// Address to save board status

#define COMPRESSION_TABLE_START 		((BOARD_STATUS_START) + (sizeof(board_status_t)))		// compressed space starts after status information, which starts at 0x00000000

#define PHOTO_DATA_START				((COMPRESSION_TABLE_START) + ((sizeof(compression_index_entry_t))*(MAX_COMPRESSED_PHOTOS)))		// Space for compressions

#define FIRMWARE_BACKUP_SIZE			(0x40000)		// 256 kB for FW backup image

#define END_OF_FRAM						(0x200000)		// 2 MB

#define FIRMWARE_BACKUP_START 			((END_OF_FRAM) - (FIRMWARE_BACKUP_SIZE))

// Pin PB12
#define FRAM_CS_LOW()   	HAL_GPIO_WritePin(CS_N_GPIO_Port, CS_N_Pin, GPIO_PIN_RESET)
#define FRAM_CS_HIGH()  	HAL_GPIO_WritePin(CS_N_GPIO_Port, CS_N_Pin, GPIO_PIN_SET)

#define FRAM_CMD_WREN   	(0x06U)  	// Write Enable
#define FRAM_CMD_WRITE  	(0x02U)  	// Write Memory
#define FRAM_CMD_READ   	(0x03U)  	// Read Memory


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
 * @brief  Reads part ID to verify init communication
 *
 * @param  id_buf   uint8_t to save id
 * 		   len		uint8_t to save id length
 *
 * @retval uint8_t   0 if ok, 1 if error
 ********************************************************************************/
uint8_t FRAM_ReadDeviceID(uint8_t *id_buf, uint8_t len);


/********************************************************************************
 * @brief  Reads device ID and compares with expected value. If it doesn't match,
 * 		   throws error
 *
 ********************************************************************************/
uint8_t TestFRAM(void);


/********************************************************************************
 * @brief  Writes board status to FRAM so it can be preserved in the case of
 *         power down.
 *
 * @note   Serializes the entire board_status_t struct into FRAM starting at
 *         FRAM_STATUS_BASE_ADDR, writing one byte at a time. Assumes the
 *         global board_status variable is up to date before calling.
 ********************************************************************************/
void SaveBoardStatusFRAM(void);


/********************************************************************************
 * @brief  Loads board status from FRAM into the global board_status struct
 *         on bootup.
 *
 * @note   Reads sizeof(board_status_t) bytes from FRAM starting at
 *         FRAM_STATUS_BASE_ADDR, restoring the last saved system state.
 *         After loading, increments boot_count and saves the updated struct
 *         back to FRAM to track the number of power cycles.
 ********************************************************************************/
void LoadBoardStatusFRAM(void);


// Erases FRAM writable space and initializes to zero. Also points the compression_ptr to PHOTO_DATA_START
// Doesn't touch FW backup space
void EraseFRAM(void);

/********************************************************************************
 * @brief  Save a buffer from SRAM into FRAM using a single burst write.
 *
 * For CY15B116x (16Mb, 2048K x 8) — 21-bit address space.
 * Opcode 0x02 (WRITE), 3-byte address, burst data with CS held LOW.
 * Address auto-increments; rolls over from 0x1FFFFF to 0x000000.
 *
 * @param  buffer       Pointer to source data (uint8_t array).
 * @param  size         Number of bytes to write.
 * @param  fram_address Starting FRAM address (must be <= 0x1FFFFF).
 */
 void SaveBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address);

 /**
  * @brief  Read a buffer from FRAM into SRAM using a single burst read.
  *
  * Per CY15B108QSN datasheet section (READ, opcode 0x03):
  *   - Single READ command with 3-byte address, then stream data in with CS held LOW
  *   - Internal address counter auto-increments after each byte
  *   - No latency/dummy cycles required for standard READ at SPI mode
  *
  * @param  buffer       Destination buffer in SRAM (uint8_t array).
  * @param  size         Number of bytes to read.
  * @param  fram_address Starting FRAM address (must be <= 0x1FFFFF for 16Mb).
  */
 void ReadBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address);


#endif
