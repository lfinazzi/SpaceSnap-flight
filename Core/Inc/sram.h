#ifndef __SRAM_H__
#define __SRAM_H__

#include "photo.h"
#include "main.h"
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

#define END_OF_BUFFERS							((COMPRESSED_BUFFER_BASE_ADDRESS) + (COMPRESSED_POOL_SIZE))

/* End of SRAM */
#define SRAM_END_ADDRESS                		(0x68400000U)

/* Remaining SRAM after raw pool and metadata pool */
#define END_OF_BUFFERS							((COMPRESSED_BUFFER_BASE_ADDRESS) + (COMPRESSED_POOL_SIZE))

typedef char sram_storage_ok[
    ((int32_t)(SRAM_END_ADDRESS) - (int32_t)(END_OF_BUFFERS) > 0) ? 1 : -1		// Protects from SRAM overflow
];


/********************************************************************************
 * @brief  Tests SRAM by writing certain values to memory locations, reading
 * 		   them back and comparing
 *
 * @note   Must be called once before any photo capture or buffer access.
 *         Pointers are mapped to static SRAM regions; no dynamic allocation
 *         is performed.
 *********************************************************************************/
void TestSRAM(void);


// TODO: Comment, dumps in binary
void DumpRawBuffer(uint8_t slot, uint32_t num_bytes);


void DumpCompressedBuffer(uint8_t slot, uint32_t num_bytes);


void ResetVolatileStatus(void);

#endif
