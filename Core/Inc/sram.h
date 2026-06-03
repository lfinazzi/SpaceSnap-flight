#ifndef __SRAM_H__
#define __SRAM_H__

#include "photo.h"
#include "main.h"

#include <stdint.h>

/* --- Raw frame pool ------------------------------------------------ */
#define RAW_PHOTO_BASE_ADDRESS          		(0x68000000U)				// NOR SRAM BANK 3 - Start of raw buffers
#define RAW_PHOTO_SIZE                  		sizeof(raw_photo_t)			// Full struct size: header + pixel data of raw photo
#define RAW_PHOTO_COUNT                 		(3U)
#define RAW_POOL_SIZE                   		(RAW_PHOTO_COUNT * RAW_PHOTO_SIZE)

/* Direct access macros — no pointer variable needed               */
#define RAW_BUFFER(n)   						((volatile raw_photo_t *)(RAW_PHOTO_BASE_ADDRESS + (n) * RAW_PHOTO_SIZE))
#define RAW_BUFFER_1    						RAW_BUFFER(0)
#define RAW_BUFFER_2    						RAW_BUFFER(1)
#define RAW_BUFFER_3    						RAW_BUFFER(2)

/* --- Compressed metadata pool -------------------------------------- */
#define MAX_COMPRESSED_PHOTOS           		(100U)						// Maximum number of compressions possible
#define COMPRESSED_METADATA_BASE_ADDRESS  		(RAW_PHOTO_BASE_ADDRESS + RAW_POOL_SIZE)		// Start of compressed metadata
#define COMPRESSED_METADATA_POOL_SIZE 			(MAX_COMPRESSED_PHOTOS * sizeof(compressed_metadata_t))

/* Direct access macro */
#define COMPRESSED_METADATA(n)  				((compressed_metadata_t *)(COMPRESSED_METADATA_BASE_ADDRESS + (n) * sizeof(compressed_metadata_t)))

/* --- Compressed data pool ------------------------------------------ */
#define COMPRESSED_DATA_BASE_ADDRESS			(COMPRESSED_METADATA_BASE_ADDRESS + COMPRESSED_METADATA_POOL_SIZE)

/* End of SRAM */
#define SRAM_END_ADDRESS                		(0x68400000U)

/* Remaining SRAM after raw pool and metadata pool */
#define COMPRESSED_POOL_SIZE					(SRAM_END_ADDRESS - COMPRESSED_DATA_BASE_ADDRESS)


/* ------------------------------------------------------------------ */
/*  Extern state variables (defined in photo.c)                       */
/* ------------------------------------------------------------------ */
extern uint16_t compressed_count;       // number of compressed images stored
extern uint8_t  *compressed_next;       // pointer to next free byte in
                                        // compressed data pool
                                        // advances after each compression


/********************************************************************************
 * @brief  Assigns fixed SRAM addresses to the pointers used for photo storage.
 *  	   Also check SRAM integrity writing and reading three memory values in
 *  	   compressed photo space.
 *
 * @note   Must be called once before any photo capture or buffer access.
 *         Pointers are mapped to static SRAM regions; no dynamic allocation
 *         is performed.
 *********************************************************************************/
void AssignSRAMMemory(void);


#endif
