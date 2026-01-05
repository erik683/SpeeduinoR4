/**
 * Serial Transport Implementation
 *
 * USB CDC serial transport for Arduino Uno R4 WiFi.
 * Provides line-based I/O with CR (\r) as the line terminator.
 */

#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

#include "Transport.h"
#include <Arduino.h>

/**
 * Default receive buffer size for line accumulation.
 */
#ifndef SERIAL_RX_BUFFER_SIZE
#define SERIAL_RX_BUFFER_SIZE 64
#endif

/**
 * Serial transport implementation using Arduino Serial (USB CDC).
 *
 * Features:
 * - Line-based buffering with CR terminator
 * - Non-blocking readLine() operation
 * - Automatic CR appending on writeLine()
 */
class SerialTransport : public ITransport {
public:
    /**
     * Constructor.
     * @param serial Reference to the HardwareSerial instance (default: Serial)
     */
    explicit SerialTransport(Stream& serial = Serial);

    void begin(uint32_t baudRate) override;
    bool available() override;
    bool readLine(char* buffer, size_t maxLen) override;
    void writeLine(const char* response) override;
    void writeChar(char c) override;
    void writeRaw(const char* data, size_t len) override;
    void flush() override;

    /**
     * Reset the internal line buffer.
     * Call this to discard any partially received data.
     */
    void resetBuffer();

private:
    Stream& _serial;
    char _rxBuffer[SERIAL_RX_BUFFER_SIZE];
    size_t _rxIndex;
    bool _lineReady;

    /**
     * Process incoming serial data into the line buffer.
     * Called internally by readLine().
     */
    void processIncoming();
};

#endif // SERIAL_TRANSPORT_H
