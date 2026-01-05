/**
 * SpeedCAN - SLCAN USB-to-CAN Adapter Firmware
 *
 * Main application entry point for Arduino Uno R4 WiFi.
 *
 * Target Hardware:
 *   - Arduino Uno R4 WiFi (Renesas RA4M1)
 *   - SN65HVD230 CAN transceiver
 *
 * Connections:
 *   - D13 (CANRX0) -> CAN transceiver CANRX
 *   - D10 (CANTX0) -> CAN transceiver CANTX
 *   - 3.3V -> CAN transceiver VCC
 *   - GND -> CAN transceiver GND
 *
 * Usage:
 *   Compatible with python-can's slcan interface:
 *   >>> import can
 *   >>> bus = can.Bus(interface='slcan', channel='/dev/ttyACM0', bitrate=500000)
 */

#include <Arduino.h>
#include "config.h"
#include "SerialTransport.h"
#include "RA4M1CAN.h"
#include "SLCAN.h"
#include "ProtocolDispatcher.h"

// =============================================================================
// Global Objects
// =============================================================================

// Transport layer - USB CDC serial
SerialTransport transport;

// CAN backend - RA4M1 hardware CAN controller
RA4M1CAN canBackend;

// SLCAN protocol handler
SLCAN slcan(canBackend);

// Protocol dispatcher for command routing
ProtocolDispatcher dispatcher;

// Buffers for command processing
static char cmdBuffer[CMD_BUFFER_SIZE];
static char responseBuffer[RESPONSE_BUFFER_SIZE];

// =============================================================================
// Setup
// =============================================================================

void setup() {
    // Initialize serial transport (USB CDC)
    transport.begin(SERIAL_BAUD_RATE);

    // Register SLCAN as the protocol handler
    dispatcher.registerHandler(&slcan);

    DEBUG_PRINTLN(FIRMWARE_NAME " v" + String(FIRMWARE_VERSION_MAJOR) + "." + String(FIRMWARE_VERSION_MINOR));
    DEBUG_PRINTLN("SLCAN USB-to-CAN adapter ready");
    DEBUG_PRINTLN("Supported bitrates: S4(125k), S5(250k), S6(500k), S8(1000k)");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    // Process ALL queued commands (up to a limit to prevent CAN starvation)
    uint8_t cmdsProcessed = 0;

    while (cmdsProcessed < MAX_CMDS_PER_LOOP) {
        if (!transport.readLine(cmdBuffer, sizeof(cmdBuffer))) {
            break;  // Queue empty
        }

        // Dispatch command to appropriate handler
        bool hasResponse = dispatcher.dispatch(cmdBuffer, responseBuffer, sizeof(responseBuffer));

        if (hasResponse) {
            // Send response with high priority (10ms timeout)
            if (responseBuffer[0] != '\0') {
                transport.writeWithPriority(responseBuffer, strlen(responseBuffer),
                                           WritePriority::COMMAND_RESPONSE);
            }
            // Send CR terminator also with high priority
            char cr = '\r';
            transport.writeWithPriority(&cr, 1, WritePriority::COMMAND_RESPONSE);
        }

        cmdsProcessed++;
    }

    // Poll handlers for async operations (e.g., forwarding received CAN frames)
    dispatcher.pollAll(&transport);
}
