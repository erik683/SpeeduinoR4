/**
 * Serial Transport Implementation
 */

#include "SerialTransport.h"
#include <string.h>

SerialTransport::SerialTransport(Stream& serial)
    : _serial(serial)
    , _rxIndex(0)
    , _lineReady(false)
{
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
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
    return _serial.available() > 0 || _lineReady;
}

bool SerialTransport::readLine(char* buffer, size_t maxLen) {
    // Process any pending incoming data
    processIncoming();

    // If we have a complete line, copy it to the output buffer
    if (_lineReady) {
        size_t copyLen = _rxIndex;
        if (copyLen >= maxLen) {
            copyLen = maxLen - 1;
        }

        memcpy(buffer, _rxBuffer, copyLen);
        buffer[copyLen] = '\0';

        // Reset for next line
        resetBuffer();
        return true;
    }

    return false;
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

void SerialTransport::flush() {
    _serial.flush();
}

void SerialTransport::resetBuffer() {
    _rxIndex = 0;
    _lineReady = false;
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
}

void SerialTransport::processIncoming() {
    // Don't process if we already have a complete line waiting
    if (_lineReady) {
        return;
    }

    while (_serial.available() > 0) {
        char c = _serial.read();

        // CR marks end of line (SLCAN standard)
        if (c == '\r') {
            _lineReady = true;
            return;
        }

        // LF is ignored (some terminals send CR+LF)
        if (c == '\n') {
            continue;
        }

        // Store character if buffer has room
        if (_rxIndex < sizeof(_rxBuffer) - 1) {
            _rxBuffer[_rxIndex++] = c;
        }
        // If buffer overflows, we'll just truncate
        // The command parser will handle invalid commands
    }
}
