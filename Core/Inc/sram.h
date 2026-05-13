#ifndef __SRAM_H__
#define __SRAM_H__

#include "photo.h"

#include <stdint.h>

#define RAW_PHOTO_BASE_ADDRESS 		  	 		(0x60000000U)									// NOR SRAM BANK 1 - Start of raw buffers
#define RAW_PHOTO_SIZE 		  	 				sizeof(raw_photo_t)								// Full struct size: header + pixel data of raw photo

#define MAX_COMPRESSED_PHOTOS			 		(100U)											// Maximum number of compressions possible
#define COMPRESSED_METADATA_BASE_ADDRESS 		(RAW_PHOTO_BASE_ADDRESS + 3*RAW_PHOTO_SIZE)		// Start of compressed metadata

// Start of compressed photo space
#define COMPRESSED_DATA_BASE_ADDRESS 			((COMPRESSED_METADATA_BASE_ADDRESS) + (MAX_COMPRESSED_PHOTOS * sizeof(compressed_metadata_t)))


extern volatile raw_photo_t* raw_buffer_1;					// Raw photo buffer number 1
extern volatile raw_photo_t* raw_buffer_2;					// Raw photo buffer number 2
extern volatile raw_photo_t* raw_buffer_3;					// Raw photo buffer number 3
extern compressed_metadata_t* compressed_metadata;			// Compressed metadata pointer
extern uint16_t compressed_count;							// Number of compressions in memory

extern uint16_t *compressed_photos;							// Pointer to first EMPTY compressed space, will move on compression


/********************************************************************************
 * @brief  Assigns fixed SRAM addresses to the pointers used for photo storage.
 *
 * @note   Must be called once before any photo capture or buffer access.
 *         Pointers are mapped to static SRAM regions; no dynamic allocation
 *         is performed.
 *********************************************************************************/
void AssignSRAMMemory(void);

#endif
