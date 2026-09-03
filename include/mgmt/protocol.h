/**
 * Joybus device management.
 *
 * A Joybus command set for identifying, configuring and updating custom
 * devices from N64 and GameCube homebrew. These opcodes are a non-standard
 * extension of the Joybus protocol.
 *
 * Lock model
 * ----------
 * The management layer is gated behind a handshake, so it cannot collide with
 * other Joybus devices claiming the same opcodes.
 *
 *   - The device boots locked.
 *   - While locked it answers exactly one thing, IDENTIFY carrying the command
 *     magic. Every other 0x6x opcode, and IDENTIFY with wrong or absent magic,
 *     gets no reply at all.
 *   - That one correct-magic IDENTIFY unlocks the remaining commands for the
 *     session.
 *   - CTRL LOCK, or a reboot, returns the device to the locked state.
 *
 * Because a device that does not speak this protocol stays silent, a reply
 * carrying a valid signature is the whole presence test.
 *
 * Groups
 * ------
 * CTRL, STATUS and CONFIG_READ/CONFIG_WRITE all carry a group byte, which
 * namespaces a subsystem's commands, status and configuration. This header
 * defines only the wire format, plus the SYSTEM group the management layer
 * implements itself. Device group definitions live in their own headers.
 *
 * This header has no dependencies beyond stdint, so a host can speak the
 * protocol without pulling in any device-side code.
 */

#pragma once

#include <stdint.h>

// Magic sent with IDENTIFY and echoed back in the response
#define MGMT_MAGIC_HI            0x4A // 'J'
#define MGMT_MAGIC_LO            0x53 // 'S'

/**
 * Identify a device and unlock the rest of the command set.
 *
 * The only command a locked device answers, and the whole presence test. A
 * device that is not ours stays silent rather than replying.
 *
 *   command   { opcode, magic_hi, magic_lo }
 *   response  { struct mgmt_identify }
 */
#define MGMT_CMD_IDENTIFY        0x60
#define MGMT_CMD_IDENTIFY_TX     3
#define MGMT_CMD_IDENTIFY_RX     8

/**
 * Send a command to a group.
 *
 *   command   { opcode, group, command, arg }
 *   response  { result }, zero on success, otherwise an enum mgmt_error
 */
#define MGMT_CMD_CTRL            0x61
#define MGMT_CMD_CTRL_TX         4
#define MGMT_CMD_CTRL_RX         1

/**
 * Get the status of a group.
 *
 *   command   { opcode, group }
 *   response  { byte0 .. byte7 }
 */
#define MGMT_CMD_STATUS          0x62
#define MGMT_CMD_STATUS_TX       2
#define MGMT_CMD_STATUS_RX       8

/**
 * Read an 8-byte configuration block from a group's non-volatile storage.
 *
 * A group and block pair is a logical address, not a storage offset. The
 * device translates it to a physical flash or EEPROM location.
 *
 *   command   { opcode, group, block }
 *   response  { byte0 .. byte7 }
 */
#define MGMT_CMD_CONFIG_READ     0x63
#define MGMT_CMD_CONFIG_READ_TX  3
#define MGMT_CMD_CONFIG_READ_RX  8

/**
 * Write an 8-byte configuration block to a group's non-volatile storage.
 *
 *   command   { opcode, group, block, byte0 .. byte7 }
 *   response  { result }, zero on success, otherwise an enum mgmt_error
 */
#define MGMT_CMD_CONFIG_WRITE    0x64
#define MGMT_CMD_CONFIG_WRITE_TX 11
#define MGMT_CMD_CONFIG_WRITE_RX 1

/**
 * Write 32 data bytes to a group at the specified offset.
 *
 * Intended for streaming data such as firmware updates, where data is sent in
 * sequential blocks. The group decides what the address means, what ordering it
 * requires, and whether it is in a state to accept data at all.
 *
 * The response is the device's CRC8 over the 32 received data bytes, or that
 * CRC8 XOR 0xFF if the device rejected the write.
 *
 *   command   { opcode, group, addr_hi, addr_lo, byte0 .. byte31 }
 *   response  { crc8 }
 */
#define MGMT_CMD_DATA_WRITE      0x65
#define MGMT_CMD_DATA_WRITE_TX   36
#define MGMT_CMD_DATA_WRITE_RX   1

// Longest response any command produces
#define MGMT_RESPONSE_SIZE       8

// Block sizes for transfer operations
#define MGMT_STATUS_SIZE         8
#define MGMT_CONFIG_BLOCK_SIZE   8
#define MGMT_DATA_BLOCK_SIZE     32

/**
 * Group id ranges.
 *
 * Ids up to MGMT_GROUP_RESERVED_MAX are reserved for built-in groups.
 * Everything above is available to devices.
 */
#define MGMT_GROUP_RESERVED_MAX  0x0F
#define MGMT_GROUP_FIRST_CUSTOM  0x10

// The one built-in group, and its only command, returning the device to the
// locked state
#define MGMT_GROUP_SYSTEM        0x00
#define MGMT_SYSTEM_LOCK         0x00

/**
 * Error codes returned by CTRL and CONFIG_WRITE.
 *
 * A zero response means success. Any other value is a failure, and a host that
 * does not recognize the code should still treat it as one.
 */
enum mgmt_error {
  MGMT_ERR_UNSUPPORTED = 1,
  MGMT_ERR_BAD_STATE,
  MGMT_ERR_BAD_ADDRESS,
  MGMT_ERR_INCOMPLETE,
  MGMT_ERR_BUSY,
};

/** IDENTIFY response */
struct mgmt_identify {
  // Validate and abort on mismatch, this is the presence test
  uint8_t magic_hi;
  uint8_t magic_lo;

  // Identifies the board, so homebrew can determine what can be configured
  uint8_t hardware_id;

  // Running firmware version
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t version_patch;

  // Just in case
  uint8_t reserved[2];
};
