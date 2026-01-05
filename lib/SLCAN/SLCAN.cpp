/**
 * SLCAN Protocol Handler Implementation
 */

#include "SLCAN.h"
#include "Transport.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

SLCAN::SLCAN(ICANBackend& can)
    : _can(can)
    , _state(SLCANState::Closed)
    , _configuredBitrate(SLCAN_BITRATE_500K)  // Default to S6 (500k)
    , _timestampEnabled(false)
    , _autoForward(true)                      // Default: auto-forward enabled
    , _filterMask(0)
    , _filterCode(0)
    , _rxHead(0)
    , _rxTail(0)
    , _rxOverflowCount(0)
    , _canRxDropCount(0)
#if ENABLE_STATUS_LED
    , _lastTxLedTime(0)
    , _lastRxLedTime(0)
    , _ledState(false)
#endif
{
    // Initialize RX ring buffer
    for (uint16_t i = 0; i < CAN_RX_QUEUE_SIZE; i++) {
        _rxQueue[i].valid = false;
        _rxQueue[i].timestamp = 0;
    }

#if ENABLE_STATUS_LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif
}

const char* SLCAN::getName() const {
    return "SLCAN";
}

bool SLCAN::canHandle(const char* cmd) const {
    if (cmd == nullptr || cmd[0] == '\0') {
        return false;
    }

    // SLCAN handles all single-character command prefixes
    char c = cmd[0];
    switch (c) {
        case SLCAN_CMD_SETUP:
        case SLCAN_CMD_SETUP_BTR:
        case SLCAN_CMD_OPEN:
        case SLCAN_CMD_LISTEN:
        case SLCAN_CMD_CLOSE:
        case SLCAN_CMD_TX_STD:
        case SLCAN_CMD_TX_EXT:
        case SLCAN_CMD_TX_RTR_STD:
        case SLCAN_CMD_TX_RTR_EXT:
        case SLCAN_CMD_STATUS:
        case SLCAN_CMD_VERSION:
        case SLCAN_CMD_SERIAL:
        case SLCAN_CMD_TIMESTAMP:
        case SLCAN_CMD_FILTER_MASK:
        case SLCAN_CMD_FILTER_CODE:
            return true;
        default:
            return false;
    }
}

bool SLCAN::processCommand(const char* cmd, char* response, size_t maxLen) {
    if (cmd == nullptr || response == nullptr || maxLen < 2) {
        return false;
    }

    char cmdChar = cmd[0];

    switch (cmdChar) {
        case SLCAN_CMD_SETUP:
            return handleSetup(cmd, response);

        case SLCAN_CMD_SETUP_BTR:
            // Custom BTR not supported - return error
            setError(response);
            return true;

        case SLCAN_CMD_OPEN:
            return handleOpen(response);

        case SLCAN_CMD_LISTEN:
            return handleListen(response);

        case SLCAN_CMD_CLOSE:
            return handleClose(response);

        case SLCAN_CMD_TX_STD:
            return handleTransmitStd(cmd, response);

        case SLCAN_CMD_TX_EXT:
            return handleTransmitExt(cmd, response);

        case SLCAN_CMD_TX_RTR_STD:
            return handleTransmitRtrStd(cmd, response);

        case SLCAN_CMD_TX_RTR_EXT:
            return handleTransmitRtrExt(cmd, response);

        case SLCAN_CMD_STATUS:
            return handleStatus(response);

        case SLCAN_CMD_VERSION:
            return handleVersion(response);

        case SLCAN_CMD_SERIAL:
            return handleSerial(response);

        case SLCAN_CMD_TIMESTAMP:
            return handleTimestamp(cmd, response);

        case SLCAN_CMD_FILTER_MASK:
            return handleFilterMask(cmd, response);

        case SLCAN_CMD_FILTER_CODE:
            return handleFilterCode(cmd, response);

        default:
            setError(response);
            return true;
    }
}

