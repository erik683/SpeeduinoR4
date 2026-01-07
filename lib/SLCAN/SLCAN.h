/**
 * SLCAN Protocol Handler
 *
 * SLCAN (Serial Line CAN) protocol implementation.
 * Compatible with python-can's slcan interface.
 *
 * Based on the Lawicel SLCAN protocol specification.
 */

#ifndef SLCAN_H
#define SLCAN_H

#include "config.h"
#include "ProtocolHandler.h"
#include "CANBackend.h"
#include "SLCANCommands.h"
#include <stdint.h>

/**
 * SLCAN channel state.
 */
enum class SLCANState : uint8_t {
    Closed,         // Channel closed (default)
    Open,           // Channel open in normal mode
    ListenOnly      // Channel open in listen-only mode
};

/**
 * SLCAN Protocol Handler.
 *
 * Implements the SLCAN protocol for USB-to-CAN communication.
 * Handles command parsing, frame transmission/reception, and
 * state management.
 *
 * Supported commands:
 *   S0-S8  : Set bitrate preset (only S4, S5, S6, S8 supported)
 *   O      : Open channel (normal mode)
 *   L      : Open channel (listen-only mode)
 *   C      : Close channel
 *   t/T    : Transmit standard/extended frame
 *   r/R    : Transmit standard/extended RTR frame
 *   F      : Read status flags
 *   V      : Get version
 *   N      : Get serial number
 *   Z0/Z1  : Disable/enable timestamps
 *   X0/X1  : Disable/enable auto-poll/send
 *   M/m    : Set acceptance filter mask/code
 */
class SLCAN : public IProtocolHandler {
public:
    /**
     * Constructor.
     * @param can Reference to the CAN backend
     */
    explicit SLCAN(ICANBackend& can);

    // IProtocolHandler interface
    const char* getName() const override;
    bool canHandle(const char* cmd) const override;
    bool processCommand(const char* cmd, char* response, size_t maxLen) override;
    void poll(ITransport* transport) override;
    bool isActive() const override;

    // State accessors
    SLCANState getState() const;
    bool isTimestampEnabled() const;

    /**
     * Format a CAN frame as an SLCAN string for transmission to host.
     * @param frame The CAN frame to format
     * @param buffer Output buffer
     * @param maxLen Maximum buffer size
     * @return Number of characters written (excluding null terminator)
     */
    size_t formatFrame(const CANFrame& frame, char* buffer, size_t maxLen);

    /**
     * Get diagnostic counters.
     * @param rxOverflows Output: RX ring buffer overflows
     * @param canRxDrops Output: CAN RX frames dropped due to USB blocking
     */
    void getCounters(uint32_t* rxOverflows, uint32_t* canRxDrops) const;

    /**
     * Reset diagnostic counters.
     */
    void resetCounters();

private:
    ICANBackend& _can;
    SLCANState _state;
    uint8_t _configuredBitrate;     // S command value (0-8)
    bool _timestampEnabled;
    bool _autoForward;              // Runtime auto-forward control (X0/X1)
    uint32_t _filterMask;
    uint32_t _filterCode;

    // Ring buffer for RX frames (protocol layer)
    struct FrameSlot {
        CANFrame msg;
        uint16_t timestamp;
        bool valid;
    };

    FrameSlot _rxQueue[CAN_RX_QUEUE_SIZE];
    uint16_t _rxHead;  // drainFromBackend() writes here
    uint16_t _rxTail;  // poll() reads here

    // Diagnostic counters
    uint32_t _rxOverflowCount;      // RX ring buffer overflows
    uint32_t _canRxDropCount;       // CAN RX frames dropped due to USB blocking

    // LED state for activity indication
#if ENABLE_STATUS_LED
    unsigned long _lastTxLedTime;
    unsigned long _lastRxLedTime;
    bool _ledState;
#endif

    // Command handlers
    bool handleSetup(const char* cmd, char* response);
    bool handleOpen(char* response);
    bool handleListen(char* response);
    bool handleClose(char* response);
    bool handleTransmitStd(const char* cmd, char* response);
    bool handleTransmitExt(const char* cmd, char* response);
    bool handleTransmitRtrStd(const char* cmd, char* response);
    bool handleTransmitRtrExt(const char* cmd, char* response);
    bool handleStatus(char* response);
    bool handleVersion(char* response);
    bool handleSerial(char* response);
    bool handleTimestamp(const char* cmd, char* response);
    bool handleFilterMask(const char* cmd, char* response);
    bool handleFilterCode(const char* cmd, char* response);
    bool handleAutopoll(const char* cmd, char* response);

    // Helper functions
    bool parseFrame(const char* cmd, CANFrame& frame, bool extended, bool rtr);
    uint8_t hexCharToNibble(char c);
    char nibbleToHexChar(uint8_t n);
    bool isHexChar(char c);
    uint32_t parseHex(const char* str, size_t len);
    size_t formatHex(uint32_t value, char* buffer, size_t digits);

    // LED control
#if ENABLE_STATUS_LED
    void blinkTxLed();
    void blinkRxLed();
    void updateLed();
#endif

    // Set error response
    void setError(char* response);
    // Set OK response
    void setOk(char* response);

    // RX ring buffer management
    void drainFromBackend();                             // Pull from backend into ring
    bool getNextRxFrame(CANFrame& frame, uint16_t* timestamp);  // Get next frame from ring
};

#endif // SLCAN_H
