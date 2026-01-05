/**
 * Protocol Dispatcher Implementation
 */

#include "ProtocolDispatcher.h"
#include <string.h>

ProtocolDispatcher::ProtocolDispatcher()
    : _handlerCount(0)
{
    for (size_t i = 0; i < MAX_PROTOCOL_HANDLERS; i++) {
        _handlers[i] = nullptr;
    }
}

bool ProtocolDispatcher::registerHandler(IProtocolHandler* handler) {
    if (handler == nullptr) {
        return false;
    }

    // Check if already registered
    for (size_t i = 0; i < _handlerCount; i++) {
        if (_handlers[i] == handler) {
            return true;  // Already registered
        }
    }

    // Check if there's room
    if (_handlerCount >= MAX_PROTOCOL_HANDLERS) {
        return false;
    }

    _handlers[_handlerCount++] = handler;
    return true;
}

bool ProtocolDispatcher::unregisterHandler(IProtocolHandler* handler) {
    if (handler == nullptr) {
        return false;
    }

    for (size_t i = 0; i < _handlerCount; i++) {
        if (_handlers[i] == handler) {
            // Shift remaining handlers down
            for (size_t j = i; j < _handlerCount - 1; j++) {
                _handlers[j] = _handlers[j + 1];
            }
            _handlers[--_handlerCount] = nullptr;
            return true;
        }
    }

    return false;
}

bool ProtocolDispatcher::dispatch(const char* cmd, char* response, size_t maxLen) {
    if (cmd == nullptr || response == nullptr || maxLen == 0) {
        return false;
    }

    // Empty command - no response
    if (cmd[0] == '\0') {
        return false;
    }

    // Try each handler in order
    for (size_t i = 0; i < _handlerCount; i++) {
        if (_handlers[i] != nullptr && _handlers[i]->canHandle(cmd)) {
            return _handlers[i]->processCommand(cmd, response, maxLen);
        }
    }

    // No handler found - return error
    // For SLCAN compatibility, return BELL character
    if (maxLen >= 2) {
        response[0] = '\x07';  // BELL - error
        response[1] = '\0';
        return true;
    }

    return false;
}

void ProtocolDispatcher::pollAll(ITransport* transport) {
    for (size_t i = 0; i < _handlerCount; i++) {
        if (_handlers[i] != nullptr) {
            _handlers[i]->poll(transport);
        }
    }
}

size_t ProtocolDispatcher::getHandlerCount() const {
    return _handlerCount;
}

IProtocolHandler* ProtocolDispatcher::getHandler(size_t index) const {
    if (index >= _handlerCount) {
        return nullptr;
    }
    return _handlers[index];
}
