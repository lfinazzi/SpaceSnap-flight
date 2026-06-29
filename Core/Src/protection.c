#include "protection.h"


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
