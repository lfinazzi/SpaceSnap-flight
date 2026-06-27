/**
  ******************************************************************************
  * @file           : status.c
  * @brief          : System status management — telemetry logging and board state
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "status.h"
#include "comms.h"
#include "main.h"

// Saves current board status
board_status_t board_status = {0};
compression_index_entry_t compression_table[MAX_COMPRESSED_PHOTOS] = {0};		// Compression table in FRAM
fw_backup_info_t fw_backup_info;		// unmodified on init on purpose, nothing can write this value except CMD_BackupFirmware()


void UpdateStatus(void)
{
	board_status.uptime_session = HAL_GetTick();		// Update uptime
	Read_MCU_ADC_Vals();								// Read Temperature and VREFINT values
}

void LogBoardStatus(void)
{
    char log_buf[96];

    sprintf(log_buf, "Boot #%lu | Total uptime [s]: %lu | Photos: %u | Compressions: %u\r\n",
            board_status.boot_count,
            board_status.uptime_total,
            board_status.photos_taken,
            board_status.compressions_done);
    Log(log_buf);

    sprintf(log_buf, "FRAM/SRAM ok: %u/%u\r\n",
            board_status.fram_ok,
            board_status.sram_ok);
    Log(log_buf);
}

void LogBoardStatusFull(void)
{

	char log_buf[96];

	// Power-on / uptime info
	sprintf(log_buf, "uptime_session [ms]: %lu\r\n", board_status.uptime_session);
	Log(log_buf);

	sprintf(log_buf, "uptime_total [s]: %lu\r\n", board_status.uptime_total + board_status.uptime_session / 1000);		// uptime_total is only calculated on boot
	Log(log_buf);

	sprintf(log_buf, "boot_count: %lu\r\n", board_status.boot_count);
	Log(log_buf);

	sprintf(log_buf, "requested_power_downs: %u\r\n", board_status.requested_power_downs);
	Log(log_buf);

	// Reset cause counters
	sprintf(log_buf, "iwdg_reset_count: %u\r\n", board_status.iwdg_reset_count);
	Log(log_buf);

	sprintf(log_buf, "lowpwr_reset_count: %u\r\n", board_status.lowpwr_reset_count);
	Log(log_buf);

	sprintf(log_buf, "sftw_reset_count: %u\r\n", board_status.sftw_reset_count);
	Log(log_buf);

	sprintf(log_buf, "por_reset_count: %u\r\n", board_status.por_reset_count);
	Log(log_buf);

	sprintf(log_buf, "pin_reset_count: %u\r\n", board_status.pin_reset_count);
	Log(log_buf);

	sprintf(log_buf, "unk_reset_count: %u\r\n", board_status.unk_reset_count);
	Log(log_buf);

	sprintf(log_buf, "last_reset_cause: %u\r\n", board_status.last_reset_cause);
	Log(log_buf);

	// Memory init status
	sprintf(log_buf, "fram_ok: %u\r\n", board_status.fram_ok);
	Log(log_buf);

	sprintf(log_buf, "sram_ok: %u\r\n", board_status.sram_ok);
	Log(log_buf);

	// Last command info
	sprintf(log_buf, "last_instruction: 0x%02X\r\n", board_status.last_instruction);
	Log(log_buf);

	sprintf(log_buf, "last_cmd_status: 0x%02X\r\n", board_status.last_cmd_status);
	Log(log_buf);

	int pos = sprintf(log_buf, "last_opcode: ");
	for (int i = 0; i < OPCODE_SIZE; i++) {
		pos += sprintf(log_buf + pos, "%02X ", board_status.last_opcode[i]);
	}
	sprintf(log_buf + pos, "\r\n");
	Log(log_buf);

	// Operation counters
	sprintf(log_buf, "photos_taken: %u\r\n", board_status.photos_taken);
	Log(log_buf);

	sprintf(log_buf, "compressions_done: %u\r\n", board_status.compressions_done);
	Log(log_buf);

	sprintf(log_buf, "images_rejected_black: %u\r\n", board_status.images_rejected_black);
	Log(log_buf);

	// FRAM compression tracking
	sprintf(log_buf, "compression_ptr_address: 0x%06lX\r\n", board_status.compression_ptr_address);
	Log(log_buf);

	sprintf(log_buf, "compression_count_in_memory: %u\r\n", board_status.compression_count);
	Log(log_buf);

	sprintf(log_buf, "fram_bytes_left: %lu\r\n", board_status.fram_bytes_left);
	Log(log_buf);

	// SRAM buffer occupancy
	for (uint8_t i = 0; i < RAW_PHOTO_COUNT; i++){
		sprintf(log_buf, "raw_buffer_%u_occupied: %u\r\n", i+1, board_status.raw_buffer_occupied[i]);
		Log(log_buf);
	}

	// ADC readings

	float vdda_actual = VREFINT_CAL * (ADC_MAX_VALUE / (float)board_status.vrefint);
	sprintf(log_buf, "VDD: %.3f V\r\n", vdda_actual);
	Log(log_buf);

	float vsense = (board_status.mcu_temp * vdda_actual) / ADC_MAX_VALUE;
	float temp_c = ((vsense - TEMPSENSOR_V25) / (TEMPSENSOR_AVG_SLOPE / 1000.0f)) + 25.0f;
	sprintf(log_buf, "MCU Temp: %.1f C\r\n", temp_c);
	Log(log_buf);

	sprintf(log_buf, "size of application in FRAM: %lu Bytes\r\n", fw_backup_info.fw_backup_size);
	Log(log_buf);

	sprintf(log_buf, "CRC of application in FRAM: 0x%06lX\r\n", fw_backup_info.fw_backup_crc32);
	Log(log_buf);

	sprintf(log_buf, "Version of application in FRAM: %lu.%lu\r\n",
						fw_backup_info.fw_backup_version & 0xFFFF0000, fw_backup_info.fw_backup_version & 0x0000FFFF);
	Log(log_buf);

	// Cam parameters in memory

	Log("Cam params in memory ------------------------------\r\n");

	sprintf(log_buf, "Black threshold: %u\r\n", board_status.cam_params.black_threshold);
	Log(log_buf);

	sprintf(log_buf, "Sensor analog gain (advanced, 32 is unity): %u\r\n", board_status.cam_params.sensor_analog_gain);
	Log(log_buf);

	sprintf(log_buf, "Sensor digital gain (advanced, 128 is unity): %u\r\n", board_status.cam_params.sensor_digital_gain);
	Log(log_buf);

	float exp_time = ((float)board_status.cam_params.sensor_coarse_exposure / 3906.25f		// Assumes PIXCLK of 13.5 MHz!
            		+ (float)board_status.cam_params.sensor_fine_exposure / 13500000.0f)
            		* 1000000.0f;
	sprintf(log_buf, "Exposure time (advanced): %.1f us\r\n", exp_time);
	Log(log_buf);

	sprintf(log_buf, "AirMac frame budget: %u/%u\r\n", sizeof(board_status_t) + sizeof(fw_backup_info_t) + DATA_HEADER_SIZE, AIRMAC_SIZE);
	Log(log_buf);

	return;
}


