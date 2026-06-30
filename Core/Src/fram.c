/**
  ******************************************************************************
  * @file           : fram.c
  * @brief          : FRAM driver — persistent storage read/write via SPI
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "fram.h"
#include "status.h"
#include "main.h"
#include "protection.h"

#include <stdio.h>
#include <string.h>

// Pin PB12 — CS macros are implementation details, defined here not in the header
#define FRAM_CS_LOW()   HAL_GPIO_WritePin(CS_N_GPIO_Port, CS_N_Pin, GPIO_PIN_RESET)
#define FRAM_CS_HIGH()  HAL_GPIO_WritePin(CS_N_GPIO_Port, CS_N_Pin, GPIO_PIN_SET)

uint8_t FRAM_ReadDeviceID(uint8_t *id_buf, uint8_t len)
{
    uint8_t cmd[2] = {0x9F, 0x00};  // opcode + dummy byte

    FRAM_CS_LOW();
    HAL_StatusTypeDef s1 = HAL_SPI_Transmit(&hspi2, cmd, 2, HAL_MAX_DELAY);
    HAL_StatusTypeDef s2 = HAL_SPI_Receive(&hspi2, id_buf, len, HAL_MAX_DELAY);
    FRAM_CS_HIGH();

    return (s1 == HAL_OK && s2 == HAL_OK) ? 0 : 1;
}

uint8_t TestFRAM(void)
{
    uint8_t id_buf[4] = {0};
    uint8_t expected_id[4] = {0x51, 0x82, 0x06, 0x00};
    char log_buf[40];

    FRAM_ReadDeviceID(id_buf, 4);

    sprintf(log_buf, "FRAM ID: %02X %02X %02X %02X\r\n",
            id_buf[0], id_buf[1], id_buf[2], id_buf[3]);
    Log(log_buf);

    if (memcmp(id_buf, expected_id, 4) == 0) {
        Log("FRAM OK: start successful\r\n");
        return 1;
    } else {
        Log("FRAM ERROR: init failed\r\n");
        return 0;
    }
    return 0;
}

void SaveBoardStatusFRAM(void)
{
	if (!BoardStatusIntact() || !CompTableIntact()) {
		Log("RAM corrupt - restoring from FRAM\r\n");

		RecoverBoardStatusFromFRAM();
		return;
	}

	/* ---- board_status: write + verify with retry ---- */
	uint32_t bs_crc = CalculateCRC32((const uint8_t *)&board_status, sizeof(board_status_t));

	uint8_t bs_verified = 0;

	// In case a write failed, retry the write additional times before giving up
	for (uint8_t bs_attempt = 0; bs_attempt < FRAM_WRITE_RETRY; bs_attempt++) {
		SaveBufferFRAM((uint8_t*)&board_status, sizeof(board_status_t), BOARD_STATUS_START);
		SaveBufferFRAM((uint8_t*)&bs_crc, sizeof(uint32_t), BOARD_STATUS_CRC);

		uint32_t readback = 0;
		ReadBufferFRAM((uint8_t*)&readback, sizeof(uint32_t), BOARD_STATUS_CRC);

		if (readback == bs_crc) {
			bs_verified = 1;
			if (bs_attempt > 0) {
				board_status.fram_corruption_write_recovery++;   /* only count actual write recoveries */
			}
			break;
		}
		Log("FRAM board_status write verify failed - retrying\r\n");
	}

	if (!bs_verified) {
		Log("FRAM board_status write failed after 3 attempts\r\n");
		board_status.fram_ok = 0;
	}
	else {
		board_status.fram_ok = 1;		// Recovery was possible
	}

	/* ---- compression_table: write + verify with retry ---- */
	uint32_t ct_crc = CalculateCRC32((const uint8_t *)compression_table,
									  sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS);

	uint8_t ct_verified = 0;

	for (uint8_t ct_attempt = 0; ct_attempt < FRAM_WRITE_RETRY; ct_attempt++) {
		SaveBufferFRAM((uint8_t*)compression_table,
					   sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS,
					   COMPRESSION_TABLE_START);
		SaveBufferFRAM((uint8_t*)&ct_crc, sizeof(uint32_t), COMPRESSION_TABLE_CRC);

		uint32_t readback = 0;
		ReadBufferFRAM((uint8_t*)&readback, sizeof(uint32_t), COMPRESSION_TABLE_CRC);

		if (readback == ct_crc) {
			ct_verified = 1;
			if (ct_attempt > 0) {
				board_status.fram_corruption_write_recovery++;   /* only count actual write recoveries */
			}
			break;
		}
		Log("FRAM compression_table write verify failed - retrying\r\n");
	}

	if (!ct_verified) {
		Log("FRAM compression_table write failed after 3 attempts\r\n");
		board_status.fram_ok = 0;
	}
	else {
		board_status.fram_ok = 1;		// Recovery was possible
	}

}

