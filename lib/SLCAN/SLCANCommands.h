/**
 * SLCAN Command Definitions
 *
 * SLCAN protocol command and response definitions.
 * Based on the Lawicel SLCAN protocol specification.
 *
 * Compatible with python-can's slcan interface.
 */

#ifndef SLCAN_COMMANDS_H
#define SLCAN_COMMANDS_H

// =============================================================================
// SLCAN Command Characters
// =============================================================================

// Configuration commands
#define SLCAN_CMD_SETUP         'S'     // Set bitrate (S0-S8)
#define SLCAN_CMD_SETUP_BTR     's'     // Set custom bit timing registers
#define SLCAN_CMD_OPEN          'O'     // Open CAN channel (normal mode)
#define SLCAN_CMD_LISTEN        'L'     // Open CAN channel (listen-only mode)
#define SLCAN_CMD_CLOSE         'C'     // Close CAN channel

// Transmit commands
#define SLCAN_CMD_TX_STD        't'     // Transmit standard frame (11-bit ID)
#define SLCAN_CMD_TX_EXT        'T'     // Transmit extended frame (29-bit ID)
#define SLCAN_CMD_TX_RTR_STD    'r'     // Transmit standard RTR frame
#define SLCAN_CMD_TX_RTR_EXT    'R'     // Transmit extended RTR frame

// Status and info commands
#define SLCAN_CMD_STATUS        'F'     // Read status flags
#define SLCAN_CMD_VERSION       'V'     // Get firmware version
#define SLCAN_CMD_SERIAL        'N'     // Get serial number

// Feature commands
#define SLCAN_CMD_TIMESTAMP     'Z'     // Enable/disable timestamps (Z0/Z1)
#define SLCAN_CMD_FILTER_MASK   'M'     // Set acceptance filter mask
#define SLCAN_CMD_FILTER_CODE   'm'     // Set acceptance filter code

// Extended commands (non-standard but common)
#define SLCAN_CMD_AUTOPOLL      'X'     // Set auto-poll/send mode (X0/X1)
#define SLCAN_CMD_POLL          'P'     // Poll for single CAN frame
#define SLCAN_CMD_POLL_ALL      'A'     // Poll for all pending CAN frames

// =============================================================================
// SLCAN Response Characters
// =============================================================================

#define SLCAN_OK                '\r'    // Success response (CR)
#define SLCAN_ERROR             '\x07'  // Error response (BELL)

// Response prefixes for transmit confirmations
#define SLCAN_TX_OK_STD         'z'     // Standard frame transmitted OK
#define SLCAN_TX_OK_EXT         'Z'     // Extended frame transmitted OK

// =============================================================================
// SLCAN Bitrate Presets (S command)
// =============================================================================

// S0 = 10 Kbps   - NOT SUPPORTED
// S1 = 20 Kbps   - NOT SUPPORTED
// S2 = 50 Kbps   - NOT SUPPORTED
// S3 = 100 Kbps  - NOT SUPPORTED
// S4 = 125 Kbps  - SUPPORTED
// S5 = 250 Kbps  - SUPPORTED
// S6 = 500 Kbps  - SUPPORTED
// S7 = 800 Kbps  - NOT SUPPORTED
// S8 = 1000 Kbps - SUPPORTED

#define SLCAN_BITRATE_10K       0
#define SLCAN_BITRATE_20K       1
#define SLCAN_BITRATE_50K       2
#define SLCAN_BITRATE_100K      3
#define SLCAN_BITRATE_125K      4
#define SLCAN_BITRATE_250K      5
#define SLCAN_BITRATE_500K      6
#define SLCAN_BITRATE_800K      7
#define SLCAN_BITRATE_1000K     8

// =============================================================================
// SLCAN Frame Format
// =============================================================================

/*
 * Standard frame (11-bit ID):
 *   tiiildd...
 *   t    = command character
 *   iii  = 3 hex digits for ID (000-7FF)
 *   l    = 1 hex digit for DLC (0-8)
 *   dd.. = 0-16 hex digits for data (2 per byte)
 *
 * Extended frame (29-bit ID):
 *   Tiiiiiiiildd...
 *   T        = command character
 *   iiiiiiii = 8 hex digits for ID (00000000-1FFFFFFF)
 *   l        = 1 hex digit for DLC (0-8)
 *   dd..     = 0-16 hex digits for data
 *
 * RTR frames use 'r' or 'R' and have no data bytes.
 *
 * Received frames (when timestamps enabled):
 *   tiiildd..tttt or Tiiiiiiiildd..tttt
 *   tttt = 4 hex digits for timestamp (0000-FFFF ms)
 */

// Frame format lengths
#define SLCAN_STD_ID_LEN        3       // 3 hex chars for 11-bit ID
#define SLCAN_EXT_ID_LEN        8       // 8 hex chars for 29-bit ID
#define SLCAN_DLC_LEN           1       // 1 hex char for DLC
#define SLCAN_DATA_CHAR_LEN     2       // 2 hex chars per data byte
#define SLCAN_TIMESTAMP_LEN     4       // 4 hex chars for timestamp

// Maximum frame string lengths
#define SLCAN_MAX_STD_FRAME_LEN (1 + SLCAN_STD_ID_LEN + SLCAN_DLC_LEN + 16 + SLCAN_TIMESTAMP_LEN + 1)
#define SLCAN_MAX_EXT_FRAME_LEN (1 + SLCAN_EXT_ID_LEN + SLCAN_DLC_LEN + 16 + SLCAN_TIMESTAMP_LEN + 1)

// =============================================================================
// Status Flags (F command response)
// =============================================================================

/*
 * Status flags byte (Fxx response):
 *   Bit 0: RX FIFO full
 *   Bit 1: TX FIFO full
 *   Bit 2: Error warning (TEC/REC > 96)
 *   Bit 3: Data overrun
 *   Bit 4: Reserved
 *   Bit 5: Error passive (TEC/REC > 127)
 *   Bit 6: Arbitration lost
 *   Bit 7: Bus error
 */

#define SLCAN_STATUS_RX_FULL        0x01
#define SLCAN_STATUS_TX_FULL        0x02
#define SLCAN_STATUS_ERR_WARNING    0x04
#define SLCAN_STATUS_DATA_OVERRUN   0x08
#define SLCAN_STATUS_RESERVED       0x10
#define SLCAN_STATUS_ERR_PASSIVE    0x20
#define SLCAN_STATUS_ARB_LOST       0x40
#define SLCAN_STATUS_BUS_ERROR      0x80

#endif // SLCAN_COMMANDS_H
