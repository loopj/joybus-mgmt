/**
 * Joybus management host.
 *
 * The host side of the management protocol, for homebrew and tools that talk
 * to a device. One function per command, each a blocking round trip on the
 * bus.
 *
 * The API is platform neutral. A platform backend, such as src/host/n64.c for
 * libdragon, supplies the transport.
 */

#pragma once

#include <stdint.h>

#include <mgmt/protocol.h>

/**
 * Management host error codes.
 *
 * Errors are reported as negatives of these codes. Functions return 0 on
 * success or a negative mgmt_host_error on failure. Success means the exchange
 * completed, so a command carrying a result code from the device reports that
 * separately, through its own parameter.
 */
enum mgmt_host_error {
  // Nothing on the port answered
  MGMT_HOST_ERR_NO_REPLY = 1,

  // Something answered IDENTIFY, but without this protocol's signature
  MGMT_HOST_ERR_BAD_MAGIC,

  // The DATA_WRITE checksum did not match, so the block was corrupted on the wire
  MGMT_HOST_ERR_BAD_CRC,

  // The group refused the DATA_WRITE, for example because it is not receiving
  MGMT_HOST_ERR_REJECTED,
};

/**
 * Identify a device, unlocking its management commands.
 *
 * Doubles as a presence probe. A silent port and a port holding a device that
 * answered with the wrong signature are reported separately, so a host can
 * tell an empty port from another device using the same opcode.
 *
 * @param port the port to query
 * @param response buffer to store the identify response in, may be NULL
 * @return 0 on success, a negative mgmt_host_error on failure
 */
int mgmt_host_identify(int port, struct mgmt_identity *response);

/**
 * Send a CTRL command to a group.
 *
 * @param port the port to command
 * @param group the group to command
 * @param verb the group-local verb
 * @param arg the argument, or 0 for verbs that take none
 * @param result buffer to store the device's result code in, 0 if it carried
 *               the command out, otherwise an ::mgmt_error or a device-defined code
 * @return 0 on success, a negative mgmt_host_error on failure
 */
int mgmt_host_ctrl(int port, uint8_t group, uint8_t verb, uint8_t arg, uint8_t *result);

/**
 * Read a group's STATUS block.
 *
 * On the wire an unknown group reads back zeros, which a host cannot tell from
 * a real all-zero status. A missing device is reported separately.
 *
 * @param port the port to query
 * @param group the group to query
 * @param response buffer to store the status block in
 * @return 0 on success, a negative mgmt_host_error on failure
 */
int mgmt_host_status(int port, uint8_t group, uint8_t response[MGMT_STATUS_SIZE]);

/**
 * Read one config block from a group.
 *
 * On the wire an unknown group or block reads back zeros, which a host cannot
 * tell from real zero data. A missing device is reported separately.
 *
 * @param port the port to read from
 * @param group the group to read from
 * @param block the group-local block index
 * @param response buffer to store the config block in
 * @return 0 on success, a negative mgmt_host_error on failure
 */
int mgmt_host_config_read(int port, uint8_t group, uint8_t block, uint8_t response[MGMT_CONFIG_BLOCK_SIZE]);

/**
 * Write one config block to a group.
 *
 * @param port the port to write to
 * @param group the group to write to
 * @param block the group-local block index
 * @param data the block to write
 * @param result buffer to store the device's result code in, 0 if the write
 *               succeeded, otherwise an ::mgmt_error or a device-defined code
 * @return 0 on success, a negative mgmt_host_error on failure
 */
int mgmt_host_config_write(int port, uint8_t group, uint8_t block, const uint8_t data[MGMT_CONFIG_BLOCK_SIZE],
                           uint8_t *result);

/**
 * Stream one data block to a group.
 *
 * The device's checksum is compared against the host's own, so a caller sees
 * a clean write, a corrupt transfer worth retrying, or a rejection.
 *
 * @param port the port to write to
 * @param group the group receiving the data
 * @param block the block's index within the stream
 * @param data the block to write
 * @return 0 on success, a negative mgmt_host_error on failure
 */
int mgmt_host_data_write(int port, uint8_t group, uint16_t block, const uint8_t data[MGMT_DATA_BLOCK_SIZE]);
