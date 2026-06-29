#include "protection.h"
#include "fram.h"

void SetState(uint8_t new_state)
{
    board_status.state = new_state;
    state_shadow_b     = new_state;
    state_shadow_c     = new_state;
}

uint8_t GetState(void)
{
    uint8_t a = board_status.state;
    uint8_t b = state_shadow_b;
    uint8_t c = state_shadow_c;

    if (a == b) return a;
    if (a == c) return a;
    if (b == c) return b;

    /* All three disagree - force idle and log */
    Log("State vote failed - forcing IDLE\r\n");
    board_status.state_vote_fail_count++;
    SetState(STATE_IDLE);
    return STATE_IDLE;
}

// Wrapper for Update_CRC32
uint32_t CalculateCRC32(const uint8_t *data, uint32_t len)
{
	return ~CRC32_Update(0xFFFFFFFF, data, len);
}

uint32_t BoardStatusCRC(void)
{
    /* Skip volatile fields that change every loop:
     * uptime_session, state, last_reset_cause, fram_ok, sram_ok,
     * last_instruction, last_cmd_status, last_opcode, delayed_flag */
    uint32_t crc = 0xFFFFFFFF;

    /* Only CRC the non-volatile fields worth protecting */
    crc = CRC32_Update(crc, (uint8_t*)&board_status.uptime_total,           	sizeof(board_status.uptime_total));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.boot_count,            		sizeof(board_status.boot_count));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.photos_taken,           	sizeof(board_status.photos_taken));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.compressions_done,      	sizeof(board_status.compressions_done));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.compression_ptr_address,	sizeof(board_status.compression_ptr_address));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.compression_count,      	sizeof(board_status.compression_count));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.cam_params,             	sizeof(board_status.cam_params));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.delayed_params,         	sizeof(board_status.delayed_params));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.delayed_start,          	sizeof(board_status.delayed_start));
    crc = CRC32_Update(crc, (uint8_t*)&board_status.delayed_intervals,      	sizeof(board_status.delayed_intervals));

    return ~crc;
}

uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
    }
    return crc;
}

#ifdef DEBUG_FAULT_INJECTION


void DEBUG_CorruptBoardStatus(void)
{
    board_status.photos_taken ^= 0xFFFF;    /* flip all bits */
    Log("DEBUG: board_status.photos_taken corrupted\r\n");
}


void DEBUG_CorruptCompressionTable(void)
{
    compression_table[0].fram_address ^= 0xFFFFFF;
    Log("DEBUG: compression_table[0].fram_address corrupted\r\n");
}


void DEBUG_CorruptFRAMStatus(void)
{
    uint8_t garbage[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    SaveFRAM_Unlocked(garbage, 4, BOARD_STATUS_START);
    Log("DEBUG: FRAM board_status corrupted\r\n");
}

#endif 	/* DEBUG_FAULT_INJECTION */
