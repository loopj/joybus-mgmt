/**
 * Management group interface.
 *
 * A group is a namespaced subsystem, owning its own CTRL commands, STATUS
 * block and configuration. Devices implement groups, the management target
 * dispatches to them and knows nothing else about them.
 *
 * To create a group, define a struct whose first member is a ::mgmt_group (so
 * it can be cast through ::MGMT_GROUP), point its api at a ::mgmt_group_api
 * table, and register it with mgmt_target_register_group().
 *
 * This header deliberately does not depend on libjoybus or on the target, so a
 * group implementation never has to know it is reached over a bus.
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
   * @param command the group-local command
   * @param arg the command argument
   * @return 0 on success, otherwise an enum mgmt_error
   */
  uint8_t (*ctrl)(struct mgmt_group *group, uint8_t command, uint8_t arg);

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
   * data. The buffer is zeroed before the call, and there is no error channel,
   * so an out of range block simply reads back zeros.
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
   * @return 0 on success, otherwise an enum mgmt_error
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
   * @return 0 on success, otherwise an enum mgmt_error
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
