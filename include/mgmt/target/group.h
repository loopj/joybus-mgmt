/**
 * Management group interface.
 *
 * A group is a namespaced subsystem, owning its own CTRL commands, STATUS
 * block and configuration. Devices implement groups, the management target
 * dispatches to them and knows nothing else about them.=
 */

#pragma once

#include <stdint.h>

#include <mgmt/protocol.h>

struct mgmt_group;

/// Cast a concrete group instance to a generic group instance
#define MGMT_GROUP(group) ((struct mgmt_group *)(group))

/**
 * API for implementing a management group. Every entry is optional.
 */
struct mgmt_group_api {
  /**
   * Handle a CTRL command.
   *
   * @param group the group handling the command
   * @param verb the group-local verb
   * @param arg the argument, or zero for verbs that take none
   * @return 0 on success, otherwise an ::mgmt_error or a device-defined code
   */
  uint8_t (*ctrl)(struct mgmt_group *group, uint8_t verb, uint8_t arg);

  /**
   * Fill in the group's STATUS block.
   *
   * The buffer is zeroed before the call, so a group only writes what it uses.
   *
   * @param group the group being queried
   * @param out destination for the status block
   */
  void (*status)(struct mgmt_group *group, uint8_t out[MGMT_STATUS_SIZE]);

  /**
   * Read a block of the group's configuration.
   *
   * The group translates the block index to wherever it actually keeps the
   * data. The buffer is zeroed before the call, and the response has no
   * dedicated error field, so an out of range block simply reads back zeros.
   *
   * @param group the group being read
   * @param block the group-local block index
   * @param out destination for the block
   */
  void (*config_read)(struct mgmt_group *group, uint8_t block, uint8_t out[MGMT_CONFIG_BLOCK_SIZE]);

  /**
   * Write a block of the group's configuration.
   *
   * The write takes effect immediately, so a group re-derives whatever it
   * computes from config here rather than waiting for a reboot.
   *
   * @param group the group being written
   * @param block the group-local block index
   * @param data the block to write
   * @return 0 on success, otherwise an ::mgmt_error or a device-defined code
   */
  uint8_t (*config_write)(struct mgmt_group *group, uint8_t block, const uint8_t data[MGMT_CONFIG_BLOCK_SIZE]);

  /**
   * Handle a block of streamed data.
   *
   * The group decides what the address means, what ordering it requires, and
   * whether it is in a state to accept data at all.
   *
   * @param group the group that owns the stream
   * @param block the 16-bit block index from the command
   * @param data the received data
   * @return 0 on success, otherwise an ::mgmt_error or a device-defined code
   */
  uint8_t (*data_write)(struct mgmt_group *group, uint16_t block, const uint8_t data[MGMT_DATA_BLOCK_SIZE]);
};

/**
 * Interface for a management group.
 */
struct mgmt_group {
  /// API for handling commands sent to this group
  const struct mgmt_group_api *api;

  /// Group id, as sent on the wire
  uint8_t id;

  /// Next group in the target's registration list
  struct mgmt_group *next;
};