void SLCAN::poll(ITransport* transport) {
    if (transport == nullptr) {
        return;
    }

#if ENABLE_STATUS_LED
    updateLed();
#endif

    // Only forward frames if channel is open
    if (_state == SLCANState::Closed) {
        return;
    }

    if (!_autoForward) {
        // Auto-forward disabled, skip RX processing
        return;
    }

    // Step 1: Drain hardware backend into ring buffer (non-blocking)
    drainFromBackend();

    // Step 2: Forward from ring to serial, rate-limited
    uint8_t framesProcessed = 0;

    while (framesProcessed < MAX_FRAMES_PER_POLL) {
        CANFrame frame;
        uint16_t timestamp;

        if (!getNextRxFrame(frame, &timestamp)) {
            break;  // Ring empty
        }

#if ENABLE_STATUS_LED
        blinkRxLed();
#endif

        // Format frame to SLCAN ASCII
        char buffer[SLCAN_MAX_EXT_FRAME_LEN];
        size_t len = formatFrame(frame, buffer, sizeof(buffer));
        if (len > 0) {
            if (len + 1 > sizeof(buffer)) {
                _canRxDropCount++;
                break;
            }
            buffer[len] = '\r';
            // Attempt write with CAN_RX_FRAME priority (0ms timeout, drop if no space)
            if (!transport->writeWithPriority(buffer, len + 1, WritePriority::CAN_RX_FRAME)) {
                _canRxDropCount++;
                break;  // USB blocked, stop forwarding this iteration (frames stay in ring)
            }
        }

        framesProcessed++;
    }

    // Note: Remaining frames stay in ring for next poll() call
}

bool SLCAN::isActive() const {
    return _state != SLCANState::Closed;
}

SLCANState SLCAN::getState() const {
    return _state;
}

bool SLCAN::isTimestampEnabled() const {
    return _timestampEnabled;
}

// =============================================================================
// Command Handlers
// =============================================================================

bool SLCAN::handleSetup(const char* cmd, char* response) {
    // Format: Sn where n is 0-8
    if (strlen(cmd) < 2) {
        setError(response);
        return true;
    }

    char bitrateChar = cmd[1];
    if (bitrateChar < '0' || bitrateChar > '8') {
        setError(response);
        return true;
    }

    uint8_t bitrate = bitrateChar - '0';

    // Check if bitrate is supported
    CANBitrate canBitrate = static_cast<CANBitrate>(bitrate);
    if (!_can.isBitrateSupported(canBitrate)) {
        setError(response);
        return true;
    }

    // Can only configure while channel is closed
    if (_state != SLCANState::Closed) {
        setError(response);
        return true;
    }

    _configuredBitrate = bitrate;
    setOk(response);
    return true;
}

bool SLCAN::handleOpen(char* response) {
    // Can only open if closed
    if (_state != SLCANState::Closed) {
        setError(response);
        return true;
    }

    CANBitrate bitrate = static_cast<CANBitrate>(_configuredBitrate);
    if (!_can.begin(bitrate, CANMode::Normal)) {
        setError(response);
        return true;
    }

    // Apply filter if configured
    if (_filterMask != 0) {
        _can.setFilter(_filterMask, _filterCode);
    }

    _state = SLCANState::Open;
    setOk(response);
    return true;
}

bool SLCAN::handleListen(char* response) {
    // Can only open if closed
    if (_state != SLCANState::Closed) {
        setError(response);
        return true;
    }

    CANBitrate bitrate = static_cast<CANBitrate>(_configuredBitrate);
    if (!_can.begin(bitrate, CANMode::ListenOnly)) {
        setError(response);
        return true;
    }

    // Apply filter if configured
    if (_filterMask != 0) {
        _can.setFilter(_filterMask, _filterCode);
    }

    _state = SLCANState::ListenOnly;
    setOk(response);
    return true;
}

bool SLCAN::handleClose(char* response) {
    if (_state == SLCANState::Closed) {
        // Already closed - still return OK
        setOk(response);
        return true;
    }

    _can.end();
    _state = SLCANState::Closed;
    setOk(response);
    return true;
}

bool SLCAN::handleTransmitStd(const char* cmd, char* response) {
    // Must be open in normal mode
    if (_state != SLCANState::Open) {
        setError(response);
        return true;
    }

    CANFrame frame;
    if (!parseFrame(cmd, frame, false, false)) {
        setError(response);
        return true;
    }

    if (!_can.write(frame)) {
        setError(response);
        return true;
    }

#if ENABLE_STATUS_LED
    blinkTxLed();
#endif

    // Return 'z' for standard frame TX OK
    response[0] = SLCAN_TX_OK_STD;
    response[1] = '\0';
    return true;
}

