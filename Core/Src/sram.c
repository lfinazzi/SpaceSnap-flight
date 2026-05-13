#include "sram.h"
#include "status.h"
#include "comms.h"

#include <stddef.h>


volatile raw_photo_t* raw_buffer_1 = NULL;
volatile raw_photo_t* raw_buffer_2 = NULL;
volatile raw_photo_t* raw_buffer_3 = NULL;
compressed_metadata_t* compressed_metadata = NULL;
uint16_t compressed_count = 0;
uint16_t* compressed_photos = NULL;

void AssignSRAMMemory(void)
{
	// Raw photo buffers — laid out sequentially in external NOR SRAM Bank 1
	raw_buffer_1 = (volatile raw_photo_t*) RAW_PHOTO_BASE_ADDRESS;
	raw_buffer_2 = (volatile raw_photo_t*)(RAW_PHOTO_BASE_ADDRESS + sizeof(raw_photo_t));
	raw_buffer_3 = (volatile raw_photo_t*)(RAW_PHOTO_BASE_ADDRESS + 2*sizeof(raw_photo_t));

	// Compressed metadata array: MAX_COMPRESSED_PHOTOS entries
	compressed_metadata = (compressed_metadata_t*) COMPRESSED_METADATA_BASE_ADDRESS;

	// Compressed photo data: contiguous region after metadata
	compressed_photos = (uint16_t*) COMPRESSED_DATA_BASE_ADDRESS;

	// Initialise write pointer in board status
	board_status.compressed_data_ptr = (uint32_t) COMPRESSED_DATA_BASE_ADDRESS;

	Log("SRAM buffers allocated\r\n");
}
