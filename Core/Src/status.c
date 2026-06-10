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

extern uint8_t current_metadata_slot;
extern uint32_t current_data_address_sram;

void UpdateStatus(void)
{
    board_status.uptime_ms = HAL_GetTick();
    board_status.compressed_metadata_slot = current_metadata_slot;
    board_status.compressed_data_sram_ptr = current_data_address_sram;

    // TODO: Add other parameters in board status. Note: Only uptime_ms and boot_count need to
    //  be here, as other status variables are handled by photo taking commands
}

void LogBoardStatus(void)
{
    char log_buf[80];

    sprintf(log_buf, "Boot #%lu | Uptime: %lums | Photos: %u | Compressions: %u\r\n",
            board_status.boot_count,
            board_status.uptime_ms,
            board_status.photos_taken,
            board_status.compressions_done);
    Log(log_buf);
}
