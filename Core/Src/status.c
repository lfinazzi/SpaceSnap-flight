#include "status.h"

// Saves current board status
board_status_t board_status = {0};

uint32_t timestamp = 0;
uint32_t total_seconds = 0;
char timestamp_string[15] = {0};
uint32_t hours = 0;
uint32_t minutes = 0;
uint32_t seconds = 0;
