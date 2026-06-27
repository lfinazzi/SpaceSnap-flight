/**
  ******************************************************************************
  * @file           : sram.h
  * @brief          : SRAM driver interface — external NOR memory API
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __SRAM_H__
#define __SRAM_H__

#include "photo.h"
#include "stm32f2xx_hal.h"

#include <stdint.h>

/* --- Raw frame pool ------------------------------------------------ */
#define RAW_PHOTO_BASE_ADDRESS          		(0x68000000U)				// NOR SRAM BANK 3 - Start of raw buffers
#define RAW_PHOTO_SIZE                  		(sizeof(raw_photo_t))		// Full struct size: header + pixel data of raw photo
#define RAW_PHOTO_COUNT                 		(5U)
#define RAW_POOL_SIZE                   		(RAW_PHOTO_COUNT * RAW_PHOTO_SIZE)

/* Direct access macros — no pointer variable needed               */
#define RAW_BUFFER(n)   						((volatile raw_photo_t *)(RAW_PHOTO_BASE_ADDRESS + (n) * RAW_PHOTO_SIZE))
#define RAW_BUFFER_1    						RAW_BUFFER(0)
#define RAW_BUFFER_2    						RAW_BUFFER(1)
#define RAW_BUFFER_3    						RAW_BUFFER(2)
#define RAW_BUFFER_4    						RAW_BUFFER(3)
#define RAW_BUFFER_5    						RAW_BUFFER(4)

/* --- Compressed metadata pool -------------------------------------- */

#define COMPRESSED_PHOTO_COUNT                 	(1U)
#define COMPRESSION_SIZE                  		((sizeof(compressed_photo_t)))						// At least of the size or a raw picture + compressed metadata
#define COMPRESSED_POOL_SIZE                   	(COMPRESSED_PHOTO_COUNT * COMPRESSION_SIZE)

#define COMPRESSED_BUFFER_BASE_ADDRESS  		((RAW_PHOTO_BASE_ADDRESS) + (RAW_POOL_SIZE))		// Start of temp compressed metadata

/* Direct access macro */
#define COMPRESSED_BUFFER(n)  					((compressed_photo_t *)(COMPRESSED_BUFFER_BASE_ADDRESS + (n) * COMPRESSION_SIZE))
#define COMPRESSED_BUFFER_1    					COMPRESSED_BUFFER(0)

/* End of SRAM */
#define SRAM_END_ADDRESS                		(0x68400000U)

/* Remaining SRAM after raw pool and metadata pool */
#define END_OF_BUFFERS							((COMPRESSED_BUFFER_BASE_ADDRESS) + (COMPRESSED_POOL_SIZE))

typedef char sram_storage_ok[
    ((int32_t)(SRAM_END_ADDRESS) - (int32_t)(END_OF_BUFFERS) > 0) ? 1 : -1		// Protects from SRAM overflow
];


/********************************************************************************
 * @brief  Tests SRAM integrity by writing fixed patterns across the entire
 *         raw photo buffer region and reading them back.
 *
 * @note   Writes and verifies two patterns (0xAAAAAAAA and 0xFFFFFFFF)
 *         across the memory region from RAW_PHOTO_BASE_ADDRESS to
 *         END_OF_BUFFERS. Each pattern is written in full before readback
 *         to catch both stuck-bit and data-line faults. Explicitly sets
 *         board_status.sram_ok = 0 on entry; only sets it to 1 on full
 *         pass. This test is destructive -- all buffer contents are
 *         overwritten. Must be called before any photo capture or buffer
 *         access. Uses volatile pointers to prevent the compiler from
 *         optimizing away writes or reads.
 ********************************************************************************/
void TestSRAM(void);


/********************************************************************************
 * @brief  Hex-dumps the pixel data of a raw photo buffer over UART4 (debug).
 *
 * @note   Prints num_bytes bytes from RAW_BUFFER(slot)->data[] in hex,
 *         formatted as 32 bytes per row with the absolute SRAM address of
 *         each row. Kicks the IWDG and inserts a 1ms delay after each row
 *         to prevent UART overflow at high data rates. This function takes
 *         a significant amount of time for large buffers (full raw frame =
 *         L * H * 2 bytes). Called internally by CMD_DumpRaw().
 *
 * @param  slot       Raw buffer slot index (0 to RAW_PHOTO_COUNT-1).
 * @param  num_bytes  Number of bytes to dump from the start of data[].
 ********************************************************************************/
void DumpRawBuffer(uint8_t slot, uint32_t num_bytes);


/********************************************************************************
 * @brief  Hex-dumps the JPEG data of a compressed photo buffer over UART4
 *         (debug).
 *
 * @note   Prints num_bytes bytes from COMPRESSED_BUFFER(slot)->data[] in
 *         hex, formatted as 32 bytes per row with the absolute SRAM address
 *         of each row. Kicks the IWDG and inserts a 1ms delay after each
 *         row to prevent UART overflow at high data rates. Called internally
 *         by CMD_DumpCompressed().
 *
 * @param  slot       Compressed buffer slot index (always 0 currently).
 * @param  num_bytes  Number of bytes to dump, typically the actual JPEG size
 *                    reconstructed from compression->size_MSB/LSB.
 ********************************************************************************/
void DumpCompressedBuffer(uint8_t slot, uint32_t num_bytes);


/********************************************************************************
 * @brief  Resets all volatile fields in board_status to their default values.
 *
 * @note   Clears all raw buffer and compression buffer occupancy flags,
 *         resets uptime_session to 0, and clears last_instruction,
 *         last_cmd_status, and last_opcode[]. Should be called once on every
 *         boot before any command processing begins, since these fields are
 *         not meaningful across resets (SRAM does not persist across power
 *         cycles) and may contain stale values loaded from FRAM via
 *         LoadBoardStatusFRAM().
 ********************************************************************************/
void ResetVolatileStatus(void);

#endif
