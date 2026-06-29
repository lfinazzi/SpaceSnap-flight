/**
  ******************************************************************************
  * @file           : protection.h
  * @brief          : Protection agains SEU and memory corruption
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __PROTECTION_H__
#define __PROTECTION_H__

#include "status.h"

/********************************************************************************
 * @brief  Sets the state machine state across all three redundant copies.
 *
 * @note   Must be used for every state transition instead of directly
 *         assigning board_status.state. Keeps all three copies in sync
 *         so GetState() majority vote remains valid.
 *
 * @param  new_state   State to transition to.
 *
 * @retval None
 ********************************************************************************/
void SetState(uint8_t new_state);


/********************************************************************************
 * @brief  Returns the current state machine state using a majority vote
 *         across three redundant copies.
 *
 * @note   If all three copies disagree (no majority possible), forces a
 *         transition to STATE_IDLE, logs the event, and increments
 *         board_status.state_vote_fail_count for ground visibility via
 *         CMD_GetStatus. Must be used for every state read instead of
 *         directly reading board_status.state.
 *
 * @retval uint8_t   Winning state from majority vote.
 ********************************************************************************/
uint8_t GetState(void);


#endif	/* __PROTECTION_H__ */
