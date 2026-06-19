#include "status.h"
#include "comms.h"
#include "main.h"

// Saves current board status
board_status_t board_status = {0};
compression_index_entry_t compression_table[MAX_COMPRESSED_PHOTOS] = {0};		// Compression table in FRAM

uint32_t timestamp = 0;
uint32_t total_seconds = 0;
char timestamp_string[15] = {0};
uint32_t hours = 0;
uint32_t minutes = 0;
uint32_t seconds = 0;


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

	sprintf(log_buf, "images_rejected_black: %u\r\n", board_status.images_rejected_black);		// TODO
	Log(log_buf);

	// FRAM compression tracking
	sprintf(log_buf, "compression_ptr_address: 0x%06lX\r\n", board_status.compression_ptr_address);
	Log(log_buf);

	sprintf(log_buf, "compression_count_in_memory: %u\r\n", board_status.compression_count);
	Log(log_buf);

	sprintf(log_buf, "fram_bytes_left: %lu\r\n", board_status.fram_bytes_left);
	Log(log_buf);

	// SRAM buffer occupancy
	sprintf(log_buf, "raw_buffer_1_occupied: %u\r\n", board_status.raw_buffer_1_occupied);
	Log(log_buf);

	sprintf(log_buf, "raw_buffer_2_occupied: %u\r\n", board_status.raw_buffer_2_occupied);
	Log(log_buf);

	sprintf(log_buf, "raw_buffer_3_occupied: %u\r\n", board_status.raw_buffer_3_occupied);
	Log(log_buf);

	sprintf(log_buf, "raw_buffer_4_occupied: %u\r\n", board_status.raw_buffer_4_occupied);
	Log(log_buf);

	sprintf(log_buf, "raw_buffer_5_occupied: %u\r\n", board_status.raw_buffer_5_occupied);
	Log(log_buf);

	// ADC readings

	float vdda_actual = VREFINT_CAL * (ADC_MAX_VALUE / (float)board_status.vrefint);		// TODO: Implement temperature correction?
	sprintf(log_buf, "VDD: %.3f V\r\n", vdda_actual);
	Log(log_buf);


	float vsense = (board_status.mcu_temp * vdda_actual) / ADC_MAX_VALUE;
	float temp_c = ((vsense - TEMPSENSOR_V25) / (TEMPSENSOR_AVG_SLOPE / 1000.0f)) + 25.0f;
	sprintf(log_buf, "MCU Temp: %.1f C\r\n", temp_c);
	Log(log_buf);

	// Firmware backup integrity, TODO
	//sprintf(log_buf, "backup_fw_crc: 0x%08lX\r\n", board_status.backup_fw_crc);
	//Log(log_buf);

	sprintf(log_buf, "size_of_board_status: %u/%u Bytes\r\n", sizeof(board_status_t), AIRMAC_SIZE - HEADER_SIZE);
	Log(log_buf);

	return;
}


