#include "sram.h"
#include "status.h"
#include "comms.h"

#include <stddef.h>
#include <stdio.h>

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

	char log_buf[64];
	uint8_t ok = 1;

	// Test start, middle and end of SRAM
	uint32_t offsets[3] = {0x000000, 0x100000, 0x1FFFFF};
	uint16_t values[3]  = {0xAA55,   0x1234,   0x55AA  };

	for(int i = 0; i < 3; i++)
		compressed_photos[offsets[i]] = values[i];									// Writes three addresses in

	for(int i = 0; i < 3; i++)
	{
		if(compressed_photos[offsets[i]] != values[i])
		{
			sprintf(log_buf, "SRAM FAIL at 0x%06lX: wrote 0x%04X read 0x%04X\r\n",
					offsets[i], values[i], compressed_photos[offsets[i]]);			// Reads them to check if they are the same
			Log(log_buf);
			ok = 0;
		}
	}

	if(ok) Log("SRAM integrity OK\r\n");

}
