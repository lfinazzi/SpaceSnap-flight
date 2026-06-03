#include "sram.h"
#include "status.h"
#include "comms.h"

#include <stddef.h>
#include <stdio.h>


uint16_t compressed_count;       // number of compressed images stored
uint8_t  *compressed_next;       // pointer to next free byte in

void AssignSRAMMemory(void)
{
	char     log_buf[64];
	uint8_t  ok = 1;

	Log("Initialising SRAM...\r\n");

	/* ---- 1. Integrity test ---------------------------------------- */
	/* Test start, middle and end of compressed pool only.             */
	/* Raw buffers and metadata region are not touched here to avoid   */
	/* corrupting valid data on a warm reset.                          */
	uint32_t test_addrs[3] = {
		COMPRESSED_DATA_BASE_ADDRESS,
		COMPRESSED_DATA_BASE_ADDRESS + (COMPRESSED_POOL_SIZE / 2),
		SRAM_END_ADDRESS - 2U
	};
	uint16_t test_vals[3] = {0xAA55, 0x1234, 0x55AA};

	/* Write test pattern */
	for (int i = 0; i < 3; i++) {
		*((volatile uint16_t *)test_addrs[i]) = test_vals[i];
	}

	/* Read back and verify */
	for (int i = 0; i < 3; i++) {
		uint16_t readback = *((volatile uint16_t *)test_addrs[i]);
		if (readback != test_vals[i]) {
			snprintf(log_buf, sizeof(log_buf),
					 "SRAM FAIL at 0x%08lX: wrote 0x%04X read 0x%04X\r\n",
					 test_addrs[i], test_vals[i], readback);
			Log(log_buf);
			ok = 0;
		}
	}

	/* Clear test locations regardless of result */
	for (int i = 0; i < 3; i++) {
		*((volatile uint16_t *)test_addrs[i]) = 0x0000;
	}

	if (!ok) {
		Log("SRAM integrity FAILED\r\n");
		return;
	}
	Log("SRAM integrity OK\r\n");

	/* ---- 2. Clear metadata pool ----------------------------------- */
	memset((void *)COMPRESSED_METADATA_BASE_ADDRESS,
		   0x00,
		   COMPRESSED_METADATA_POOL_SIZE);

	/* ---- 3. Initialise runtime state ------------------------------ */
	compressed_count                  = 0;
	board_status.compressed_data_ptr  = COMPRESSED_DATA_BASE_ADDRESS;

	snprintf(log_buf, sizeof(log_buf),
			 "SRAM OK — compressed pool: %lu bytes available\r\n",
			 (uint32_t)COMPRESSED_POOL_SIZE);
	Log(log_buf);

	return;
}
