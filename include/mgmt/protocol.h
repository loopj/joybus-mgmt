/**
 * Joybus device management protocol.
 *
 * The wire format for a command set that identifies, configures and updates
 * custom devices from N64 and GameCube homebrew, as a non-standard extension
 * of the Joybus protocol.
 */

#pragma once

#include <stdint.h>

// Magic sent with IDENTIFY and echoed back in the response
#define MGMT_MAGIC_HI            0x4A // 'J'
#define MGMT_MAGIC_LO            0x53 // 'S'

/**
 * Identify a device and unlock the rest of the command set.
 *
 * A device boots locked, answering only this command and only when the magic
 * matches. Everything else gets no reply at all, so the command set cannot
 * collide with other Joybus devices using the same opcodes.
 *
 *   command   { opcode, magic_hi, magic_lo }
 *   response  { struct mgmt_identity }
 */
#define MGMT_CMD_IDENTIFY        0x60
#define MGMT_CMD_IDENTIFY_TX     3
#define MGMT_CMD_IDENTIFY_RX     8

/**
 * Send a command to a group.
 *
 * Triggers an action, for example beginning a firmware update or capturing a
 * calibration pose. One argument byte is always present, ignored by verbs that
 * do not use it.
 *
 * Verbs are scoped to their group, so two groups may use the same value for
 * different actions.
 *
 *   command   { opcode, group, verb, arg }
 *   response  { result }, zero on success, otherwise an error code
 */
#define MGMT_CMD_CTRL            0x61
#define MGMT_CMD_CTRL_TX         4
#define MGMT_CMD_CTRL_RX         1

/**
 * Get the status of a group.
 *
 * Reports live state rather than stored settings, for example how far a
 * firmware update has got.
 *
 * The response has no dedicated error field, so a group the device does not
 * have, or one that reports no status, reads back all zeros.
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
 * The response has no dedicated error field, so an unknown group, a group with
 * no configuration, or a block outside its range all read back as zeros.
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
 * A group and block pair is a logical address, not a storage offset. The
 * device translates it to a physical flash or EEPROM location.
 *
 *   command   { opcode, group, block, byte0 .. byte7 }
 *   response  { result }, zero on success, otherwise an error code
 */
#define MGMT_CMD_CONFIG_WRITE    0x64
#define MGMT_CMD_CONFIG_WRITE_TX 11
#define MGMT_CMD_CONFIG_WRITE_RX 1

/**
 * Write 32 data bytes to a group at the specified offset.
 *
 * Intended for streaming data such as firmware updates. The group decides what
 * the address means, what ordering it requires, and whether it is in a state to
 * accept data at all.
 *
 * The response is the device's checksum over the 32 received data bytes, which
 * a host compares against its own to detect a corrupt transfer, or that
 * checksum XOR 0xFF if the device rejected the write.
 *
 * The checksum is CRC-8 with polynomial 0x85, the same one N64 Controller Pak
 * transfers use. Implementations already exist, as joybus_data_checksum() in
 * libjoybus and joybus_accessory_calculate_data_crc() in libdragon.
 *
 *   command   { opcode, group, addr_hi, addr_lo, byte0 .. byte31 }
 *   response  { crc8 }
 */
#define MGMT_CMD_DATA_WRITE      0x65
#define MGMT_CMD_DATA_WRITE_TX   36
#define MGMT_CMD_DATA_WRITE_RX   1

// Block sizes for transfer operations
#define MGMT_STATUS_SIZE         8
#define MGMT_CONFIG_BLOCK_SIZE   8
#define MGMT_DATA_BLOCK_SIZE     32

// How many config blocks a record of `payload` bytes occupies
#define MGMT_CONFIG_RECORD_BLOCKS(payload) \
  (((payload) + MGMT_CONFIG_BLOCK_SIZE - 1) / MGMT_CONFIG_BLOCK_SIZE)

// Padded size of that record, which is the buffer each side needs
#define MGMT_CONFIG_RECORD_BYTES(payload) (MGMT_CONFIG_RECORD_BLOCKS(payload) * MGMT_CONFIG_BLOCK_SIZE)

// First ID available for custom groups, below this is reserved
#define MGMT_GROUP_CUSTOM        0x80

// First ID available for custom vendors, below this is reserved
#define MGMT_VENDOR_CUSTOM       0x80

// Built-in system group, and the command that re-locks the device
#define MGMT_GROUP_SYSTEM        0x00
#define MGMT_SYSTEM_LOCK         0x00

/**
 * Error codes a device can answer with.
 *
 * A zero response means success. Any other value is a failure, and a host that
 * does not recognize the code should still treat it as one.
 */
enum mgmt_error {
  // The group, or the command within it, is not implemented
  MGMT_ERR_UNSUPPORTED = 1,

  // Understood, but not while the device is in its current state
  MGMT_ERR_BAD_STATE,

  // No such address in this group
  MGMT_ERR_BAD_ADDRESS,

  // The address was fine, the value was not
  MGMT_ERR_BAD_VALUE,

  // The device could not complete the command, such as a failed storage write
  MGMT_ERR_FAILED,

  // Busy with something else, so worth retrying
  MGMT_ERR_BUSY,

  // Codes from here up are device defined
  MGMT_ERR_CUSTOM = 0x80,
};

/** IDENTIFY response */
struct mgmt_identity {
  // Fixed signature for this protocol
  uint8_t magic_hi;
  uint8_t magic_lo;

  // Hardware vendor, model, and variant
  uint8_t vendor;
  uint8_t model;
  uint8_t variant;

  // Firmware version
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t version_patch;
};