bool SLCAN::handleTransmitExt(const char* cmd, char* response) {
    // Must be open in normal mode
    if (_state != SLCANState::Open) {
        setError(response);
        return true;
    }

    CANFrame frame;
    if (!parseFrame(cmd, frame, true, false)) {
        setError(response);
        return true;
    }

    if (!_can.write(frame)) {
        setError(response);
        return true;
    }

#if ENABLE_STATUS_LED
    blinkTxLed();
#endif

    // Return 'Z' for extended frame TX OK
    response[0] = SLCAN_TX_OK_EXT;
    response[1] = '\0';
    return true;
}

bool SLCAN::handleTransmitRtrStd(const char* cmd, char* response) {
    // Must be open in normal mode
    if (_state != SLCANState::Open) {
        setError(response);
        return true;
    }

    CANFrame frame;
    if (!parseFrame(cmd, frame, false, true)) {
        setError(response);
        return true;
    }

    if (!_can.write(frame)) {
        setError(response);
        return true;
    }

#if ENABLE_STATUS_LED
    blinkTxLed();
#endif

    response[0] = SLCAN_TX_OK_STD;
    response[1] = '\0';
    return true;
}

bool SLCAN::handleTransmitRtrExt(const char* cmd, char* response) {
    // Must be open in normal mode
    if (_state != SLCANState::Open) {
        setError(response);
        return true;
    }

    CANFrame frame;
    if (!parseFrame(cmd, frame, true, true)) {
        setError(response);
        return true;
    }

    if (!_can.write(frame)) {
        setError(response);
        return true;
    }

#if ENABLE_STATUS_LED
    blinkTxLed();
#endif

    response[0] = SLCAN_TX_OK_EXT;
    response[1] = '\0';
    return true;
}

bool SLCAN::handleStatus(char* response) {
    CANStatus status = _can.getStatus();

    uint8_t flags = 0;
    if (status.rxFifoFull)      flags |= SLCAN_STATUS_RX_FULL;
    if (status.txFifoFull)      flags |= SLCAN_STATUS_TX_FULL;
    if (status.errorWarning)    flags |= SLCAN_STATUS_ERR_WARNING;
    if (status.dataOverrun)     flags |= SLCAN_STATUS_DATA_OVERRUN;
    if (status.errorPassive)    flags |= SLCAN_STATUS_ERR_PASSIVE;
    if (status.arbitrationLost) flags |= SLCAN_STATUS_ARB_LOST;
    if (status.busError)        flags |= SLCAN_STATUS_BUS_ERROR;

    // Format: Fxx
    response[0] = 'F';
    response[1] = nibbleToHexChar((flags >> 4) & 0x0F);
    response[2] = nibbleToHexChar(flags & 0x0F);
    response[3] = '\0';
    return true;
}

bool SLCAN::handleVersion(char* response) {
    // Format: Vxxyy (hardware version xx, software version yy)
    response[0] = 'V';
    response[1] = nibbleToHexChar((FIRMWARE_VERSION_MAJOR >> 4) & 0x0F);
    response[2] = nibbleToHexChar(FIRMWARE_VERSION_MAJOR & 0x0F);
    response[3] = nibbleToHexChar((FIRMWARE_VERSION_MINOR >> 4) & 0x0F);
    response[4] = nibbleToHexChar(FIRMWARE_VERSION_MINOR & 0x0F);
    response[5] = '\0';
    return true;
}

bool SLCAN::handleSerial(char* response) {
    // Format: Nxxxx (serial number)
    // Use a fixed serial for now - could be made unique per device
    response[0] = 'N';
    response[1] = 'S';
    response[2] = 'C';
    response[3] = 'A';
    response[4] = 'N';
    response[5] = '\0';
    return true;
}

bool SLCAN::handleTimestamp(const char* cmd, char* response) {
    // Format: Z0 or Z1
    if (strlen(cmd) < 2) {
        setError(response);
        return true;
    }

    char val = cmd[1];
    if (val == '0') {
        _timestampEnabled = false;
        setOk(response);
    } else if (val == '1') {
        _timestampEnabled = true;
        setOk(response);
    } else {
        setError(response);
    }
    return true;
}

