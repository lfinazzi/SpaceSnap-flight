#include "status.h"
#include "comms.h"
#include "main.h"

// Saves current board status
board_status_t board_status = {0};

uint32_t timestamp = 0;
uint32_t total_seconds = 0;
char timestamp_string[15] = {0};
uint32_t hours = 0;
uint32_t minutes = 0;
uint32_t seconds = 0;

uint32_t total_elapsed_ms = 0;

void UpdateStatus(void)
{
	uint32_t now = HAL_GetTick();
	uint32_t delta_ms = now - board_status.uptime_ms;

	total_elapsed_ms += delta_ms;
	board_status.total_uptime = total_elapsed_ms / 100;  	// 100 ms intervals
	board_status.uptime_ms = now;
}

void LogBoardStatus(void)
{
    char log_buf[96];

    sprintf(log_buf, "Boot #%lu | Total uptime [s]: %lu | Photos: %u | Compressions: %u | FRAM/SRAM ok: %u/%u\r\n",
            board_status.boot_count,
            board_status.total_uptime / 10,
            board_status.photos_taken,
            board_status.compressions_done,
			board_status.fram_ok,
			board_status.sram_ok);
    Log(log_buf);
}
