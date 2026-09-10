/**
 * libdragon backend for the management host.
 *
 * Builds each command as a PIF block and runs it through joybus_exec(), which
 * blocks until the exchange completes. libdragon's joybus_exec_cmd() is not
 * used because it drops the PIF's error flags, and with them any way to tell
 * a silent port from a device that answered zeros.
 */

#include <stdbool.h>
#include <string.h>

#include <libdragon.h>

#include <mgmt/host.h>

// Flags the PIF sets in a command's receive length byte after execution
#define PIF_RX_NO_DEVICE 0x80 // Nothing on the port answered
#define PIF_RX_OVERFLOW  0x40 // The reply was longer than the space allocated

// Marks the end of the command list in a PIF block
#define PIF_END_OF_BLOCK 0xFE

// Written to the last byte of the block to ask the PIF to run the commands
#define PIF_RUN_COMMANDS 0x01

// Not public API: libdragon exports this but declares it only in a private header
uint8_t joybus_accessory_calculate_data_crc(const uint8_t *data);

// Run one command on a port, returning false if the device did not answer
// Not joybus_exec_cmd(), which discards the receive length byte and its flags
static bool exec(int port, const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len)
{
  // The PIF block sent, and the one that comes back
  uint8_t input[JOYBUS_BLOCK_SIZE]  = {0};
  uint8_t output[JOYBUS_BLOCK_SIZE] = {0};

  // Skip the ports before this one, then lay down the command and its lengths
  size_t i   = port;
  input[i++] = command_len;
  input[i++] = response_len;
  memcpy(&input[i], command, command_len);
  i += command_len + response_len;

  // Close out the block and ask the PIF to run it
  input[i]                     = PIF_END_OF_BLOCK;
  input[JOYBUS_BLOCK_SIZE - 1] = PIF_RUN_COMMANDS;

  // Run the block, blocking until the exchange completes
  joybus_exec(input, output);

  // Check the receive length byte for a missing device or an overrun reply
  if (output[port + 1] & (PIF_RX_NO_DEVICE | PIF_RX_OVERFLOW))
    return false;

  // Copy the reply out of the block
  memcpy(response, &output[i - response_len], response_len);

  return true;
}

int mgmt_host_identify(int port, struct mgmt_identity *response)
{
  // Build the IDENTIFY command
  const uint8_t command[MGMT_CMD_IDENTIFY_TX] = {MGMT_CMD_IDENTIFY, MGMT_MAGIC_HI, MGMT_MAGIC_LO};

  // Execute the command and check for a reply, the wire layout being the struct layout
  struct mgmt_identity identity;
  if (!exec(port, command, sizeof(command), (uint8_t *)&identity, MGMT_CMD_IDENTIFY_RX))
    return -MGMT_HOST_ERR_NO_REPLY;

  // Something answered on this opcode, but it is not one of ours
  if (identity.magic_hi != MGMT_MAGIC_HI || identity.magic_lo != MGMT_MAGIC_LO)
    return -MGMT_HOST_ERR_BAD_MAGIC;

  // Copy the identity into the output struct, if provided
  if (response)
    *response = identity;

  return 0;
}

int mgmt_host_ctrl(int port, uint8_t group, uint8_t verb, uint8_t arg, uint8_t *result)
{
  // Build the CTRL command
  const uint8_t command[MGMT_CMD_CTRL_TX] = {MGMT_CMD_CTRL, group, verb, arg};

  // Execute the command and check for a reply
  if (!exec(port, command, sizeof(command), result, MGMT_CMD_CTRL_RX))
    return -MGMT_HOST_ERR_NO_REPLY;

  return 0;
}

int mgmt_host_status(int port, uint8_t group, uint8_t response[MGMT_STATUS_SIZE])
{
  // Build the STATUS command
  const uint8_t command[MGMT_CMD_STATUS_TX] = {MGMT_CMD_STATUS, group};

  // Execute the command and check for a reply
  if (!exec(port, command, sizeof(command), response, MGMT_CMD_STATUS_RX))
    return -MGMT_HOST_ERR_NO_REPLY;

  return 0;
}

int mgmt_host_config_read(int port, uint8_t group, uint8_t block, uint8_t response[MGMT_CONFIG_BLOCK_SIZE])
{
  // Build the CONFIG_READ command
  const uint8_t command[MGMT_CMD_CONFIG_READ_TX] = {MGMT_CMD_CONFIG_READ, group, block};

  // Execute the command and check for a reply
  if (!exec(port, command, sizeof(command), response, MGMT_CMD_CONFIG_READ_RX))
    return -MGMT_HOST_ERR_NO_REPLY;

  return 0;
}

int mgmt_host_config_write(int port, uint8_t group, uint8_t block, const uint8_t data[MGMT_CONFIG_BLOCK_SIZE],
                           uint8_t *result)
{
  // Build the CONFIG_WRITE command
  uint8_t command[MGMT_CMD_CONFIG_WRITE_TX] = {MGMT_CMD_CONFIG_WRITE, group, block};

  // Copy the data to write into the command
  memcpy(&command[3], data, MGMT_CONFIG_BLOCK_SIZE);

  // Execute the command and check for a reply
  if (!exec(port, command, sizeof(command), result, MGMT_CMD_CONFIG_WRITE_RX))
    return -MGMT_HOST_ERR_NO_REPLY;

  return 0;
}

int mgmt_host_data_write(int port, uint8_t group, uint16_t block, const uint8_t data[MGMT_DATA_BLOCK_SIZE])
{
  // Build the DATA_WRITE command
  uint8_t command[MGMT_CMD_DATA_WRITE_TX] = {MGMT_CMD_DATA_WRITE, group, block >> 8, block & 0xFF};

  // Copy the data to write into the command
  memcpy(&command[4], data, MGMT_DATA_BLOCK_SIZE);

  // Execute the command and check for a reply
  uint8_t crc;
  if (!exec(port, command, sizeof(command), &crc, MGMT_CMD_DATA_WRITE_RX))
    return -MGMT_HOST_ERR_NO_REPLY;

  // Check the CRC against the expected value
  uint8_t expected = joybus_accessory_calculate_data_crc(data);
  if (crc == expected)
    return 0;

  // Check if the CRC is the complement of the expected value
  if (crc == (uint8_t)(expected ^ 0xFF))
    return -MGMT_HOST_ERR_REJECTED;

  // The CRC is invalid
  return -MGMT_HOST_ERR_BAD_CRC;
}
