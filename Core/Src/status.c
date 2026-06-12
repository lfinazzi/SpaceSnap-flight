#include "status.h"
#include "comms.h"
#include "main.h"

// Saves current board status
board_status_t board_status = {0};
airmac_board_status_t airmac_board_status = {0};

uint32_t timestamp = 0;
uint32_t total_seconds = 0;
char timestamp_string[15] = {0};
uint32_t hours = 0;
uint32_t minutes = 0;
uint32_t seconds = 0;


void UpdateStatus(void)
{
	board_status.uptime_session = HAL_GetTick();

	// copy abridged version of board_status_t for airmac transfer
	airmac_board_status.uptime_session = board_status.uptime_session;
	airmac_board_status.uptime_total = board_status.uptime_total;
	airmac_board_status.boot_count = board_status.boot_count;
	airmac_board_status.fram_ok = board_status.fram_ok;
	airmac_board_status.sram_ok = board_status.sram_ok;
	airmac_board_status.last_instruction = board_status.last_instruction;
	airmac_board_status.last_cmd_status = board_status.last_cmd_status;
	memcpy(airmac_board_status.last_opcode, board_status.last_opcode, OPCODE_SIZE);
	airmac_board_status.photos_taken = board_status.photos_taken;
	airmac_board_status.compressions_done = board_status.compressions_done;
	airmac_board_status.compression_ptr_address = board_status.compression_ptr_address;
	airmac_board_status.compression_count = board_status.compression_count;
	airmac_board_status.raw_buffer_1_occupied = board_status.raw_buffer_1_occupied;
	airmac_board_status.raw_buffer_2_occupied = board_status.raw_buffer_2_occupied;
	airmac_board_status.raw_buffer_3_occupied = board_status.raw_buffer_3_occupied;
	airmac_board_status.raw_buffer_4_occupied = board_status.raw_buffer_4_occupied;
	airmac_board_status.raw_buffer_5_occupied = board_status.raw_buffer_5_occupied;

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
