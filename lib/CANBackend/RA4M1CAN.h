/**
 * RA4M1 CAN Backend Implementation
 *
 * CAN backend implementation for Arduino Uno R4 WiFi using the
 * built-in Arduino_CAN library and RA4M1 CAN controller.
 */

#ifndef RA4M1_CAN_H
#define RA4M1_CAN_H

#include "CANBackend.h"
#include "config.h"
#include <Arduino_CAN.h>

/**
 * RA4M1 CAN controller backend.
 *
 * Wraps the Arduino_CAN library to provide the ICANBackend interface.
 * Supports bitrates: 125k, 250k, 500k, 1000k (S4, S5, S6, S8).
 */
class RA4M1CAN : public ICANBackend {
public:
    RA4M1CAN();

    bool isBitrateSupported(CANBitrate bitrate) const override;
    bool begin(CANBitrate bitrate, CANMode mode = CANMode::Normal) override;
    void end() override;
    bool isOpen() const override;
    CANMode getMode() const override;
    bool write(const CANFrame& frame) override;
    bool available() override;
    bool read(CANFrame& frame) override;
    CANStatus getStatus() override;
    bool setFilter(uint32_t mask, uint32_t filter) override;
    bool clearFilter() override;

    /**
     * Service the TX queue - call from poll() to drain queued frames.
     * Attempts to send queued frames if hardware has space available.
     */
    void serviceTxQueue() override;

    /**
     * Get diagnostic counters.
     * @param txQueueFull Output: TX queue full count (frames rejected)
     */
    void getCounters(uint32_t* txQueueFull) const;

    /**
     * Reset diagnostic counters.
     */
    void resetCounters();

private:
    bool _isOpen;
    CANMode _mode;
    CANBitrate _bitrate;

    // Filter state
    uint32_t _filterMask;
    uint32_t _filterValue;
    bool _filterEnabled;

    // TX queue (ring buffer)
    CANFrame _txQueue[CAN_TX_QUEUE_SIZE];
    uint8_t _txQueueHead;   // write() enqueues here
    uint8_t _txQueueTail;   // serviceTxQueue() dequeues here
    uint8_t _txQueueCount;  // Number of frames in queue

    // Diagnostic counters
    uint32_t _txQueueFullCount;  // Frames rejected due to queue full

    /**
     * Convert CANBitrate enum to Arduino_CAN CanBitRate.
     * @param bitrate Our bitrate enum
     * @return Arduino_CAN bitrate enum, or invalid value if unsupported
     */
    CanBitRate toArduinoBitrate(CANBitrate bitrate) const;

    /**
     * Apply software filter (for ListenOnly mode or extended filtering).
     * @param id The CAN ID to check
     * @return true if frame passes filter, false if rejected
     */
    bool passesFilter(uint32_t id) const;

    /**
     * Clear queued TX frames to avoid sending stale traffic on reopen.
     */
    void clearTxQueue();
};

#endif // RA4M1_CAN_H
