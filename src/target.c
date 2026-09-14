#include <string.h>

#include <joybus/checksum.h>
#include <joybus/errors.h>

#include <mgmt/protocol.h>
#include <mgmt/target.h>

struct mgmt_target_group *mgmt_target_find_group(struct mgmt_target *mgmt, uint8_t id)
{
  for (struct mgmt_target_group *group = mgmt->groups; group; group = group->next) {
    if (group->id == id)
      return group;
  }

  return NULL;
}

static inline int handle_identify(struct mgmt_target *mgmt, const uint8_t *command, uint8_t bytes_read,
                                  joybus_target_response_cb send_response, void *user_data)
{
  if (bytes_read == MGMT_CMD_IDENTIFY_TX) {
    if (!(command[1] == MGMT_MAGIC_HI && command[2] == MGMT_MAGIC_LO))
      return -JOYBUS_ERR_NOT_SUPPORTED;

    // Unlock the management commands and reply
    mgmt->locked = false;

    // Sent straight from the stored struct, whose layout is the wire layout
    send_response((const uint8_t *)&mgmt->identity, MGMT_CMD_IDENTIFY_RX, user_data);
  }

  return MGMT_CMD_IDENTIFY_TX - bytes_read;
}

static inline int handle_ctrl(struct mgmt_target *mgmt, const uint8_t *command, uint8_t bytes_read,
                              joybus_target_response_cb send_response, void *user_data)
{
  if (mgmt->locked)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  if (bytes_read == MGMT_CMD_CTRL_TX) {
    // An unregistered group is indistinguishable from an unknown command
    uint8_t result = MGMT_ERR_UNSUPPORTED;

    if (command[1] == MGMT_GROUP_SYSTEM) {
      // Handle system control commands
      if (command[2] == MGMT_SYSTEM_LOCK) {
        mgmt->locked = true;
        result       = 0;
      }
    } else {
      // Pass the control command to the group's API
      struct mgmt_target_group *group = mgmt_target_find_group(mgmt, command[1]);
      if (group && group->api->ctrl)
        result = group->api->ctrl(group, command[2], command[3]);
    }

    mgmt->response[0] = result;
    send_response(mgmt->response, MGMT_CMD_CTRL_RX, user_data);
  }

  return MGMT_CMD_CTRL_TX - bytes_read;
}

static inline int handle_status(struct mgmt_target *mgmt, const uint8_t *command, uint8_t bytes_read,
                                joybus_target_response_cb send_response, void *user_data)
{
  if (mgmt->locked)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  if (bytes_read == MGMT_CMD_STATUS_TX) {
    struct mgmt_target_group *group = mgmt_target_find_group(mgmt, command[1]);

    // No dedicated error field, so an unimplemented group reads zeros
    memset(mgmt->response, 0, MGMT_STATUS_SIZE);

    if (group && group->api->status)
      group->api->status(group, mgmt->response);

    send_response(mgmt->response, MGMT_CMD_STATUS_RX, user_data);
  }

  return MGMT_CMD_STATUS_TX - bytes_read;
}

static inline int handle_config_read(struct mgmt_target *mgmt, const uint8_t *command, uint8_t bytes_read,
                                     joybus_target_response_cb send_response, void *user_data)
{
  if (mgmt->locked)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  if (bytes_read == MGMT_CMD_CONFIG_READ_TX) {
    struct mgmt_target_group *group = mgmt_target_find_group(mgmt, command[1]);

    // No dedicated error field, so an unreadable block reads zeros
    memset(mgmt->response, 0, MGMT_CONFIG_BLOCK_SIZE);

    if (group && group->api->config_read)
      group->api->config_read(group, command[2], mgmt->response);

    send_response(mgmt->response, MGMT_CMD_CONFIG_READ_RX, user_data);
  }

  return MGMT_CMD_CONFIG_READ_TX - bytes_read;
}

