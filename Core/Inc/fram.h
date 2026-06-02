#ifndef __FRAM_H__
#define __FRAM_H__

#include <stdint.h>
#include "main.h"
#include "status.h"

#define PHOTO_DATA_START 	sizeof(board_status_t)			// compressed space starts after status information, which starts at 0x00000000

#define FRAM_MAGIC_ADDR  0x0FFFFE   						// last 2 bytes of FRAM
#define FRAM_MAGIC_VAL   0xAB

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
 * @brief  Writes and reads back a test byte to verify FRAM communication.
 *
 * @note   Writes 0xA5 to address 0x000010 and reads it back. Logs OK or FAIL
 *         with both the written and read values via Log() (debug UART4).
 ********************************************************************************/
void TestFRAM(void);

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

// TODO: Function to erase FRAM, set everything to zero

#endif
