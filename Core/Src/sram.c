#include "sram.h"
#include "status.h"
#include "comms.h"

#include <stddef.h>
#include <stdio.h>


uint16_t compressed_count;       // number of compressed images stored
uint8_t  *compressed_next;       // pointer to next free byte in

extern IWDG_HandleTypeDef hiwdg;

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

void DumpRawBuffer(uint8_t slot, uint32_t num_bytes)
{
    char log_buf[128];
    volatile raw_photo_t *buf = RAW_BUFFER(slot);
    uint8_t *data = (uint8_t *)&buf->data[0];

    sprintf(log_buf, "--- RAW BUFFER %d DUMP (%lu bytes) ---\r\n", slot, num_bytes);
    Log(log_buf);

    for (uint32_t i = 0; i < num_bytes; i += 32)
    {
        // Build entire line in one buffer
        char line[128] = {0};
        char tmp[8];

        // Address
        sprintf(line, "0x%08lX | ", (uint32_t)(data + i));

        // 32 bytes
        for (uint32_t j = 0; j < 32 && (i + j) < num_bytes; j++)
        {
            sprintf(tmp, "%02X ", data[i + j]);
            strcat(line, tmp);
        }

        strcat(line, "|\r\n");
        Log(line);

        HAL_IWDG_Refresh(&hiwdg);
        HAL_Delay(1);  // delay to prevent UART overflow for high data rates
    }

    Log("--- END DUMP ---\r\n");
}