bool SLCAN::handleFilterMask(const char* cmd, char* response) {
    // Format: Mxxxxxxxx (8 hex digits for 29-bit mask)
    if (strlen(cmd) < 9) {
        setError(response);
        return true;
    }

    // Validate hex digits
    for (int i = 1; i <= 8; i++) {
        if (!isHexChar(cmd[i])) {
            setError(response);
            return true;
        }
    }

    _filterMask = parseHex(cmd + 1, 8);

    // Apply filter if channel is open
    if (_state != SLCANState::Closed) {
        _can.setFilter(_filterMask, _filterCode);
    }

    setOk(response);
    return true;
}

bool SLCAN::handleFilterCode(const char* cmd, char* response) {
    // Format: mxxxxxxxx (8 hex digits for 29-bit filter code)
    if (strlen(cmd) < 9) {
        setError(response);
        return true;
    }

    // Validate hex digits
    for (int i = 1; i <= 8; i++) {
        if (!isHexChar(cmd[i])) {
            setError(response);
            return true;
        }
    }

    _filterCode = parseHex(cmd + 1, 8);

    // Apply filter if channel is open
    if (_state != SLCANState::Closed) {
        _can.setFilter(_filterMask, _filterCode);
    }

    setOk(response);
    return true;
}

// =============================================================================
// Frame Parsing and Formatting
// =============================================================================

bool SLCAN::parseFrame(const char* cmd, CANFrame& frame, bool extended, bool rtr) {
    size_t idLen = extended ? SLCAN_EXT_ID_LEN : SLCAN_STD_ID_LEN;
    size_t minLen = 1 + idLen + SLCAN_DLC_LEN;  // cmd + id + dlc

    if (strlen(cmd) < minLen) {
        return false;
    }

    // Parse ID
    const char* idStr = cmd + 1;
    for (size_t i = 0; i < idLen; i++) {
        if (!isHexChar(idStr[i])) {
            return false;
        }
    }
    frame.id = parseHex(idStr, idLen);

    // Validate ID range
    if (!extended && frame.id > 0x7FF) {
        return false;
    }
    if (extended && frame.id > 0x1FFFFFFF) {
        return false;
    }

    // Parse DLC
    const char* dlcStr = cmd + 1 + idLen;
    if (!isHexChar(dlcStr[0])) {
        return false;
    }
    frame.dlc = hexCharToNibble(dlcStr[0]);
    if (frame.dlc > 8) {
        return false;
    }

    frame.extended = extended;
    frame.rtr = rtr;

    // Parse data bytes (not for RTR frames)
    if (!rtr) {
        const char* dataStr = cmd + 1 + idLen + SLCAN_DLC_LEN;
        size_t expectedDataLen = frame.dlc * 2;

        if (strlen(dataStr) < expectedDataLen) {
            return false;
        }

        for (uint8_t i = 0; i < frame.dlc; i++) {
            if (!isHexChar(dataStr[i * 2]) || !isHexChar(dataStr[i * 2 + 1])) {
                return false;
            }
            frame.data[i] = (hexCharToNibble(dataStr[i * 2]) << 4) |
                            hexCharToNibble(dataStr[i * 2 + 1]);
        }
    }

    // Zero remaining data bytes
    for (uint8_t i = frame.dlc; i < 8; i++) {
        frame.data[i] = 0;
    }

    return true;
}

size_t SLCAN::formatFrame(const CANFrame& frame, char* buffer, size_t maxLen) {
    size_t pos = 0;

    // Determine command character
    char cmdChar;
    if (frame.rtr) {
        cmdChar = frame.extended ? SLCAN_CMD_TX_RTR_EXT : SLCAN_CMD_TX_RTR_STD;
    } else {
        cmdChar = frame.extended ? SLCAN_CMD_TX_EXT : SLCAN_CMD_TX_STD;
    }

    if (pos >= maxLen) return 0;
    buffer[pos++] = cmdChar;

    // Format ID
    size_t idDigits = frame.extended ? SLCAN_EXT_ID_LEN : SLCAN_STD_ID_LEN;
    if (pos + idDigits >= maxLen) return 0;
    pos += formatHex(frame.id, buffer + pos, idDigits);

    // Format DLC
    if (pos >= maxLen) return 0;
    buffer[pos++] = nibbleToHexChar(frame.dlc);

    // Format data bytes (not for RTR)
    if (!frame.rtr) {
        for (uint8_t i = 0; i < frame.dlc && pos + 2 < maxLen; i++) {
            buffer[pos++] = nibbleToHexChar((frame.data[i] >> 4) & 0x0F);
            buffer[pos++] = nibbleToHexChar(frame.data[i] & 0x0F);
        }
    }

    // Format timestamp if enabled
    if (_timestampEnabled && pos + SLCAN_TIMESTAMP_LEN < maxLen) {
        pos += formatHex(frame.timestamp, buffer + pos, SLCAN_TIMESTAMP_LEN);
    }

    // Null terminate
    if (pos < maxLen) {
        buffer[pos] = '\0';
    }

    return pos;
}

