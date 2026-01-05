/**
 * Protocol Handler Interface
 *
 * Abstract interface for protocol handlers (SLCAN, ISO-TP, GVRET, etc.).
 * Allows multiple protocols to be registered and dispatched.
 */

#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration
class ITransport;

/**
 * Abstract protocol handler interface.
 *
 * Protocol handlers parse incoming commands and generate responses.
 * They can also perform periodic polling for async operations like
 * forwarding received CAN frames.
 */
class IProtocolHandler {
public:
    /**
     * Get the protocol name for identification.
     * @return Protocol name string (e.g., "SLCAN", "GVRET")
     */
    virtual const char* getName() const = 0;

    /**
     * Check if this handler can process a given command.
     * Used by the dispatcher to route commands.
     *
     * @param cmd The command string to check
     * @return true if this handler can process the command
     */
    virtual bool canHandle(const char* cmd) const = 0;

    /**
     * Process a command and generate a response.
     *
     * @param cmd The command string (without terminator)
     * @param response Output buffer for the response (without terminator)
     * @param maxLen Maximum response buffer size
     * @return true if a response was generated, false if no response needed
     */
    virtual bool processCommand(const char* cmd, char* response, size_t maxLen) = 0;

    /**
     * Periodic poll function.
     * Called regularly from the main loop for async operations
     * (e.g., forwarding received CAN frames).
     *
     * @param transport Transport to use for output (if needed)
     */
    virtual void poll(ITransport* transport) = 0;

    /**
     * Check if the handler is currently active/enabled.
     * @return true if active
     */
    virtual bool isActive() const = 0;

    virtual ~IProtocolHandler() = default;
};

#endif // PROTOCOL_HANDLER_H
