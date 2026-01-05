/**
 * CAN Backend Interface
 *
 * Abstract interface for CAN controller implementations.
 * Provides a hardware-independent API for CAN operations.
 */

#ifndef CAN_BACKEND_H
#define CAN_BACKEND_H

#include <stdint.h>
#include <stddef.h>

/**
 * CAN bitrate presets.
 * Maps to SLCAN S0-S8 commands.
 */
enum class CANBitrate : uint8_t {
    BR_10K   = 0,   // S0 - Not supported on RA4M1
    BR_20K   = 1,   // S1 - Not supported on RA4M1
    BR_50K   = 2,   // S2 - Not supported on RA4M1
    BR_100K  = 3,   // S3 - Not supported on RA4M1
    BR_125K  = 4,   // S4 - Supported
    BR_250K  = 5,   // S5 - Supported
    BR_500K  = 6,   // S6 - Supported
    BR_800K  = 7,   // S7 - Not supported on RA4M1
    BR_1000K = 8    // S8 - Supported
};

/**
 * CAN operation mode.
 */
enum class CANMode : uint8_t {
    Normal,         // Normal transmit/receive operation
    ListenOnly      // Listen-only mode (no ACK, no transmit)
};

/**
 * CAN bus status flags.
 * Used for SLCAN 'F' (read status flags) command.
 */
struct CANStatus {
    bool rxFifoFull;        // Bit 0: RX FIFO full
    bool txFifoFull;        // Bit 1: TX FIFO full
    bool errorWarning;      // Bit 2: Error warning (TEC/REC > 96)
    bool dataOverrun;       // Bit 3: Data overrun
    bool unused4;           // Bit 4: Reserved
    bool errorPassive;      // Bit 5: Error passive (TEC/REC > 127)
    bool arbitrationLost;   // Bit 6: Arbitration lost
    bool busError;          // Bit 7: Bus error
};

/**
 * CAN frame structure.
 * Represents a single CAN message.
 */
struct CANFrame {
    uint32_t id;            // CAN identifier (11-bit or 29-bit)
    uint8_t dlc;            // Data length code (0-8)
    uint8_t data[8];        // Message data
    bool extended;          // true = 29-bit extended ID, false = 11-bit standard
    bool rtr;               // true = Remote Transmission Request frame
    uint16_t timestamp;     // Timestamp in milliseconds (optional)

    CANFrame() : id(0), dlc(0), extended(false), rtr(false), timestamp(0) {
        for (int i = 0; i < 8; i++) data[i] = 0;
    }
};

/**
 * Abstract CAN backend interface.
 *
 * Implementations wrap hardware-specific CAN controllers
 * and provide a consistent API for protocol handlers.
 */
class ICANBackend {
public:
    /**
     * Check if a bitrate is supported by this backend.
     * @param bitrate The bitrate to check
     * @return true if supported, false otherwise
     */
    virtual bool isBitrateSupported(CANBitrate bitrate) const = 0;

    /**
     * Initialize and open the CAN controller.
     * @param bitrate CAN bus bitrate
     * @param mode Operating mode (Normal or ListenOnly)
     * @return true on success, false on failure
     */
    virtual bool begin(CANBitrate bitrate, CANMode mode = CANMode::Normal) = 0;

    /**
     * Close and de-initialize the CAN controller.
     */
    virtual void end() = 0;

    /**
     * Check if the CAN controller is currently open.
     * @return true if open, false if closed
     */
    virtual bool isOpen() const = 0;

    /**
     * Get the current operating mode.
     * @return Current CANMode
     */
    virtual CANMode getMode() const = 0;

    /**
     * Transmit a CAN frame.
     * @param frame The frame to transmit
     * @return true on success, false on failure
     */
    virtual bool write(const CANFrame& frame) = 0;

    /**
     * Check if a received frame is available.
     * @return true if a frame is waiting in the receive buffer
     */
    virtual bool available() = 0;

    /**
     * Read a received CAN frame.
     * @param frame Output parameter for the received frame
     * @return true if a frame was read, false if no frame available
     */
    virtual bool read(CANFrame& frame) = 0;

    /**
     * Get the current CAN bus status.
     * @return CANStatus structure with current flags
     */
    virtual CANStatus getStatus() = 0;

    /**
     * Set hardware acceptance filter.
     * Filters are applied as: (received_id & mask) == (filter & mask)
     *
     * @param mask Acceptance mask (1 = care, 0 = don't care)
     * @param filter Acceptance filter value
     * @return true on success, false on failure
     */
    virtual bool setFilter(uint32_t mask, uint32_t filter) = 0;

    /**
     * Clear/disable the acceptance filter (accept all frames).
     * @return true on success, false on failure
     */
    virtual bool clearFilter() = 0;

    virtual ~ICANBackend() = default;
};

#endif // CAN_BACKEND_H