// =============================================================================
// Helper Functions
// =============================================================================

uint8_t SLCAN::hexCharToNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

char SLCAN::nibbleToHexChar(uint8_t n) {
    n &= 0x0F;
    return (n < 10) ? ('0' + n) : ('A' + n - 10);
}

bool SLCAN::isHexChar(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

uint32_t SLCAN::parseHex(const char* str, size_t len) {
    uint32_t value = 0;
    for (size_t i = 0; i < len; i++) {
        value = (value << 4) | hexCharToNibble(str[i]);
    }
    return value;
}

size_t SLCAN::formatHex(uint32_t value, char* buffer, size_t digits) {
    for (size_t i = digits; i > 0; i--) {
        buffer[i - 1] = nibbleToHexChar(value & 0x0F);
        value >>= 4;
    }
    return digits;
}

void SLCAN::setError(char* response) {
    response[0] = SLCAN_ERROR;
    response[1] = '\0';
}

void SLCAN::setOk(char* response) {
    response[0] = '\0';  // Empty response followed by CR
}

// =============================================================================
// LED Control
// =============================================================================

#if ENABLE_STATUS_LED

void SLCAN::blinkTxLed() {
    _lastTxLedTime = millis();
    digitalWrite(LED_PIN, HIGH);
    _ledState = true;
}

void SLCAN::blinkRxLed() {
    _lastRxLedTime = millis();
    digitalWrite(LED_PIN, HIGH);
    _ledState = true;
}

void SLCAN::updateLed() {
    if (!_ledState) {
        return;
    }

    unsigned long now = millis();
    bool txActive = (now - _lastTxLedTime) < LED_TX_BLINK_MS;
    bool rxActive = (now - _lastRxLedTime) < LED_RX_BLINK_MS;

    if (!txActive && !rxActive) {
        digitalWrite(LED_PIN, LOW);
        _ledState = false;
    }
}

#endif // ENABLE_STATUS_LED

// =============================================================================
// RX Ring Buffer Management
// =============================================================================

void SLCAN::drainFromBackend() {
    // Pull frames from hardware CAN backend into our protocol-layer ring
    while (_can.available()) {
        uint16_t nextHead = (_rxHead + 1) % CAN_RX_QUEUE_SIZE;

        if (nextHead == _rxTail) {
            // Ring full: drop NEWEST frame (per user preference - keep history)
            _rxOverflowCount++;
            return;  // Stop draining, preserve oldest frames
        }

        CANFrame msg;
        if (_can.read(msg)) {
            _rxQueue[_rxHead].msg = msg;
            _rxQueue[_rxHead].timestamp = millis() & 0xFFFF;
            _rxQueue[_rxHead].valid = true;
            _rxHead = nextHead;
        } else {
            break;  // No more frames available
        }
    }
}

bool SLCAN::getNextRxFrame(CANFrame& frame, uint16_t* timestamp) {
    // Internal helper: get next frame from ring
    if (_rxTail == _rxHead) {
        return false;  // Ring empty
    }

    frame = _rxQueue[_rxTail].msg;
    if (timestamp) *timestamp = _rxQueue[_rxTail].timestamp;
    _rxQueue[_rxTail].valid = false;
    _rxTail = (_rxTail + 1) % CAN_RX_QUEUE_SIZE;

    return true;
}

// =============================================================================
// Diagnostic Counters
// =============================================================================

void SLCAN::getCounters(uint32_t* rxOverflows, uint32_t* canRxDrops) const {
    if (rxOverflows) *rxOverflows = _rxOverflowCount;
    if (canRxDrops) *canRxDrops = _canRxDropCount;
}

void SLCAN::resetCounters() {
    _rxOverflowCount = 0;
    _canRxDropCount = 0;
}
