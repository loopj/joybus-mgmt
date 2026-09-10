# Joybus Device Management Protocol

A Joybus command set for identifying, configuring and updating microcontrollers from N64, GameCube, and Wii homebrew.

Includes a [libjoybus](https://github.com/loopj/libjoybus) target implementation.

## Lock Model

The management layer is gated behind a handshake to prevent collisions with other Joybus devices that may use the same opcode space.

- The management command set starts **locked** at boot
- While locked the only command that answers is `0x60` with the magic number `0x4A53`
- Every other `0x6x` opcode, and `0x60` with wrong or absent magic number, gets **no reply at all**
- An `IDENTIFY` with the correct magic number **unlocks** the remaining `0x6x` commands
- `CTRL(GROUP_SYSTEM, LOCK)` or a reboot returns the command set to the locked state

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

Get the device's vendor, model, variant and firmware version, and unlock the rest of the command set.

```
Command  (3): {0x60, 0x4A, 0x53}
Response (8): {0x4A, 0x53, vendor, model, variant, version[3]}
```

The response fields are:

| Byte | Field     | Meaning                                                              |
|-----:|-----------|----------------------------------------------------------------------|
|  0-1 | `magic`   | Always `0x4A53`. A host **must** validate this and abort on mismatch |
|    2 | `vendor`  | Who makes this device, see [Vendors](#vendors)                       |
|    3 | `model`   | What kind of device it is, unique per vendor                         |
|    4 | `variant` | Further detail about the device, defined by the vendor               |
|  5-7 | `version` | Firmware version, as major, minor, patch                             |

### CTRL

Send a command to a group, answering with a [result code](#result-codes). Triggers an action, for example beginning a firmware update or capturing a calibration pose. One argument byte is always present, ignored by verbs that do not use it.

Verbs are group-local, so the same value means different things in different groups.

```
Command  (4): {0x61, group, verb, arg}
Response (1): {result}
```

### STATUS

Get the status of a group. Reports live state rather than stored settings, for example how far a firmware update has got.

```
Command  (2): {0x62, group}
Response (8): {byte0..byte7}
```

The response has no dedicated error field, so a group the device does not have, or one that reports no status, reads back all zeros.

### CONFIG_READ / CONFIG_WRITE

Read an 8-byte configuration data block from a group's non-volatile configuration storage.

```
Command  (3):  {0x63, group, block}
Response (8):  {byte0..byte7}
```

Write an 8-byte configuration data block to a group's non-volatile configuration storage, answering with a [result code](#result-codes).

```
Command  (11): {0x64, group, block, byte0..byte7}
Response (1):  {result}
```

A group and block pair is a logical address, not a direct storage offset. The device translates it to a physical flash or EEPROM location.

A read has no dedicated error field, so an unknown group, a group with no configuration, or a block outside its range all read back as zeros.

Configuration larger than a single block is transferred as a [config record](#config-records).

### DATA_WRITE

Write 32 data bytes to a group at the specified offset. Intended for streaming data such as firmware updates. The group decides what the address means, what ordering it requires, and whether it is in a state to accept data at all.

```
Command  (36): {0x65, group, addr_hi, addr_lo, byte0..byte31}
Response (1):  {crc8}
```

The 16-bit address range gives a maximum of 65,536 (2^16) blocks of 32 bytes, or a maximum image size of 2 MB.

The response **crc8** is the device's CRC8 (polynomial `0x85`) over the 32 received data bytes. The host compares it against its own and retransmits that block on mismatch. It is the same checksum N64 Controller Pak transfers use, so implementations already exist, as `joybus_data_checksum()` in libjoybus and `joybus_accessory_calculate_data_crc()` in libdragon.

If the group rejects the write, for example because it does not accept streamed data or is not in a state to receive any, the device responds with the payload crc8 **XOR `0xFF`** so the host can detect the rejection.

### Result Codes

`CTRL` and `CONFIG_WRITE` both answer with a result byte. A host that does not recognize a code should still treat it as a failure. Codes from `0x80` up are device defined, and only mean anything to a host that recognizes the vendor and model.

| Code   | Name        | Meaning                                                                   |
|--------|-------------|---------------------------------------------------------------------------|
| `0x00` | success     | The command was carried out                                               |
| `0x01` | unsupported | The group, or the command within it, is not implemented                   |
| `0x02` | bad state   | Understood, but not while the device is in its current state              |
| `0x03` | bad address | No such address in this group                                             |
| `0x04` | bad value   | The address was fine, the value was not                                   |
| `0x05` | failed      | The device could not complete the command, such as a failed storage write |
| `0x06` | busy        | Busy with something else, so worth retrying                               |

### Config Records

A record is a config value with its own address, occupying as many whole blocks as it needs. For example, a 10-byte record takes two blocks, leaving six bytes spare.

Put each record's address and payload size in a header shared between device firmware and host tools, so both sides agree on the layout. The macros `MGMT_CONFIG_RECORD_BLOCKS(payload)` and `MGMT_CONFIG_RECORD_BYTES(payload)` are provided to help calculate a record's block count and total size in bytes.

## Groups

All commands except `IDENTIFY` take a `group` parameter, selecting which subsystem the command applies to.

### Built-in Groups

Group numbers below `0x80` are reserved for built-in groups.

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

## Vendors

The `vendor` byte of the `IDENTIFY` response says who makes the device. Model and variant are defined per vendor, so a host needs the vendor before the rest of the identity means anything.

Vendor IDs below `0x80` are reserved.

| ID     | Vendor                                                   |
|--------|----------------------------------------------------------|
| `0x01` | [Joystamp](https://github.com/loopj/joystamp)            |
| `0x02` | [WavePhoenix](https://github.com/loopj/wavephoenix)      |

Custom devices should use an ID from `0x80` (`MGMT_VENDOR_CUSTOM`) up. Open a pull request to reserve a new ID below `0x80`.

## Layout

Two libraries ship from this repo.

| Target               | Contents                                     | Dependencies               |
|----------------------|----------------------------------------------|----------------------------|
| `joybus_mgmt`        | Wire protocol definitions, `mgmt/*.h`        | none                       |
| `joybus_mgmt_target` | Generic management target, `mgmt/target*.h`  | `joybus_mgmt`, `libjoybus` |

Homebrew and host tools link `joybus_mgmt` and get headers only. Device firmware links `joybus_mgmt_target` and inherits the protocol headers with it.

## License

joybus-mgmt is released under the MIT license. See [LICENSE](LICENSE).
