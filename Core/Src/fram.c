#include "fram.h"

extern SPI_HandleTypeDef hspi2;

void FRAM_WriteByte(uint32_t addr, uint8_t data)
{
    uint8_t buf[5];

    // First send WREN
    FRAM_CS_LOW();
    buf[0] = FRAM_CMD_WREN;
    HAL_SPI_Transmit(&hspi2, buf, 1, HAL_MAX_DELAY);
    FRAM_CS_HIGH();

    // Then write: opcode + 3 addr bytes + data
    FRAM_CS_LOW();
    buf[0] = FRAM_CMD_WRITE;
    buf[1] = (addr >> 16) & 0xFF;  // MSB
    buf[2] = (addr >>  8) & 0xFF;
    buf[3] = (addr >>  0) & 0xFF;  // LSB
    buf[4] = data;
    HAL_SPI_Transmit(&hspi2, buf, 5, HAL_MAX_DELAY);
    FRAM_CS_HIGH();
}

uint8_t FRAM_ReadByte(uint32_t addr)
{
    uint8_t buf[4];
    uint8_t rxdata = 0;

    FRAM_CS_LOW();
    buf[0] = FRAM_CMD_READ;
    buf[1] = (addr >> 16) & 0xFF;
    buf[2] = (addr >>  8) & 0xFF;
    buf[3] = (addr >>  0) & 0xFF;
    HAL_SPI_Transmit(&hspi2, buf, 4, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi2, &rxdata, 1, HAL_MAX_DELAY);
    FRAM_CS_HIGH();

    return rxdata;
}

void TestFRAM(void)
{
	char log_buf[64];
	uint8_t ok = 1;

	uint32_t addrs[3] = {0x000000, 0x080000, 0x0FFFFF};  // start, mid, end of 1MB
	uint8_t  vals[3]  = {0xA5,     0x5A,     0xF0    };

	for(int i = 0; i < 3; i++)
		FRAM_WriteByte(addrs[i], vals[i]);

	for(int i = 0; i < 3; i++)
	{
		uint8_t r = FRAM_ReadByte(addrs[i]);
		if(r != vals[i])
		{
			sprintf(log_buf, "FRAM FAIL at 0x%06lX: wrote 0x%02X read 0x%02X\r\n",
					addrs[i], vals[i], r);
			Log(log_buf);
			ok = 0;
			board_status.fram_ok = 0;		// FRAM init not ok
		}
	}

	if(ok){
		Log("FRAM OK: start/mid/end passed\r\n");
		board_status.fram_ok = 1;		// FRAM init ok
	}
}

void SaveBoardStatusFRAM(void)
{
    uint8_t *p = (uint8_t *)&board_status;

    for(uint32_t i = 0; i < PHOTO_DATA_START; i++)
    {
        FRAM_WriteByte(0x0 + i, p[i]);
    }
}


void LoadBoardStatusFRAM(void)
{
	// Check if FRAM has been initialized before (or first boot)
	if(FRAM_ReadByte(FRAM_MAGIC_ADDR) != FRAM_MAGIC_VAL)			// Tries to read a m
	{
		// First boot — zero out struct and save
		Log("First boot detected, initializing FRAM...\r\n");
		memset(&board_status, 0, PHOTO_DATA_START);
		board_status.compression_ptr_address = PHOTO_DATA_START;
		FRAM_WriteByte(FRAM_MAGIC_ADDR, FRAM_MAGIC_VAL);			// writes a value in last position to flag that this is not first boot
	}
	else
	{
		// Normal boot — load saved state
		uint8_t *p = (uint8_t *)&board_status;
		for(uint32_t i = 0; i < PHOTO_DATA_START; i++)
			p[i] = FRAM_ReadByte(0x00 + i);
	}

    // Increment boot count and save back
    board_status.boot_count++;

    SaveBoardStatusFRAM();
}

void EraseFRAMOnNextBoot(void)
{
	FRAM_WriteByte(FRAM_MAGIC_ADDR, 0x00);
	return;
}

/**
 * @brief  Save a buffer from SRAM into FRAM using a single burst write.
 *
 * For CY15B116x (16Mb, 2048K x 8) — 21-bit address space.
 * Opcode 0x02 (WRITE), 3-byte address, burst data with CS held LOW.
 * Address auto-increments; rolls over from 0x1FFFFF to 0x000000.
 *
 * @param  buffer       Pointer to source data (uint8_t array).
 * @param  size         Number of bytes to write.
 * @param  fram_address Starting FRAM address (must be <= 0x1FFFFF).
 */
void SaveBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address)
{
    uint8_t cmd[4];

    if (fram_address + size > 0x200000) {
        return;  // would wrap past 16Mb boundary
    }

    /* Write Enable Latch */
    FRAM_CS_LOW();
    cmd[0] = FRAM_CMD_WREN;
    HAL_SPI_Transmit(&hspi2, cmd, 1, HAL_MAX_DELAY);
    FRAM_CS_HIGH();

    /* WRITE command + 21-bit address, then burst data */
    FRAM_CS_LOW();
    cmd[0] = FRAM_CMD_WRITE;
    cmd[1] = (fram_address >> 16) & 0xFF;   // A[20:16]
    cmd[2] = (fram_address >>  8) & 0xFF;   // A[15:8]
    cmd[3] = (fram_address >>  0) & 0xFF;   // A[7:0]
    HAL_SPI_Transmit(&hspi2, cmd, 4, HAL_MAX_DELAY);

    HAL_SPI_Transmit(&hspi2, buffer, size, HAL_MAX_DELAY);

    FRAM_CS_HIGH();
}


void ReadBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address)
{
    uint8_t cmd[4];

    FRAM_CS_LOW();
    cmd[0] = FRAM_CMD_READ;
    cmd[1] = (fram_address >> 16) & 0xFF;   // A[20:16]
    cmd[2] = (fram_address >>  8) & 0xFF;   // A[15:8]
    cmd[3] = (fram_address >>  0) & 0xFF;   // A[7:0]
    HAL_SPI_Transmit(&hspi2, cmd, 4, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi2, buffer, size, HAL_MAX_DELAY);
    FRAM_CS_HIGH();
}
