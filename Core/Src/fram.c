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