void RestoreBoardStatusFromFRAM(void)
{
	/* ---- Load and verify board_status ---- */
	ReadBufferFRAM((uint8_t *)&board_status, sizeof(board_status_t), BOARD_STATUS_START);

	uint32_t stored_bs_crc = 0;
	ReadBufferFRAM((uint8_t *)&stored_bs_crc, sizeof(uint32_t), BOARD_STATUS_CRC);

	uint32_t computed_bs_crc = CalculateCRC32((const uint8_t *)&board_status,
											   sizeof(board_status_t));

	if (computed_bs_crc != stored_bs_crc) {
		Log("---------------------------------------------------\r\n");
		Log("FRAM BOARD STATUS CRC FAIL - USING DEFAULTS\r\n");
		Log("---------------------------------------------------\r\n");
		memset(&board_status, 0, sizeof(board_status_t));
		board_status.compression_ptr_address    		 = PHOTO_DATA_START;
		board_status.fram_bytes_left            		 = FIRMWARE_BACKUP_START - PHOTO_DATA_START;
		board_status.cam_params.black_threshold          = BLACK_THRESHOLD_DEFAULT;
		board_status.cam_params.sensor_analog_gain       = GAIN_ANALOG_DEFAULT;
		board_status.cam_params.sensor_digital_gain      = GAIN_DIGITAL_DEFAULT;
		board_status.cam_params.sensor_coarse_exposure   = EXPOSURE_COARSE_DEFAULT;
		board_status.cam_params.sensor_fine_exposure     = EXPOSURE_FINE_DEFAULT;
		board_status.delayed_params.num_photos           = BURST_NUM_PHOTOS;
		board_status.delayed_params.time_between_photos  = BURST_INTERVAL;
		board_status.delayed_params.perform_compressions = BURST_COMPRESSION;
		board_status.delayed_params.compression_quality  = BURST_COMPR_QUALITY;
		board_status.fram_ok = 1;	// Restore performed, healthy
		board_status.sram_ok = 1;	// Assumed was working, restore doesn't touch SRAM
		board_status.fram_corruption_defaulted++;	// Increment reset counter for FRAM corruption
	}

	/* ---- Load and verify compression_table ---- */
	ReadBufferFRAM((uint8_t *)compression_table,
					sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS,
					COMPRESSION_TABLE_START);

	uint32_t stored_ct_crc = 0;
	ReadBufferFRAM((uint8_t *)&stored_ct_crc, sizeof(uint32_t), COMPRESSION_TABLE_CRC);

	uint32_t computed_ct_crc = CalculateCRC32((const uint8_t *)compression_table,
											   sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS);

	if (computed_ct_crc != stored_ct_crc) {
		Log("---------------------------------------------------\r\n");
		Log("FRAM COMPRESSION TABLE CRC FAIL - USING DEFAULTS\r\n");
		Log("---------------------------------------------------\r\n");
		memset(&compression_table, 0,
			   sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS);
		board_status.fram_ok = 1;
		board_status.sram_ok = 1;		// Assumed was working, restore doesn't touch SRAM
		board_status.fram_corruption_defaulted++;	// Increment reset counter for FRAM corruption
	}
	else {
		/* Table CRC passed - recount valid entries in case board_status
		 * was reset to defaults above, so compression_count matches
		 * what is actually in the table */
		uint16_t count = 0;
		for (uint16_t i = 0; i < MAX_COMPRESSED_PHOTOS; i++) {
			if (compression_table[i].valid) count++;
		}
		board_status.compression_count = count;
	}

	// reads backup size and crc from FRAM
	ReadBufferFRAM((uint8_t *)&fw_backup_info.fw_backup_size, sizeof(uint32_t), FIRMWARE_BACKUP_START);
	ReadBufferFRAM((uint8_t *)&fw_backup_info.fw_backup_crc32, sizeof(uint32_t), FIRMWARE_BACKUP_START + sizeof(uint32_t));
	ReadBufferFRAM((uint8_t *)&fw_backup_info.fw_backup_version, sizeof(uint32_t), FIRMWARE_BACKUP_START + 2 * sizeof(uint32_t));

}


