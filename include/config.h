/**
 * SpeeduinoR4 Configuration
 *
 * Global configuration settings for the SLCAN USB-to-CAN adapter firmware.
 * Target: Arduino Uno R4 WiFi with SN65HVD230 CAN transceiver
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// Firmware Version
// =============================================================================

#define FIRMWARE_VERSION_MAJOR  1
#define FIRMWARE_VERSION_MINOR  2
#define FIRMWARE_NAME           "SpeeduinoR4"
#define FIRMWARE_HW_VERSION     "1.0"   // Hardware revision string

// =============================================================================
// Serial Configuration
// =============================================================================

#define SERIAL_BAUD_RATE        1000000
#define CMD_BUFFER_SIZE         64      // Max command line length
#define RESPONSE_BUFFER_SIZE    64      // Max response line length

// Serial RX buffering (for SerialTransport)
#define SERIAL_RX_BUFFER_SIZE   256     // Per-line buffer capacity
#define SERIAL_CMD_QUEUE_SIZE   4       // Number of commands to queue
#define MAX_CMDS_PER_LOOP       4       // Max commands processed per loop iteration

// =============================================================================
// CAN Configuration
// =============================================================================

// Pin assignments (Arduino Uno R4 WiFi specific)
// Note: These are fixed by the RA4M1 hardware
#define CAN_RX_PIN              13      // D13/CANRX0
#define CAN_TX_PIN              10      // D10/CANTX0

// Default CAN bitrate (used if not set via SLCAN command)
// Options: BR_125K, BR_250K, BR_500K, BR_1000K
#define DEFAULT_CAN_BITRATE     6       // S6 = 500 Kbps

// CAN RX buffering (protocol layer - in SLCAN)
#define CAN_RX_QUEUE_SIZE       128      // Ring buffer capacity
#define MAX_FRAMES_PER_POLL     6       // Max frames forwarded per loop iteration

// CAN TX buffering (backend layer - in RA4M1CAN)
#define CAN_TX_QUEUE_SIZE       24      // Software TX queue capacity

// =============================================================================
// Feature Flags
// =============================================================================

#define ENABLE_TIMESTAMPS       0       // Support Z0/Z1 timestamp commands
#define ENABLE_STATUS_LED       1       // Blink LED_BUILTIN on TX/RX activity
#define ENABLE_HW_FILTERS       1       // Hardware acceptance filtering (M/m commands)
#define AUTO_FORWARD_RX         1       // Auto-forward received CAN frames to host

// =============================================================================
// LED Configuration
// =============================================================================

#if ENABLE_STATUS_LED
#define LED_PIN                 LED_BUILTIN
#define LED_TX_BLINK_MS        50       // TX activity blink duration (ms)
#define LED_RX_BLINK_MS        50       // RX activity blink duration (ms)
#endif

// =============================================================================
// Protocol Settings
// =============================================================================

// Maximum number of protocol handlers that can be registered
#define MAX_PROTOCOL_HANDLERS   4

// =============================================================================
// Debug Configuration
// =============================================================================

#define DEBUG_SERIAL            0       // Set to 1 for debug output on Serial

#if DEBUG_SERIAL
#define DEBUG_PRINT(x)          Serial.print(x)
#define DEBUG_PRINTLN(x)        Serial.println(x)
#define DEBUG_PRINTF(...)       Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

// =============================================================================
// SLCAN Protocol Constants
// =============================================================================

// Response characters
#define SLCAN_OK                '\r'    // Success response
#define SLCAN_ERROR             '\x07'  // Bell character - error response

// Supported bitrate presets (S command)
// S4 = 125k, S5 = 250k, S6 = 500k, S8 = 1000k
// S0-S3, S7 return error (unsupported by Arduino_CAN library)

#endif // CONFIG_H
