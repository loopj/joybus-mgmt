/**
 * Joybus management target.
 *
 * A generic Joybus target implementing the management command set. Owns the
 * lock state, the registered groups and the data stream, and dispatches every
 * command to the group that owns it.
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

#include <joybus/target.h>

#include <mgmt/protocol.h>
#include <mgmt/target/group.h>

/// Cast from a generic Joybus target to a management target
#define MGMT_TARGET(target) ((struct mgmt_target *)(target))

/**
 * Device identity, reported in the IDENTIFY response.
 */
struct mgmt_identity {
  /// Board identifier
  uint8_t hardware_id;

  /// Running firmware version
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t version_patch;
};

/**
 * Joybus management target.
 */
struct mgmt_target {
  /// API interface
  struct joybus_target base;

  /// Child target handling everything that is not a management command
  struct joybus_target *child;

  /// Reported by IDENTIFY
  struct mgmt_identity identity;

  /// Head of the registered group list
  struct mgmt_group *groups;

  /// Locked until a correct-magic IDENTIFY
  bool locked;

  /// Running CRC over the DATA_WRITE payload
  uint8_t crc;

  /// Response buffer
  uint8_t response[MGMT_RESPONSE_SIZE];
};

/**
 * Initialize the management target.
 *
 * @param mgmt the management target to initialize
 * @param child the child target to manage, eg. a concrete N64 or GameCube target
 * @param identity the identity to report from IDENTIFY
 */
void mgmt_target_init(struct mgmt_target *mgmt, struct joybus_target *child, const struct mgmt_identity *identity);

/**
 * Register a group with the management target.
 *
 * The group is caller-owned and must outlive the target.
 *
 * @param mgmt the management target
 * @param group the group to register
 * @return 0 on success, negative error code if the id is reserved or taken, or
 *         if the group streams and another streaming group is already registered
 */
int mgmt_target_register_group(struct mgmt_target *mgmt, struct mgmt_group *group);

/**
 * Find a registered group by id.
 *
 * @param mgmt the management target
 * @param id the group id to look for
 * @return the group, or NULL if it is not registered
 */
struct mgmt_group *mgmt_find_group(struct mgmt_target *mgmt, uint8_t id);
