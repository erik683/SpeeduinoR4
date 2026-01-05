/**
 * Transport Interface
 *
 * Abstract interface for transport layer implementations.
 * Provides line-based I/O for protocol communication.
 */

#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

/**
 * Abstract transport interface for line-based communication.
 *
 * Implementations handle the underlying transport (Serial, WiFi, etc.)
 * and provide a consistent line-based API for protocol handlers.
 */
class ITransport {
public:
    /**
     * Initialize the transport.
     * @param baudRate Baud rate for serial transports (ignored by others)
     */
    virtual void begin(uint32_t baudRate) = 0;

    /**
     * Check if data is available to read.
     * @return true if there is pending data
     */
    virtual bool available() = 0;

    /**
     * Read a complete line from the transport.
     * Non-blocking: returns false if no complete line is ready.
     *
     * @param buffer Output buffer for the line (null-terminated, without terminator)
     * @param maxLen Maximum buffer size including null terminator
     * @return true if a complete line was read, false otherwise
     */
    virtual bool readLine(char* buffer, size_t maxLen) = 0;

    /**
     * Write a null-terminated response string followed by CR.
     * @param response Response string to write
     */
    virtual void writeLine(const char* response) = 0;

    /**
     * Write a single character.
     * @param c Character to write
     */
    virtual void writeChar(char c) = 0;

    /**
     * Write raw data without adding terminators.
     * @param data Data to write
     * @param len Number of bytes to write
     */
    virtual void writeRaw(const char* data, size_t len) = 0;

    /**
     * Flush any pending output.
     */
    virtual void flush() = 0;

    virtual ~ITransport() = default;
};

#endif // TRANSPORT_H
