/**
 * libogc transport for the management host.
 *
 * Hands each command to SI_Transfer() and waits for its callback, since libogc
 * offers no synchronous form. The buffers are bounced through word-aligned
 * locals because SI moves them a word at a time.
 */

#include <string.h>

#include <ogc/si.h>

#include <mgmt/host.h>

#include "transfer.h"

// SI encodes each transfer length in seven bits, so 128 bytes is the most it
// can carry either way. Sizing the buffers to that means any transfer the
// hardware can perform fits, however long a command gets.
#define SI_MAX_BYTES 128
#define SI_MAX_WORDS (SI_MAX_BYTES / 4)

// Filled in by the callback when the exchange finishes
static volatile int transfer_done;
static volatile u32 transfer_status;

// Trampoline, run from the SI interrupt once the exchange completes
static void transfer_callback(s32 chan, u32 status)
{
  (void)chan;

  transfer_status = status;
  transfer_done   = 1;
}

int mgmt_host_transfer(int port, const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len)
{
  // Word aligned, so SI can move them a word at a time
  u32 out[SI_MAX_WORDS] = {0};
  u32 in[SI_MAX_WORDS]  = {0};

  memcpy(out, command, command_len);

  // Start the exchange, which is refused outright if the channel is occupied
  transfer_done = 0;
  if (!SI_Transfer(port, out, command_len, in, response_len, transfer_callback, 0))
    return -MGMT_HOST_ERR_TRANSFER;

  // Wait for the trampoline
  while (!transfer_done)
    ;

  // Nothing on the port answered
  if (transfer_status & SI_ERROR_NO_RESPONSE)
    return -MGMT_HOST_ERR_NO_REPLY;

  // A collision, an over run or an under run
  if (transfer_status)
    return -MGMT_HOST_ERR_TRANSFER;

  // Copy the reply out of the aligned buffer
  memcpy(response, in, response_len);

  return 0;
}
