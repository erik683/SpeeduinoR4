/**
 * Protocol Dispatcher
 *
 * Routes commands to appropriate protocol handlers.
 * Allows multiple protocols to be registered for future expansion.
 */

#ifndef PROTOCOL_DISPATCHER_H
#define PROTOCOL_DISPATCHER_H

#include "ProtocolHandler.h"
#include "Transport.h"

#ifndef MAX_PROTOCOL_HANDLERS
#define MAX_PROTOCOL_HANDLERS 4
#endif

/**
 * Protocol dispatcher for routing commands to handlers.
 *
 * Features:
 * - Register multiple protocol handlers
 * - Route commands to the appropriate handler
 * - Poll all handlers for async operations
 */
class ProtocolDispatcher {
public:
    ProtocolDispatcher();

    /**
     * Register a protocol handler.
     * @param handler Pointer to the handler (must remain valid)
     * @return true if registered successfully, false if no room
     */
    bool registerHandler(IProtocolHandler* handler);

    /**
     * Unregister a protocol handler.
     * @param handler Pointer to the handler to remove
     * @return true if found and removed, false if not found
     */
    bool unregisterHandler(IProtocolHandler* handler);

    /**
     * Dispatch a command to the appropriate handler.
     *
     * @param cmd The command string
     * @param response Output buffer for the response
     * @param maxLen Maximum response buffer size
     * @return true if a response was generated, false otherwise
     */
    bool dispatch(const char* cmd, char* response, size_t maxLen);

    /**
     * Poll all registered handlers.
     * Call this regularly from the main loop.
     *
     * @param transport Transport to pass to handlers for output
     */
    void pollAll(ITransport* transport);

    /**
     * Get the number of registered handlers.
     * @return Number of handlers
     */
    size_t getHandlerCount() const;

    /**
     * Get a registered handler by index.
     * @param index Handler index (0 to getHandlerCount()-1)
     * @return Handler pointer or nullptr if invalid index
     */
    IProtocolHandler* getHandler(size_t index) const;

private:
    IProtocolHandler* _handlers[MAX_PROTOCOL_HANDLERS];
    size_t _handlerCount;
};

#endif // PROTOCOL_DISPATCHER_H
