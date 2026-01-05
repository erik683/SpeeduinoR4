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
#include "config.h"

/**
 * Default values if not defined in config.h
 */
#ifndef SERIAL_RX_BUFFER_SIZE
#define SERIAL_RX_BUFFER_SIZE 256
#endif

#ifndef SERIAL_CMD_QUEUE_SIZE
#define SERIAL_CMD_QUEUE_SIZE 4
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
    bool writeWithPriority(const char* data, size_t len, WritePriority prio) override;
    void flush() override;

    /**
     * Reset the internal line buffer.
     * Call this to discard any partially received data.
     */
    void resetBuffer();

    /**
     * Get diagnostic counters.
     * @param cmdResponseDrops Output: command responses dropped due to timeout
     * @param canTxDrops Output: CAN RX frames dropped due to USB unavailable
     * @param cmdOverflows Output: command queue overflows
     */
    void getCounters(uint32_t* cmdResponseDrops, uint32_t* canTxDrops, uint32_t* cmdOverflows) const;

    /**
     * Reset all diagnostic counters to zero.
     */
    void resetCounters();

private:
    Stream& _serial;

    // Multi-command queue
    struct CommandSlot {
        char buffer[SERIAL_RX_BUFFER_SIZE];
        uint16_t length;
        bool valid;
    };

    CommandSlot _cmdQueue[SERIAL_CMD_QUEUE_SIZE];
    uint8_t _cmdHead;  // processIncoming() writes here
    uint8_t _cmdTail;  // readLine() reads here

    // Current accumulator (for incomplete line)
    char _rxAccumulator[SERIAL_RX_BUFFER_SIZE];
    uint16_t _rxAccIndex;

    // Diagnostic counters
    uint32_t _cmdResponseDropCount;  // Command responses dropped (timeout)
    uint32_t _canTxDropCount;        // CAN RX frames dropped (no space)
    uint32_t _cmdOverflowCount;      // Command queue overflow

    /**
     * Process incoming serial data into the line buffer.
     * Called internally by readLine().
     */
    void processIncoming();
};

#endif // SERIAL_TRANSPORT_H
