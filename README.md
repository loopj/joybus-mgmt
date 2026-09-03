# Joybus Device Management Protocol

A Joybus command set for identifying, configuring and updating microcontrollers from N64, GameCube, and Wii homebrew.

Includes a [libjoybus](https://github.com/loopj/libjoybus) target implementation.

## Lock Model

The management layer is gated behind a handshake to prevent collisions with other Joybus devices that may use the same opcode space.

- The device boots **locked**
- While locked the device answers exactly one thing, `0x60` with the magic number `0x4A53`
- Every other `0x6x` opcode, and `0x60` with wrong or absent magic number, gets **no reply at all**
- An `IDENTIFY` with the correct magic number **unlocks** the remaining `0x6x` commands
- `CTRL(GROUP_SYSTEM, LOCK)` or a reboot returns the device to the locked state

## Protocol

The management protocol introduces the following Joybus command opcodes, which are a non-standard extension of the Joybus protocol.

| Opcode | Name                                         | Cmd len | Resp len |
|--------|----------------------------------------------|--------:|---------:|
| `0x60` | [`IDENTIFY`](#identify)                      |       3 |        8 |
| `0x61` | [`CTRL`](#ctrl)                              |       4 |        1 |
| `0x62` | [`STATUS`](#status)                          |       2 |        8 |
| `0x63` | [`CONFIG_READ`](#config_read--config_write)  |       3 |        8 |
| `0x64` | [`CONFIG_WRITE`](#config_read--config_write) |      11 |        1 |
| `0x65` | [`DATA_WRITE`](#data_write)                  |      36 |        1 |

### IDENTIFY

Get the device's hardware ID and firmware version.

```
Command  (3): {0x60, magic_hi, magic_lo}
Response (8): {magic_hi, magic_lo, hardware_id, version_major, version_minor, version_patch, reserved x2}
```

| Field                                           | Meaning                                                             |
|-------------------------------------------------|---------------------------------------------------------------------|
| `magic_hi` `magic_lo`                           | Fixed `0x4A53`. A host **must** validate this and abort on mismatch |
| `hardware_id`                                   | Board identifier                                                    |
| `version_major` `version_minor` `version_patch` | Firmware version                                                    |
| `reserved`                                      | Reads zero                                                          |

### CTRL

Send a command to a group.

```
Command  (4): {0x61, group, command, arg}
Response (1): {result}
```

A device that does not recognize the `group` or the `command` responds with `0x01` (unsupported).

### STATUS

Get the status of a group.

```
Command  (2): {0x62, group}
Response (8): {byte0..byte7}
```

### CONFIG_READ / CONFIG_WRITE

Read an 8-byte configuration data block from a group's non-volatile configuration storage.

```
Command  (3):  {0x63, group, block}
Response (8):  {byte0..byte7}
```

Write an 8-byte configuration data block to a group's non-volatile configuration storage.

```
Command  (11): {0x64, group, block, byte0..byte7}
Response (1):  {result}
```

A group and block pair is a logical address, not a direct storage offset. The device translates it to a physical flash or EEPROM location.

### DATA_WRITE

Write 32 data bytes to a group at the specified offset. Intended for streaming data such as firmware updates, where data is sent in sequential blocks.

```
Command  (36): {0x65, group, addr_hi, addr_lo, byte0..byte31}
Response (1):  {crc8}
```

The 16-bit address range gives a maximum of 65,536 (2^16) blocks of 32 bytes, or a maximum image size of 2 MB.

The response **crc8** is the device's CRC8 (polynomial `0x85`) over the 32 received data bytes. The host compares it against its own and retransmits that block on mismatch.

If the device rejects the write, for example if the group does not accept streamed data, the address does not match the expected next block, or the group isn't ready to receive data, it responds with the payload crc8 **XOR `0xFF`** so the host can detect the rejection.

## Groups

All commands except `IDENTIFY` take a `group` parameter, selecting which subsystem the command applies to.

### Built-in Groups

Group numbers `0x00 - 0x0F` are reserved for built-in groups.

The `SYSTEM` group (0x00) provides a single built-in command `0x00` (`LOCK`), which [locks the management protocol](#lock-model).

### Custom Groups

Custom groups can have their own namespaced commands, status, and configuration.

For example, we could define a custom group `GROUP_DFU` (0x10) for firmware updates, and a custom group `GROUP_INPUT` (0x11) for stick/trigger calibration.

#### Firmware update

```
STATUS(GROUP_DFU)           // Check DFU status
CTRL(GROUP_DFU, DFU_BEGIN)  // Begin DFU update
STATUS(GROUP_DFU)           // Wait for device to be ready for data
DATA_WRITE(GROUP_DFU, ...)  // Write firmware data chunk
...
CTRL(GROUP_DFU, DFU_APPLY)  // Apply DFU update and reboot
```

#### Calibrate a stick/trigger

```
STATUS(GROUP_INPUT)                       // Check how many sticks/triggers are present
CTRL(GROUP_INPUT, CAL_BEGIN, 0)           // Begin calibration on stick/trigger 0
CTRL(GROUP_INPUT, CAL_CAPTURE, POSE_REST) // Capture resting position
CTRL(GROUP_INPUT, CAL_CAPTURE, POSE_UP)   // Capture "stick up" position
...
CTRL(GROUP_INPUT, CAL_COMMIT, 0)          // Commit calibration data (arg ignored)
```

#### Read stick shaping config

```
CONFIG_READ(GROUP_INPUT, SHAPE_DATA + n * 2)     // Read bytes 0-7 of shaping data for input n
CONFIG_READ(GROUP_INPUT, SHAPE_DATA + n * 2 + 1) // Read bytes 8-15 of shaping data for input n
```

#### Write stick shaping config

```
CONFIG_WRITE(GROUP_INPUT, SHAPE_DATA + n * 2, ...)      // Write bytes 0-7 of shaping config for input n
CONFIG_WRITE(GROUP_INPUT, SHAPE_DATA + n * 2 + 1, ...)  // Write bytes 8-15 of shaping config for input n
```

## Layout

Two libraries ship from this repo.

| Target               | Contents                                     | Dependencies               |
|----------------------|----------------------------------------------|----------------------------|
| `joybus_mgmt`        | Wire protocol definitions, `mgmt/*.h`        | none                       |
| `joybus_mgmt_target` | Generic management target, `mgmt/target/*.h` | `joybus_mgmt`, `libjoybus` |

Homebrew and host tools link `joybus_mgmt` and get headers only. Device firmware links `joybus_mgmt_target` and inherits the protocol headers with it.

## License

joybus-mgmt is released under the MIT license. See [LICENSE](LICENSE).
