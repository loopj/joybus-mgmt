/**
 * libdragon transport for the management host.
 *
 * Builds each command as a PIF block and runs it through joybus_exec(), which
 * blocks until the exchange completes. libdragon's joybus_exec_cmd() is not
 * used because it drops the PIF's error flags, and with them any way to tell
 * a silent port from a device that answered zeros.
 */

#include <string.h>

#include <libdragon.h>

#include <mgmt/host.h>

#include "transport.h"

// Flags the PIF sets in a command's receive length byte after execution
#define PIF_RX_NO_DEVICE 0x80 // Nothing on the port answered
#define PIF_RX_OVERFLOW  0x40 // The reply was longer than the space allocated

// Marks the end of the command list in a PIF block
#define PIF_END_OF_BLOCK 0xFE

// Written to the last byte of the block to ask the PIF to run the commands
#define PIF_RUN_COMMANDS 0x01

// Not joybus_exec_cmd(), which discards the receive length byte and its flags
int mgmt_host_transfer(int port, const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len)
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
    return -MGMT_HOST_ERR_NO_REPLY;

  // Copy the reply out of the block
  memcpy(response, &output[i - response_len], response_len);

  return 0;
}