void LoadBoardStatusFRAM(void)
{
	uint8_t ret = TestFRAM();

	if(ret == 0) { // failed
        board_status.fram_ok = 0;	// FRAM can't be trusted
	}
	else { // success
		RestoreBoardStatusFromFRAM();
	}

	board_status.boot_count++;

	// This is to only load a delayed state after boot, but on a fault go to STATE_IDLE
	uint8_t restored_state = board_status.state;
	if (restored_state != STATE_DELAYED_PICTURE) {
	    restored_state = STATE_IDLE;
	}
	SetState(restored_state);

	CommitBoardStatus();			// Commits the shadow CRCs on boot
	CommitCompressionTable();
}

// might take a while
void EraseFRAM(void)
{
	Log("Erasing FRAM writable space...\r\n");

	uint8_t zero_chunk[256] = {0};
	uint32_t addr = 0x00;

	// Writes zero in FRAM upto FIRMWARE_BACKUP_START in chunks for less SPI transactions
	while (addr < FIRMWARE_BACKUP_START) {
	    uint32_t chunk_size = ((FIRMWARE_BACKUP_START - addr) < sizeof(zero_chunk))		// Protects the firmware region
	                          ? (FIRMWARE_BACKUP_START - addr)
	                          : sizeof(zero_chunk);

	    SaveBufferFRAM(zero_chunk, chunk_size, addr);
	    HAL_IWDG_Refresh(&hiwdg);    // kick IWDG once per 256-byte chunk instead of per byte

	    addr += chunk_size;
	}

	memset(&board_status, 0, sizeof(board_status_t));													// reset board status
	memset(&compression_table, 0, sizeof(compression_index_entry_t)*MAX_COMPRESSED_PHOTOS);				// reset compression table
	board_status.compression_ptr_address = PHOTO_DATA_START;		// point compression_ptr to the correct address (first address)
	board_status.fram_bytes_left = FIRMWARE_BACKUP_START - PHOTO_DATA_START;							// All bytes available

	SetState(STATE_IDLE);

	// Default cam parameter values
	board_status.cam_params.black_threshold = BLACK_THRESHOLD_DEFAULT;
	board_status.cam_params.sensor_analog_gain = GAIN_ANALOG_DEFAULT;
	board_status.cam_params.sensor_digital_gain = GAIN_DIGITAL_DEFAULT;
	board_status.cam_params.sensor_coarse_exposure = EXPOSURE_COARSE_DEFAULT;
	board_status.cam_params.sensor_fine_exposure = EXPOSURE_FINE_DEFAULT;

	// Default delayed photo burst parameter values
	board_status.delayed_params.num_photos = BURST_NUM_PHOTOS;
	board_status.delayed_params.time_between_photos = BURST_INTERVAL;
	board_status.delayed_params.perform_compressions = BURST_COMPRESSION;
	board_status.delayed_params.compression_quality = BURST_COMPR_QUALITY;

	board_status.fram_ok = 1;		// newly erased FRAM
	board_status.sram_ok = 1;   	// assumed working - erase doesn't affect SRAM

	CommitCompressionTable();
	CommitBoardStatus();

	Log("FRAM erase complete\r\n");
	return;
}

