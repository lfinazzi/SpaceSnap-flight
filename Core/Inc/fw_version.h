/**
  ******************************************************************************
  * @file           : fw_version.h
  * @brief          : Firmware version definitions
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __FW_VERSION_H__
#define __FW_VERSION_H__

#define VERSION_MAJOR		(uint16_t)(0U)
#define VERSION_MINOR		(uint16_t)(9U)

// TODO: Burst logic and changes inside delayed picture state --> I think it is easier to define another command: CMD_TakePictureBurst() for taking many
// TODO: Update comments that might be out of date
// TODO: Remove any stale global variables and tidy up
// TODO: ERASE FRAM because board_status was changed
// TODO: tests. Check everything ok on monday and check bootloader recovery on corrupted .elf
// TODO: Startup banner

#endif
