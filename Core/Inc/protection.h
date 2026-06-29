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
#include "main.h"

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


/********************************************************************************
 * @brief  Computes the CRC32 checksum of a contiguous data buffer.
 *
 * @note   Uses zlib-compatible CRC32: polynomial 0xEDB88320 (reflected),
 *         initial value 0xFFFFFFFF, final XOR 0xFFFFFFFF. Matches the
 *         bootloader's CRC32_Calculate() and the inline loop in
 *         CMD_BackupFirmware().
 *
 * @param  data   Pointer to buffer.
 * @param  len    Number of bytes.
 *
 * @retval uint32_t   CRC32 value.
 ********************************************************************************/
uint32_t CalculateCRC32(const uint8_t *data, uint32_t len);


/********************************************************************************
 * @brief  Computes a CRC32 over the non-volatile fields of board_status
 *         that are worth protecting against SRAM corruption.
 *
 * @note   Intentionally excludes fields that change legitimately every
 *         loop iteration (uptime_session, state, last_reset_cause,
 *         fram_ok, sram_ok, last_instruction, last_cmd_status,
 *         last_opcode, delayed_flag) to avoid false corruption
 *         detections from normal runtime updates. State is excluded
 *         because it is already protected by the majority vote in
 *         GetState()/SetState(). Uses CRC32_Update() to accumulate
 *         a single CRC across all included fields rather than
 *         computing independent CRCs per field. The final ~crc
 *         matches the zlib-compatible convention used throughout
 *         the codebase.
 *
 * @retval uint32_t   CRC32 over the protected non-volatile fields
 *                    of board_status.
 ********************************************************************************/
uint32_t BoardStatusCRC(void);


/********************************************************************************
 * @brief  Updates a running CRC32 accumulator over a data buffer.
 *
 * @note   Incremental variant of the zlib-compatible CRC32 algorithm
 *         (poly 0xEDB88320, reflected). Unlike CalculateCRC32(), this
 *         function neither initialises the accumulator to 0xFFFFFFFF
 *         nor applies the final XOR — the caller is responsible for
 *         both. Allows a single valid CRC32 to be computed over
 *         non-contiguous fields by chaining calls:
 *
 *           uint32_t crc = 0xFFFFFFFF;
 *           crc = CRC32_Update(crc, field_a, sizeof(field_a));
 *           crc = CRC32_Update(crc, field_b, sizeof(field_b));
 *           uint32_t result = ~crc;
 *
 * @param  crc    Running accumulator. Pass 0xFFFFFFFF on first call,
 *                then pass the return value of each subsequent call.
 * @param  data   Pointer to the data buffer to process.
 * @param  len    Number of bytes to process.
 *
 * @retval uint32_t   Updated accumulator. Apply ~ to get the final
 *                    CRC32 after the last call in the chain.
 ********************************************************************************/
uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len);


	// Functions to inject faults in board_status and compression_table.
	#ifdef DEBUG_FAULT_INJECTION

	/********************************************************************************
	 * @brief  Corrupts a single byte of board_status in RAM without updating
	 *         the shadow CRC, simulating an SRAM SEU on a non-volatile field.
	 *         The next BoardStatusIntact() check should detect the mismatch
	 *         and trigger a FRAM restore.
	 *
	 * @retval None
	 ********************************************************************************/
	void DEBUG_CorruptBoardStatus(void);


	/********************************************************************************
	 * @brief  Corrupts a single entry of compression_table in RAM without
	 *         updating the shadow CRC, simulating an SRAM SEU on the index.
	 *
	 * @retval None
	 ********************************************************************************/
	void DEBUG_CorruptCompressionTable(void);


	/********************************************************************************
	 * @brief  Corrupts board_status directly in FRAM without touching RAM,
	 *         simulating a FRAM cell upset or failed write. The next
	 *         LoadBoardStatusFRAM() call should detect the CRC mismatch
	 *         on boot.
	 *
	 * @retval None
	 ********************************************************************************/
	void DEBUG_CorruptFRAMStatus(void);


	#endif   /* DEBUG_FAULT_INJECTION */


#endif	/* __PROTECTION_H__ */