// To write compressed photos and status
void SaveBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address)
{
    uint8_t cmd[4];

    if (fram_address < BOARD_STATUS_START || fram_address + size > FIRMWARE_BACKUP_START) {
		Log("SaveBufferFRAM: address out of region bounds\r\n");
		board_status.fram_ok = 0;
		return;
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

void EraseCompressions(void)
{
	Log("Erasing compressions in memory...\r\n");		// Takes approximately 40 s

	uint8_t zero_chunk[256] = {0};
	uint32_t addr = PHOTO_DATA_START;

	// From PHOTO_DATA_START to start of FW backup, might take a while
	while (addr < FIRMWARE_BACKUP_START) {
		uint32_t chunk_size = ((FIRMWARE_BACKUP_START - addr) < sizeof(zero_chunk))		// Protects the firmware region
							  ? (FIRMWARE_BACKUP_START - addr)
							  : sizeof(zero_chunk);

		SaveBufferFRAM(zero_chunk, chunk_size, addr);
		HAL_IWDG_Refresh(&hiwdg);    // kick IWDG once per 256-byte chunk instead of per byte

		addr += chunk_size;
	}

	memset(&compression_table, 0, sizeof(compression_index_entry_t)*MAX_COMPRESSED_PHOTOS);				// reset compression table
	CommitCompressionTable();   /* re-seal after erase */

	board_status.compression_ptr_address = PHOTO_DATA_START;		// point compression_ptr to the correct address (first address)
	board_status.fram_bytes_left = FIRMWARE_BACKUP_START - board_status.compression_ptr_address;		// All bytes available
	board_status.compression_count = 0;		// no more compressions in memory
	CommitBoardStatus();   /* re-seal after erase */

	Log("Compressions erased\r\n");
	return;
}

// To write firmware backup. Only function that can touch it
void SaveFRAM_Unlocked(uint8_t *buffer, uint32_t size, uint32_t fram_address)
{
    uint8_t cmd[4];

    if (fram_address < FIRMWARE_BACKUP_START) {
		Log("SaveFRAM_Unlocked: address below FIRMWARE_BACKUP_START\r\n");
		board_status.fram_ok = 0;
		return;
	}
	if (fram_address + size > END_OF_FRAM) {
		Log("SaveFRAM_Unlocked: address would exceed END_OF_FRAM\r\n");
		board_status.fram_ok = 0;
		return;
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

CMD_ReturnStatus SaveCompressionToFRAM(void)
{

	/* Sanity check compression_ptr_address before trusting it.
	 * Corruption here could overwrite board_status, compression table,
	 * or firmware backup region. */
	if (board_status.compression_ptr_address < PHOTO_DATA_START ||
		board_status.compression_ptr_address >= FIRMWARE_BACKUP_START) {
		Log("compression_ptr_address out of bounds - aborting\r\n");
		board_status.compression_ptr_address = PHOTO_DATA_START;
		board_status.fram_bytes_left = FIRMWARE_BACKUP_START - PHOTO_DATA_START;
		board_status.fram_ok = 0;
		CommitBoardStatus();
		return CMD_ERROR;
	}

    compressed_photo_t *compression = COMPRESSED_BUFFER(0);
    uint32_t jpeg_size = ((uint32_t)compression->size_MSB << 16) | compression->size_LSB;
	uint32_t header_size = sizeof(compressed_photo_t) - sizeof(compression->data);
	uint32_t total_size  = header_size + jpeg_size;

	if (board_status.compression_ptr_address + total_size > FIRMWARE_BACKUP_START) {		// Checks if FRAM will overflow allowed space (before FW backup start)
		Log("FRAM full — cannot save compression.\r\n");
		return CMD_FRAM_FULL;
	}

	if (board_status.compression_count >= MAX_COMPRESSED_PHOTOS) {
		Log("Compression index full.\r\n");
		return CMD_INDEX_FULL;
	}

	uint16_t idx = board_status.compression_count;
	compression_table[idx].fram_address = board_status.compression_ptr_address;
	compression_table[idx].total_size   = total_size;
	compression_table[idx].valid        = 1;

	CommitCompressionTable();   /* re-seal after legitimate change */

	char log_buf[96];
	sprintf(log_buf, "Index[%u].fram_address = 0x%06lX\r\n", idx, compression_table[idx].fram_address);
	Log(log_buf);

	// Save header (everything before data[])
	SaveBufferFRAM((uint8_t *)compression, header_size, board_status.compression_ptr_address);
	board_status.compression_ptr_address += header_size;

	// Save JPEG data
	SaveBufferFRAM(compression->data, jpeg_size, board_status.compression_ptr_address);
	board_status.compression_ptr_address += jpeg_size;

	if (board_status.compression_ptr_address <= FIRMWARE_BACKUP_START) {
		board_status.fram_bytes_left = FIRMWARE_BACKUP_START - board_status.compression_ptr_address;
	} else {
		board_status.fram_bytes_left = 0;   // shouldn't happen, but avoids unsigned underflow if it does
		Log("WARNING: compression_ptr_address exceeds FIRMWARE_BACKUP_START!\r\n");
	}

	// Increment number of compressions in memory by one
	board_status.compression_count++;		// compressions in memory currently
	board_status.compressions_done++;		// total compressions done
	board_status.compression_buffer_occupied = 1;

    return CMD_OK;
}

CMD_ReturnStatus WriteFirmwareCopy(uint32_t app_start, uint32_t app_size,
                                   uint32_t dst, uint32_t *out_crc)
{
    uint8_t  chunk[256];
    uint32_t crc      = 0xFFFFFFFF;
    uint32_t src       = app_start;
    uint32_t remaining = app_size;

    while (remaining > 0) {
        uint32_t n = (remaining > sizeof(chunk)) ? sizeof(chunk) : remaining;
        memcpy(chunk, (uint8_t *)src, n);
        SaveFRAM_Unlocked(chunk, n, dst);

        for (uint32_t i = 0; i < n; i++) {
            crc ^= chunk[i];
            for (int b = 0; b < 8; b++)
                crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
        }

        src += n; dst += n; remaining -= n;
        HAL_IWDG_Refresh(&hiwdg);
    }
    *out_crc = ~crc;

    /* Readback verify */
    uint32_t verify_crc = 0xFFFFFFFF;
    uint32_t verify_src = dst - app_size;   /* recompute start of this copy */
    remaining = app_size;
    while (remaining > 0) {
        uint32_t n = (remaining > sizeof(chunk)) ? sizeof(chunk) : remaining;
        ReadBufferFRAM(chunk, n, verify_src);
        for (uint32_t i = 0; i < n; i++) {
            verify_crc ^= chunk[i];
            for (int b = 0; b < 8; b++)
                verify_crc = (verify_crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(verify_crc & 1)));
        }
        verify_src += n; remaining -= n;
        HAL_IWDG_Refresh(&hiwdg);
    }
    verify_crc = ~verify_crc;

    return (verify_crc == *out_crc) ? CMD_OK : CMD_ERROR;
}

void RecoverBoardStatusFromFRAM(void)
{
    RestoreBoardStatusFromFRAM();   /* shared load+verify logic */

    /* Same state restore policy as LoadBoardStatusFRAM(): only
	 * STATE_DELAYED_PICTURE survives, everything else goes to IDLE.
	 * Duplicated here intentionally rather than shared, since FRAM is
	 * the trustworthy source regardless of whether this recovery was
	 * triggered by a cold boot or a runtime RAM corruption event. */

    // This is the only way to recover this state
	uint8_t restored_state = board_status.state;
	if (restored_state != STATE_DELAYED_PICTURE) {
		restored_state = STATE_IDLE;
	}
	SetState(restored_state);

    CommitBoardStatus();
    CommitCompressionTable();

    board_status.ram_corruption_recovery++;
}
