/**
 * Serial Transport Implementation
 */

#include "SerialTransport.h"
#include <string.h>

SerialTransport::SerialTransport(Stream& serial)
    : _serial(serial)
    , _cmdHead(0)
    , _cmdTail(0)
    , _rxAccIndex(0)
    , _cmdResponseDropCount(0)
    , _canTxDropCount(0)
    , _cmdOverflowCount(0)
{
    memset(_rxAccumulator, 0, sizeof(_rxAccumulator));
    for (uint8_t i = 0; i < SERIAL_CMD_QUEUE_SIZE; i++) {
        memset(_cmdQueue[i].buffer, 0, sizeof(_cmdQueue[i].buffer));
        _cmdQueue[i].length = 0;
        _cmdQueue[i].valid = false;
    }
}

void SerialTransport::begin(uint32_t baudRate) {
    // For USB CDC on R4 WiFi, Serial is already available
    // but we call begin() for compatibility and to set baud rate
    // Note: USB CDC ignores baud rate, but hardware UART would use it
    if (&_serial == &Serial) {
        Serial.begin(baudRate);
        // Wait for USB CDC to be ready (with timeout)
        unsigned long start = millis();
        while (!Serial && (millis() - start) < 3000) {
            // Wait up to 3 seconds for USB connection
        }
    }
    resetBuffer();
}

bool SerialTransport::available() {
    return _serial.available() > 0 || (_cmdTail != _cmdHead);
}

bool SerialTransport::readLine(char* buffer, size_t maxLen) {
    // First, try to drain new input
    processIncoming();

    // Check if queue has a command ready
    if (_cmdTail == _cmdHead) {
        return false;  // Queue empty
    }

    // Dequeue oldest command
    size_t copyLen = _cmdQueue[_cmdTail].length;
    if (copyLen >= maxLen) {
        copyLen = maxLen - 1;
    }

    memcpy(buffer, _cmdQueue[_cmdTail].buffer, copyLen);
    buffer[copyLen] = '\0';

    _cmdQueue[_cmdTail].valid = false;
    _cmdTail = (_cmdTail + 1) % SERIAL_CMD_QUEUE_SIZE;

    return true;
}

void SerialTransport::writeLine(const char* response) {
    if (response && *response) {
        _serial.print(response);
    }
    _serial.print('\r');
}

void SerialTransport::writeChar(char c) {
    _serial.print(c);
}

void SerialTransport::writeRaw(const char* data, size_t len) {
    _serial.write(data, len);
}

bool SerialTransport::writeWithPriority(const char* data, size_t len, WritePriority prio) {
    // Check if USB CDC can accept the full write atomically.
    // Note: Some cores return 0 for availableForWrite() to indicate "unknown".
    int available = _serial.availableForWrite();
    if (available > 0 && available < (int)len) {
        if (prio == WritePriority::COMMAND_RESPONSE) {
            // Critical: Block briefly with timeout (10ms max to stay within loop budget)
            uint32_t start = millis();
            while (_serial.availableForWrite() < (int)len) {
                if (millis() - start > 10) {
                    // Timeout expired: drop response to prevent hang
                    _cmdResponseDropCount++;
                    return false;
                }
            }
            // Space available after wait
        } else {
            // CAN RX frame: Drop immediately if no space (0ms timeout)
            _canTxDropCount++;
            return false;
        }
    }

    // All-or-nothing write (space is available)
    _serial.write(data, len);
    return true;
}

void SerialTransport::flush() {
    _serial.flush();
}

void SerialTransport::getCounters(uint32_t* cmdResponseDrops, uint32_t* canTxDrops, uint32_t* cmdOverflows) const {
    if (cmdResponseDrops) *cmdResponseDrops = _cmdResponseDropCount;
    if (canTxDrops) *canTxDrops = _canTxDropCount;
    if (cmdOverflows) *cmdOverflows = _cmdOverflowCount;
}

void SerialTransport::resetCounters() {
    _cmdResponseDropCount = 0;
    _canTxDropCount = 0;
    _cmdOverflowCount = 0;
}

void SerialTransport::resetBuffer() {
    _cmdHead = 0;
    _cmdTail = 0;
    _rxAccIndex = 0;
    memset(_rxAccumulator, 0, sizeof(_rxAccumulator));
}

void SerialTransport::processIncoming() {
    // Drain all available serial data
    while (_serial.available() > 0) {
        char c = _serial.read();

        // CR or LF marks end of line (SLCAN standard)
        if (c == '\r' || c == '\n') {
            if (_rxAccIndex > 0) {
                // Line complete: enqueue it
                uint8_t nextHead = (_cmdHead + 1) % SERIAL_CMD_QUEUE_SIZE;

                if (nextHead == _cmdTail) {
                    // Queue full: drop new command, count overflow
                    _cmdOverflowCount++;
                    _rxAccIndex = 0;  // Discard current line
                    continue;
                }

                // Copy to queue slot
                memcpy(_cmdQueue[_cmdHead].buffer, _rxAccumulator, _rxAccIndex);
                _cmdQueue[_cmdHead].buffer[_rxAccIndex] = '\0';
                _cmdQueue[_cmdHead].length = _rxAccIndex;
                _cmdQueue[_cmdHead].valid = true;
                _cmdHead = nextHead;

                _rxAccIndex = 0;  // Reset accumulator, keep draining
            }
        } else {
            // Accumulate character
            if (_rxAccIndex < SERIAL_RX_BUFFER_SIZE - 1) {
                _rxAccumulator[_rxAccIndex++] = c;
            } else {
                // Line too long: count overflow and truncate
                _cmdOverflowCount++;
            }
        }
    }
}
