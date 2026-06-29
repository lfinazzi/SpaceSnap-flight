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

void FRAM_WriteByte(uint32_t addr, uint8_t data)
{
    uint8_t buf[5];

    if (addr >= END_OF_FRAM) {
		Log("FRAM_WriteByte: address out of bounds\r\n");
		return;
	}

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

    if (addr >= END_OF_FRAM) {
		Log("FRAM_ReadByte: address out of bounds\r\n");
		return 0xFF;    /* return erased value */
	}

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

		LoadBoardStatusFRAM();
		board_status.ram_corruption_recovery++;
		return;
	}

	/* Write board_status */
	uint8_t *p = (uint8_t *)&board_status;
	for (uint32_t i = 0; i < sizeof(board_status_t); i++)
		FRAM_WriteByte(BOARD_STATUS_START + i, p[i]);

	/* Write board_status CRC */
	uint32_t bs_crc = CalculateCRC32((const uint8_t *)&board_status,
									  sizeof(board_status_t));
	uint8_t *bs_crc_p = (uint8_t *)&bs_crc;
	for (uint32_t i = 0; i < sizeof(uint32_t); i++)
		FRAM_WriteByte(BOARD_STATUS_CRC + i, bs_crc_p[i]);

	/* Write compression_table */
	uint8_t *pc = (uint8_t *)&compression_table;
	for (uint32_t i = 0; i < sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS; i++)
		FRAM_WriteByte(COMPRESSION_TABLE_START + i, pc[i]);

	/* Write compression_table CRC */
	uint32_t ct_crc = CalculateCRC32((const uint8_t *)compression_table,
									  sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS);
	uint8_t *ct_crc_p = (uint8_t *)&ct_crc;
	for (uint32_t i = 0; i < sizeof(uint32_t); i++)
		FRAM_WriteByte(COMPRESSION_TABLE_CRC + i, ct_crc_p[i]);
}


void LoadBoardStatusFRAM(void)
{
	uint8_t ret = TestFRAM();

	if(ret == 0)	// failed
	{
        board_status.fram_ok = 0;
		/* continue */
	}
	else {			// success

		// Normal boot — load saved state
		uint8_t *p = (uint8_t *)&board_status;
		for(uint32_t i = 0; i < sizeof(board_status_t); i++)
			p[i] = FRAM_ReadByte(BOARD_STATUS_START + i);

		uint32_t stored_bs_crc = 0;
		uint8_t *bs_crc_p = (uint8_t *)&stored_bs_crc;
		for (uint32_t i = 0; i < sizeof(uint32_t); i++)
			bs_crc_p[i] = FRAM_ReadByte(BOARD_STATUS_CRC + i);



		uint32_t computed_bs_crc = CalculateCRC32((const uint8_t *)&board_status,
		                                                   sizeof(board_status_t));

		if (computed_bs_crc != stored_bs_crc) {
			Log("FRAM board_status CRC fail - using defaults\r\n");
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
			board_status.fram_ok = 0;
		}
		else
			board_status.fram_ok = 1;

		uint8_t *pc = (uint8_t *)&compression_table;
		for(uint32_t i = 0; i < sizeof(compression_index_entry_t)*MAX_COMPRESSED_PHOTOS; i++)
			pc[i] = FRAM_ReadByte(COMPRESSION_TABLE_START + i);

		uint32_t stored_ct_crc = 0;
		uint8_t *ct_crc_p = (uint8_t *)&stored_ct_crc;
		for (uint32_t i = 0; i < sizeof(uint32_t); i++)
			ct_crc_p[i] = FRAM_ReadByte(COMPRESSION_TABLE_CRC + i);

		uint32_t computed_ct_crc = CalculateCRC32((const uint8_t *)compression_table,
												   sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS);

		if (computed_ct_crc != stored_ct_crc) {
			Log("FRAM compression_table CRC fail - clearing table\r\n");
			memset(&compression_table, 0,
				   sizeof(compression_index_entry_t) * MAX_COMPRESSED_PHOTOS);
			board_status.fram_ok = 0;
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
		ReadBufferFRAM((uint8_t *)&fw_backup_info.fw_backup_version, sizeof(uint32_t), FIRMWARE_BACKUP_START + 2*sizeof(uint32_t));

	}

	board_status.boot_count++;

	SetState(board_status.state);

	CommitBoardStatus();			// Commits the shadow CRCs on boot
	CommitCompressionTable();
}

// might take a while
void EraseFRAM(void)
{
	Log("Erasing FRAM writable space... might take a while (approx. 40 s)\r\n");
	// From 0 to start of FW backup, might take a while
	for (uint32_t addr = 0x00; addr < FIRMWARE_BACKUP_START; addr++) {
		FRAM_WriteByte(addr, 0x00);
		HAL_IWDG_Refresh(&hiwdg);		// kick IWDG to avoid reset

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

	CommitCompressionTable();
	CommitBoardStatus();

	Log("FRAM erase complete\r\n");
	return;
}

void SaveBufferFRAM(uint8_t *buffer, uint32_t size, uint32_t fram_address)
{
    uint8_t cmd[4];

    if (fram_address < PHOTO_DATA_START) {
		Log("SaveBufferFRAM: address below PHOTO_DATA_START\r\n");
		board_status.fram_ok = 0;
		return;
	}
	if (fram_address + size > FIRMWARE_BACKUP_START) {
		Log("SaveBufferFRAM: address would overflow into backup region\r\n");
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
	Log("Erasing compressions in memory... might take a while (approx. 40 s)\r\n");		// Takes approximately 40 s
	// From PHOTO_DATA_START to start of FW backup, might take a while
	for (uint32_t addr = PHOTO_DATA_START; addr < FIRMWARE_BACKUP_START; addr++) {
		FRAM_WriteByte(addr, 0x00);
		HAL_IWDG_Refresh(&hiwdg);		// kick IWDG to avoid reset

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

void SaveFRAM_Unlocked(uint8_t *buffer, uint32_t size, uint32_t fram_address)
{
    uint8_t cmd[4];

    if (fram_address < FIRMWARE_IMAGE_START) {
		Log("SaveFRAM_Unlocked: address below FIRMWARE_IMAGE_START\r\n");
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
