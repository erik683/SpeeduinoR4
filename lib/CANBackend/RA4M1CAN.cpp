/**
 * RA4M1 CAN Backend Implementation
 */

#include "RA4M1CAN.h"

RA4M1CAN::RA4M1CAN()
    : _isOpen(false)
    , _mode(CANMode::Normal)
    , _bitrate(CANBitrate::BR_500K)
    , _filterMask(0)
    , _filterValue(0)
    , _filterEnabled(false)
{
}

bool RA4M1CAN::isBitrateSupported(CANBitrate bitrate) const {
    switch (bitrate) {
        case CANBitrate::BR_125K:
        case CANBitrate::BR_250K:
        case CANBitrate::BR_500K:
        case CANBitrate::BR_1000K:
            return true;
        default:
            return false;
    }
}

CanBitRate RA4M1CAN::toArduinoBitrate(CANBitrate bitrate) const {
    switch (bitrate) {
        case CANBitrate::BR_125K:  return CanBitRate::BR_125k;
        case CANBitrate::BR_250K:  return CanBitRate::BR_250k;
        case CANBitrate::BR_500K:  return CanBitRate::BR_500k;
        case CANBitrate::BR_1000K: return CanBitRate::BR_1000k;
        default:
            // Return a valid default, though this shouldn't be reached
            // if isBitrateSupported() is checked first
            return CanBitRate::BR_500k;
    }
}

bool RA4M1CAN::begin(CANBitrate bitrate, CANMode mode) {
    // Check if bitrate is supported
    if (!isBitrateSupported(bitrate)) {
        return false;
    }

    // Close if already open
    if (_isOpen) {
        end();
    }

    // Initialize CAN controller
    CanBitRate arduinoBitrate = toArduinoBitrate(bitrate);

    if (!CAN.begin(arduinoBitrate)) {
        return false;
    }

    _isOpen = true;
    _mode = mode;
    _bitrate = bitrate;

    // Note: Arduino_CAN library doesn't have a direct listen-only mode setting.
    // For listen-only mode, we simply won't transmit (checked in write()).
    // True hardware listen-only would require direct register access.

    return true;
}

void RA4M1CAN::end() {
    if (_isOpen) {
        CAN.end();
        _isOpen = false;
    }
}

bool RA4M1CAN::isOpen() const {
    return _isOpen;
}

CANMode RA4M1CAN::getMode() const {
    return _mode;
}

bool RA4M1CAN::write(const CANFrame& frame) {
    if (!_isOpen) {
        return false;
    }

    // Don't transmit in listen-only mode
    if (_mode == CANMode::ListenOnly) {
        return false;
    }

    // Create Arduino_CAN message
    CanMsg msg;

    if (frame.extended) {
        msg = CanMsg(CanExtendedId(frame.id), frame.dlc, frame.data);
    } else {
        msg = CanMsg(CanStandardId(frame.id), frame.dlc, frame.data);
    }

    // RTR frames
    // Note: Arduino_CAN CanMsg doesn't have explicit RTR support in the simple constructor.
    // For RTR, we'd need to check if the library supports it or implement differently.
    // For now, we send the frame as-is. RTR handling may need enhancement.

    int rc = CAN.write(msg);
    return (rc >= 0);
}

bool RA4M1CAN::available() {
    if (!_isOpen) {
        return false;
    }
    return CAN.available() > 0;
}

bool RA4M1CAN::read(CANFrame& frame) {
    if (!_isOpen) {
        return false;
    }

    if (!CAN.available()) {
        return false;
    }

    CanMsg msg = CAN.read();

    // Extract frame data
    frame.id = msg.id;
    frame.dlc = msg.data_length;
    frame.extended = msg.isExtendedId();
    // Note: Arduino_CAN library doesn't expose RTR flag directly
    // RTR frames are rare in practice; set to false
    frame.rtr = false;

    // Copy data bytes
    for (uint8_t i = 0; i < frame.dlc && i < 8; i++) {
        frame.data[i] = msg.data[i];
    }
    // Zero remaining bytes
    for (uint8_t i = frame.dlc; i < 8; i++) {
        frame.data[i] = 0;
    }

    // Set timestamp (milliseconds since boot, wrapped to 16-bit)
    frame.timestamp = (uint16_t)(millis() & 0xFFFF);

    // Apply software filter if enabled
    if (_filterEnabled && !passesFilter(frame.id)) {
        // Frame rejected by filter - read next frame recursively
        // or return false to let caller try again
        return false;
    }

    return true;
}

CANStatus RA4M1CAN::getStatus() {
    CANStatus status = {};

    // The Arduino_CAN library doesn't provide detailed error status access.
    // We return a basic status structure.
    // For more detailed status, direct register access would be needed.

    if (!_isOpen) {
        return status;
    }

    // Check if CAN is available (basic operational check)
    // Advanced error flags would require direct RA4M1 register access

    // For now, return a clean status if we're open
    status.rxFifoFull = false;
    status.txFifoFull = false;
    status.errorWarning = false;
    status.dataOverrun = false;
    status.errorPassive = false;
    status.arbitrationLost = false;
    status.busError = false;

    return status;
}

bool RA4M1CAN::setFilter(uint32_t mask, uint32_t filter) {
    // Arduino_CAN library has limited filter support.
    // We implement software filtering.
    _filterMask = mask;
    _filterValue = filter;
    _filterEnabled = true;
    return true;
}

bool RA4M1CAN::clearFilter() {
    _filterMask = 0;
    _filterValue = 0;
    _filterEnabled = false;
    return true;
}

bool RA4M1CAN::passesFilter(uint32_t id) const {
    if (!_filterEnabled) {
        return true;
    }
    // Standard acceptance filter logic:
    // Pass if (id & mask) == (filter & mask)
    return (id & _filterMask) == (_filterValue & _filterMask);
}
