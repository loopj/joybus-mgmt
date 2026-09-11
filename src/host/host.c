/**
 * Joybus management host.
 *
 * Every command of the host interface, laid out as command bytes and handed to
 * the platform's transport. Nothing here touches the bus, so this file is
 * shared by every backend.
 */

#include <string.h>

#include <mgmt/host.h>

#include "transfer.h"

// CRC-8 over a data block, seeded at zero with polynomial 0x85
// From libdragon's joybus_accessory_calculate_data_crc(), public domain
static uint8_t data_crc(const uint8_t data[MGMT_DATA_BLOCK_SIZE])
{
  unsigned crc = 0;

  for (int i = 0; i < MGMT_DATA_BLOCK_SIZE; i++) {
    unsigned x = crc ^ data[i];

    crc = (x & 0x80) ? 0x89 : 0;
    crc ^= (x & 0x40) ? 0x86 : 0;
    crc ^= (x & 0x20) ? 0x43 : 0;
    crc ^= (x & 0x10) ? 0xE3 : 0;
    crc ^= (x & 0x08) ? 0xB3 : 0;
    crc ^= (x & 0x04) ? 0x9B : 0;
    crc ^= (x & 0x02) ? 0x8F : 0;
    crc ^= (x & 0x01) ? 0x85 : 0;
  }

  return (uint8_t)crc;
}

int mgmt_host_identify(int port, struct mgmt_identity *response)
{
  // Build the IDENTIFY command
  const uint8_t command[MGMT_CMD_IDENTIFY_TX] = {MGMT_CMD_IDENTIFY, MGMT_MAGIC_HI, MGMT_MAGIC_LO};

  // Execute the command and check for a reply, the wire layout being the struct layout
  struct mgmt_identity identity;
  int err = mgmt_host_transfer(port, command, sizeof(command), (uint8_t *)&identity, MGMT_CMD_IDENTIFY_RX);
  if (err != 0)
    return err;

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
  int err = mgmt_host_transfer(port, command, sizeof(command), result, MGMT_CMD_CTRL_RX);
  if (err != 0)
    return err;

  return 0;
}

int mgmt_host_status(int port, uint8_t group, uint8_t response[MGMT_STATUS_SIZE])
{
  // Build the STATUS command
  const uint8_t command[MGMT_CMD_STATUS_TX] = {MGMT_CMD_STATUS, group};

  // Execute the command and check for a reply
  int err = mgmt_host_transfer(port, command, sizeof(command), response, MGMT_CMD_STATUS_RX);
  if (err != 0)
    return err;

  return 0;
}

int mgmt_host_config_read(int port, uint8_t group, uint8_t block, uint8_t response[MGMT_CONFIG_BLOCK_SIZE])
{
  // Build the CONFIG_READ command
  const uint8_t command[MGMT_CMD_CONFIG_READ_TX] = {MGMT_CMD_CONFIG_READ, group, block};

  // Execute the command and check for a reply
  int err = mgmt_host_transfer(port, command, sizeof(command), response, MGMT_CMD_CONFIG_READ_RX);
  if (err != 0)
    return err;

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
  int err = mgmt_host_transfer(port, command, sizeof(command), result, MGMT_CMD_CONFIG_WRITE_RX);
  if (err != 0)
    return err;

  return 0;
}

int mgmt_host_data_write(int port, uint8_t group, uint16_t block, const uint8_t data[MGMT_DATA_BLOCK_SIZE])
{
  // Build the DATA_WRITE command
  uint8_t command[MGMT_CMD_DATA_WRITE_TX] = {MGMT_CMD_DATA_WRITE, group, (uint8_t)(block >> 8),
                                             (uint8_t)(block & 0xFF)};

  // Copy the data to write into the command
  memcpy(&command[4], data, MGMT_DATA_BLOCK_SIZE);

  // Execute the command and check for a reply
  uint8_t crc;
  int err = mgmt_host_transfer(port, command, sizeof(command), &crc, MGMT_CMD_DATA_WRITE_RX);
  if (err != 0)
    return err;

  // Check the CRC against the expected value
  uint8_t expected = data_crc(data);
  if (crc == expected)
    return 0;

  // Check if the CRC is the complement of the expected value
  if (crc == (uint8_t)(expected ^ 0xFF))
    return -MGMT_HOST_ERR_REJECTED;

  // The CRC is invalid
  return -MGMT_HOST_ERR_BAD_CRC;
}

int mgmt_host_config_read_record(int port, uint8_t group, uint8_t addr, void *response, size_t size)
{
  // The record arrives one block at a time
  uint8_t *record = response;

  for (size_t offset = 0; offset < size; offset += MGMT_CONFIG_BLOCK_SIZE) {
    // Read the next block of the record
    uint8_t block[MGMT_CONFIG_BLOCK_SIZE];
    int err = mgmt_host_config_read(port, group, addr++, block);
    if (err != 0)
      return err;

    // Copy it in, stopping short of the padding in the last block
    size_t remaining = size - offset;
    memcpy(record + offset, block, remaining < MGMT_CONFIG_BLOCK_SIZE ? remaining : MGMT_CONFIG_BLOCK_SIZE);
  }

  return 0;
}

int mgmt_host_config_write_record(int port, uint8_t group, uint8_t addr, const void *data, size_t size, uint8_t *result)
{
  // The record goes out one block at a time
  const uint8_t *record = data;

  for (size_t offset = 0; offset < size; offset += MGMT_CONFIG_BLOCK_SIZE) {
    // Zeroed, so the padding past the end of the record goes out as zeros
    uint8_t block[MGMT_CONFIG_BLOCK_SIZE] = {0};

    // Fill the block with whatever is left of the record
    size_t remaining = size - offset;
    memcpy(block, record + offset, remaining < MGMT_CONFIG_BLOCK_SIZE ? remaining : MGMT_CONFIG_BLOCK_SIZE);

    // Write the block and check for a reply
    int err = mgmt_host_config_write(port, group, addr++, block, result);
    if (err != 0)
      return err;

    // Stop at the first block the device refuses, leaving the record half written
    if (*result != 0)
      return 0;
  }

  return 0;
}
