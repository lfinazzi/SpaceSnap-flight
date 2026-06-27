/**
  ******************************************************************************
  * @file           : sram.c
  * @brief          : SRAM driver — external NOR SRAM interface for image storage
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "sram.h"
#include "status.h"
#include "comms.h"
#include "main.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

void TestSRAM(void)
{
	volatile uint32_t *start = (volatile uint32_t *)(RAW_PHOTO_BASE_ADDRESS);
	volatile uint32_t *end   = (volatile uint32_t *)(END_OF_BUFFERS);

	const uint32_t len = (uint32_t)(end - start);   /* in 32-bit words */

	board_status.sram_ok = 0;   // explicit, don't rely on prior state

	/* --- Pass 1: walking ones ---------------------------------------- */
	static const uint32_t patterns[] = {
		0xAAAAAAAAU,
		0xFFFFFFFFU,
	};

	for (uint32_t p = 0; p < sizeof(patterns) / sizeof(patterns[0]); ++p)
	{
		/* Write */
		for (uint32_t i = 0; i < len; ++i)
			start[i] = patterns[p];

		/* Read-back */
		for (uint32_t i = 0; i < len; ++i)
		{
			if (start[i] != patterns[p]){
				Log("SRAM integrity compromised\r\n");
				return;   /* sram_ok stays 0 */
			}

		}
	}

	Log("SRAM OK: patterns written and verified\r\n");
	board_status.sram_ok = 1;
	return;
}

// Dumped in hex, for fast debug
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

// Dumped in hex, for fast debug
void DumpCompressedBuffer(uint8_t slot, uint32_t num_bytes)
{
    char log_buf[128];
    volatile compressed_photo_t *buf = COMPRESSED_BUFFER(slot);
    uint8_t *data = (uint8_t *)&buf->data[0];

    sprintf(log_buf, "--- COMPRESSED BUFFER %d DUMP (%lu bytes) ---\r\n", slot, num_bytes);
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

void ResetVolatileStatus(void)
{
	// All buffers unoccupied
	memset(board_status.raw_buffer_occupied, 0x00, RAW_PHOTO_COUNT);
	board_status.compression_buffer_occupied = 0;

	// reset uptime
	board_status.uptime_session = 0;

	// reset last instruction metadata saved in FRAM
	board_status.last_instruction = 0;
	board_status.last_cmd_status = 0;
	memset(board_status.last_opcode, 0x00, OPCODE_SIZE);
	return;
}

