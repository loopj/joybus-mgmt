/**
 * Transport for the management host.
 *
 * The whole host interface is written in terms of this one function, so a
 * platform is supported by implementing it and nothing else. Private to the
 * library, not part of the public interface.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <mgmt/host.h>

/**
 * Send one command to a port and read its reply, blocking until it completes.
 *
 * The response buffer is left untouched unless the device answered.
 *
 * @param port the port to send to
 * @param command the command bytes, opcode first
 * @param command_len how many command bytes to send
 * @param response buffer to store the reply in
 * @param response_len how many reply bytes to expect
 * @return 0 on success, a negative mgmt_host_error on failure
 */
int mgmt_host_transfer(int port, const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len);