static inline int handle_config_write(struct mgmt_target *mgmt, const uint8_t *command, uint8_t bytes_read,
                                      joybus_target_response_cb send_response, void *user_data)
{
  if (mgmt->locked)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  if (bytes_read == MGMT_CMD_CONFIG_WRITE_TX) {
    uint8_t result                  = MGMT_ERR_UNSUPPORTED;
    struct mgmt_target_group *group = mgmt_target_find_group(mgmt, command[1]);

    if (group && group->api->config_write)
      result = group->api->config_write(group, command[2], &command[3]);

    mgmt->response[0] = result;
    send_response(mgmt->response, MGMT_CMD_CONFIG_WRITE_RX, user_data);
  }

  return MGMT_CMD_CONFIG_WRITE_TX - bytes_read;
}

static inline int handle_data_write(struct mgmt_target *mgmt, const uint8_t *command, uint8_t bytes_read,
                                    joybus_target_response_cb send_response, void *user_data)
{
  if (mgmt->locked)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  // First 4 bytes are the opcode, group and address, reset the payload checksum
  if (bytes_read == 4) {
    mgmt->crc = 0;

    return MGMT_CMD_DATA_WRITE_TX - bytes_read;
  }

  // Subsequent bytes are payload, accumulate the checksum as they arrive
  mgmt->crc = joybus_data_checksum_update(mgmt->crc, command[bytes_read - 1]);

  if (bytes_read == MGMT_CMD_DATA_WRITE_TX) {
    struct mgmt_target_group *group = mgmt_target_find_group(mgmt, command[1]);
    uint16_t block                  = ((uint16_t)command[2] << 8) | command[3];

    // A group that cannot receive data rejects the block like any other refusal
    uint8_t result = MGMT_ERR_UNSUPPORTED;

    if (group && group->api->data_write)
      result = group->api->data_write(group, block, &command[4]);

    // Flip the CRC so the host can tell a rejection from a corrupt block
    if (result != 0)
      mgmt->crc ^= 0xFF;

    send_response(&mgmt->crc, MGMT_CMD_DATA_WRITE_RX, user_data);
  }

  return MGMT_CMD_DATA_WRITE_TX - bytes_read;
}

static int mgmt_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t bytes_read,
                              joybus_target_response_cb send_response, void *user_data)
{
  struct mgmt_target *mgmt = MGMT_TARGET(target);

  switch (command[0]) {
    case MGMT_CMD_IDENTIFY:
      return handle_identify(mgmt, command, bytes_read, send_response, user_data);

    case MGMT_CMD_CTRL:
      return handle_ctrl(mgmt, command, bytes_read, send_response, user_data);

    case MGMT_CMD_STATUS:
      return handle_status(mgmt, command, bytes_read, send_response, user_data);

    case MGMT_CMD_CONFIG_READ:
      return handle_config_read(mgmt, command, bytes_read, send_response, user_data);

    case MGMT_CMD_CONFIG_WRITE:
      return handle_config_write(mgmt, command, bytes_read, send_response, user_data);

    case MGMT_CMD_DATA_WRITE:
      return handle_data_write(mgmt, command, bytes_read, send_response, user_data);
  }

  // Not a management command
  return -JOYBUS_ERR_NOT_SUPPORTED;
}

static const struct joybus_target_api mgmt_api = {
  .byte_received = mgmt_byte_received,
};

void mgmt_target_init(struct mgmt_target *mgmt, const struct mgmt_identity *identity)
{
  // Start from a clean state
  memset(mgmt, 0, sizeof(*mgmt));

  struct joybus_target *target = JOYBUS_TARGET(mgmt);
  target->api                  = &mgmt_api;

  mgmt->identity = *identity;

  // Set here rather than by the caller, so a device cannot get it wrong
  mgmt->identity.magic_hi = MGMT_MAGIC_HI;
  mgmt->identity.magic_lo = MGMT_MAGIC_LO;

  // Commands locked until a correct-magic IDENTIFY
  mgmt->locked = true;
}

int mgmt_target_register_group(struct mgmt_target *mgmt, struct mgmt_target_group *group)
{
  // Reserved ids belong to the management layer, not to devices
  if (group->id < MGMT_GROUP_CUSTOM)
    return -1;

  if (mgmt_target_find_group(mgmt, group->id))
    return -1;

  group->next  = mgmt->groups;
  mgmt->groups = group;

  return 0;
}
