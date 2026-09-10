/**
 * libjoybus management target.
 *
 * A libjoybus target implementing the management command set. Owns the
 * lock state and the registered groups, and dispatches every command to the
 * group it names.
 *
 * The target knows nothing about any individual group beyond the SYSTEM group
 * it implements itself. Devices register their own groups.
 *
 * Commands the target does not handle are delegated to a child target, so it
 * can be layered in front of a concrete N64 or GameCube controller.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <joybus/bus.h>
#include <joybus/target.h>

#include <mgmt/protocol.h>
#include <mgmt/target_group.h>

/// Cast from a generic Joybus target to a management target
#define MGMT_TARGET(target) ((struct mgmt_target *)(target))

/**
 * Joybus management target.
 */
struct mgmt_target {
  /// API interface
  struct joybus_target base;

  /// Child target handling everything that is not a management command
  struct joybus_target *child;

  /// The IDENTIFY response, built once at init and sent verbatim
  struct mgmt_identity identity;

  /// Head of the registered group list
  struct mgmt_target_group *groups;

  /// Locked until a correct-magic IDENTIFY
  bool locked;

  /// Running CRC over the DATA_WRITE payload
  uint8_t crc;

  /// Response buffer
  uint8_t response[JOYBUS_BLOCK_SIZE];
};

/**
 * Initialize the management target.
 *
 * @param mgmt the management target to initialize
 * @param child the child target to manage, eg. an N64 or GameCube controller
 * @param identity what to report from IDENTIFY. The magic is owned by the
 *                 library and set here, so a caller fills in only the vendor,
 *                 model, variant and version.
 */
void mgmt_target_init(struct mgmt_target *mgmt, struct joybus_target *child, const struct mgmt_identity *identity);

/**
 * Register a group with the management target.
 *
 * The group is caller-owned and must outlive the target.
 *
 * @param mgmt the management target
 * @param group the group to register
 * @return 0 on success, negative error code if the id is reserved or taken
 */
int mgmt_target_register_group(struct mgmt_target *mgmt, struct mgmt_target_group *group);

/**
 * Find a registered group by id.
 *
 * @param mgmt the management target
 * @param id the group id to look for
 * @return the group, or NULL if it is not registered
 */
struct mgmt_target_group *mgmt_target_find_group(struct mgmt_target *mgmt, uint8_t id);
