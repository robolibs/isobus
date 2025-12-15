/**
 * @file nmea_sender.cpp
 * @brief Example demonstrating sending proprietary NMEA messages (PHTG) over ISOBUS
 *
 * This example reads NMEA sentences from serial and sends the proprietary PHTG
 * (authentication) message over CAN using AgIsoStack++ with proper ECU address claiming.
 *
 * The standard NMEA sentences (GGA, RMC, etc.) are converted to J1939 PGNs by the
 * Python bridge (send_can_j1939.py), while this C++ application handles the
 * proprietary PHTG message using ISOBUS protocols.
 *
 * Usage: ./nmea_sender [can_interface] [serial_port] [baud_rate]
 *        Default: can0, /dev/ttyUSB0, 115200
 */

#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/socket_can_interface.hpp"
#include "isobus/isobus/can_network_manager.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

// ============================================================================
// Configuration
// ============================================================================

// Proprietary PGN for NMEA PHTG messages
// Using Proprietary A (0xEF00) which allows 8-byte payloads
// For longer messages, Transport Protocol is used automatically
constexpr std::uint32_t PGN_PROPRIETARY_NMEA = 0xEF00;

// Proprietary B can be used for manufacturer-specific messages (up to 1785 bytes with TP)
constexpr std::uint32_t PGN_PROPRIETARY_B_NMEA = 0xFF00;

static std::atomic_bool running{true};

void signal_handler(int) { running = false; }

// ============================================================================
// Serial Port Helper
// ============================================================================

class SerialPort {
  public:
    SerialPort() : fd_(-1) {}
    ~SerialPort() { close_port(); }

    bool open_port(const std::string &port, int baud_rate) {
        fd_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) {
            std::cerr << "Failed to open serial port: " << port << std::endl;
            return false;
        }

        struct termios tty;
        if (tcgetattr(fd_, &tty) != 0) {
            std::cerr << "Failed to get terminal attributes" << std::endl;
            close_port();
            return false;
        }

        // Set baud rate
        speed_t speed = B115200;
        switch (baud_rate) {
        case 9600:
            speed = B9600;
            break;
        case 19200:
            speed = B19200;
            break;
        case 38400:
            speed = B38400;
            break;
        case 57600:
            speed = B57600;
            break;
        case 115200:
            speed = B115200;
            break;
        default:
            std::cerr << "Unsupported baud rate, using 115200" << std::endl;
            break;
        }
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);

        // 8N1, no flow control
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag |= CREAD | CLOCAL;

        tty.c_lflag &= ~ICANON;
        tty.c_lflag &= ~ECHO;
        tty.c_lflag &= ~ECHOE;
        tty.c_lflag &= ~ECHONL;
        tty.c_lflag &= ~ISIG;

        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

        tty.c_oflag &= ~OPOST;
        tty.c_oflag &= ~ONLCR;

        tty.c_cc[VTIME] = 1;
        tty.c_cc[VMIN] = 0;

        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            std::cerr << "Failed to set terminal attributes" << std::endl;
            close_port();
            return false;
        }

        return true;
    }

    void close_port() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    bool read_line(std::string &line) {
        line.clear();
        char c;
        while (running) {
            ssize_t n = read(fd_, &c, 1);
            if (n > 0) {
                if (c == '\n' || c == '\r') {
                    if (!line.empty()) {
                        return true;
                    }
                } else {
                    line += c;
                }
            } else if (n == 0 || (n < 0 && errno == EAGAIN)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                return false;
            }
        }
        return false;
    }

    bool is_open() const { return fd_ >= 0; }

  private:
    int fd_;
};

// ============================================================================
// PHTG Message Parser
// ============================================================================

struct PhtgMessage {
    std::string time_tag;
    std::string system;  // GPS or GAL
    std::string service; // HAS, OSNMA, HO
    int auth_result;     // 0..4
    int status;          // 0..3
    int warning;         // 0..3
    bool valid;

    PhtgMessage() : auth_result(0), status(0), warning(0), valid(false) {}
};

/**
 * @brief Parse a PHTG NMEA sentence
 * @param nmea_line The full NMEA sentence (e.g., "$PHTG,16:12:2025,10:30:45.00,GAL,HAS,0,0*XX")
 * @return Parsed PHTG message
 */
PhtgMessage parse_phtg(const std::string &nmea_line) {
    PhtgMessage msg;

    // Check if this is a PHTG sentence
    if (nmea_line.find("PHTG") == std::string::npos) {
        return msg;
    }

    // Remove checksum if present
    std::string line = nmea_line;
    size_t checksum_pos = line.find('*');
    if (checksum_pos != std::string::npos) {
        line = line.substr(0, checksum_pos);
    }

    // Remove leading '$' if present
    if (!line.empty() && line[0] == '$') {
        line = line.substr(1);
    }

    // Parse comma-separated fields
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        fields.push_back(field);
    }

    // PHTG format: PHTG,date,time,system,service,status,warning
    // Or: PHTG,timetag,system,service,status,warning (combined timetag)
    if (fields.size() >= 6) {
        // Combined timetag format: PHTG,dd:mm:yyyy,hh:mm:ss.ss,system,service,status,warning
        if (fields.size() >= 7) {
            msg.time_tag = fields[1] + "," + fields[2];
            msg.system = fields[3];
            msg.service = fields[4];
            try {
                msg.auth_result = std::stoi(fields[5]);
                msg.warning = std::stoi(fields[6]);
                msg.status = msg.auth_result; // Use auth_result as status
            } catch (...) {
                return msg;
            }
        } else {
            msg.time_tag = fields[1];
            msg.system = fields[2];
            msg.service = fields[3];
            try {
                msg.auth_result = std::stoi(fields[4]);
                msg.warning = std::stoi(fields[5]);
                msg.status = msg.auth_result;
            } catch (...) {
                return msg;
            }
        }
        msg.valid = true;
    }

    return msg;
}

// ============================================================================
// ISOBUS NMEA Sender
// ============================================================================

class IsobusNmeaSender {
  public:
    IsobusNmeaSender() : initialized_(false) {}

    ~IsobusNmeaSender() { shutdown(); }

    bool initialize(const std::string &can_interface) {
        // Set up CAN hardware
        can_driver_ = std::make_shared<isobus::SocketCANInterface>(can_interface);
        isobus::CANHardwareInterface::set_number_of_can_channels(1);
        isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, can_driver_);

        if (!isobus::CANHardwareInterface::start() || !can_driver_->get_is_valid()) {
            std::cerr << "Failed to start CAN hardware interface on " << can_interface << std::endl;
            return false;
        }

        // Create ISO NAME for our ECU
        // This identifies our device on the ISOBUS network
        isobus::NAME my_name(0);
        my_name.set_arbitrary_address_capable(true);
        my_name.set_industry_group(2); // Agricultural and Forestry Equipment
        my_name.set_device_class(17);  // Sensor Systems
        my_name.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::GlobalNavigationSatelliteSystem));
        my_name.set_identity_number(1);       // Unique serial number
        my_name.set_ecu_instance(0);          // First instance
        my_name.set_function_instance(0);     // First function instance
        my_name.set_device_class_instance(0); // First device class instance
        my_name.set_manufacturer_code(1407);  // Manufacturer code (get from SAE)

        // Create our internal control function (ECU)
        ecu_ = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(my_name, 0);

        if (!ecu_) {
            std::cerr << "Failed to create internal control function" << std::endl;
            return false;
        }

        // Wait for address claiming to complete
        std::cout << "Waiting for address claim..." << std::endl;
        auto address_claimed = std::async(std::launch::async, [this]() {
            int attempts = 0;
            while (!ecu_->get_address_valid() && attempts < 50) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                attempts++;
            }
            return ecu_->get_address_valid();
        });

        if (address_claimed.wait_for(std::chrono::seconds(5)) == std::future_status::timeout ||
            !address_claimed.get()) {
            std::cerr << "Address claiming timed out" << std::endl;
            return false;
        }

        std::cout << "Address claimed successfully! Address: 0x" << std::hex << static_cast<int>(ecu_->get_address())
                  << std::dec << std::endl;

        initialized_ = true;
        return true;
    }

    /**
     * @brief Send a raw NMEA sentence as a proprietary message
     *
     * For messages <= 8 bytes, sends as single frame.
     * For messages > 8 bytes, Transport Protocol is used automatically.
     *
     * @param nmea_sentence The NMEA sentence to send
     * @return true if sent successfully
     */
    bool send_nmea_string(const std::string &nmea_sentence) {
        if (!initialized_ || !ecu_ || !ecu_->get_address_valid()) {
            return false;
        }

        std::vector<std::uint8_t> data(nmea_sentence.begin(), nmea_sentence.end());

        // send_can_message automatically uses Transport Protocol for data > 8 bytes
        return isobus::CANNetworkManager::CANNetwork.send_can_message(PGN_PROPRIETARY_B_NMEA, data.data(), data.size(),
                                                                      ecu_);
    }

    /**
     * @brief Send PHTG authentication data in a compact binary format
     *
     * This encodes the PHTG data efficiently for CAN transmission:
     * Byte 0: Message type identifier (0x01 = PHTG)
     * Byte 1: System (0 = GPS, 1 = GAL)
     * Byte 2: Service (0 = HAS, 1 = OSNMA, 2 = HO)
     * Byte 3: Auth result (0-4)
     * Byte 4: Status (0-3)
     * Byte 5: Warning (0-3)
     * Byte 6-7: Reserved
     *
     * @param msg Parsed PHTG message
     * @return true if sent successfully
     */
    bool send_phtg_binary(const PhtgMessage &msg) {
        if (!initialized_ || !ecu_ || !ecu_->get_address_valid() || !msg.valid) {
            return false;
        }

        std::array<std::uint8_t, 8> data = {0};

        // Message type identifier
        data[0] = 0x01; // PHTG message type

        // System: GPS=0, GAL=1
        if (msg.system == "GAL" || msg.system == "Galileo") {
            data[1] = 1;
        } else {
            data[1] = 0; // GPS
        }

        // Service: HAS=0, OSNMA=1, HO=2
        if (msg.service == "OSNMA") {
            data[2] = 1;
        } else if (msg.service == "HO") {
            data[2] = 2;
        } else {
            data[2] = 0; // HAS
        }

        // Auth result, status, warning
        data[3] = static_cast<std::uint8_t>(msg.auth_result & 0x07);
        data[4] = static_cast<std::uint8_t>(msg.status & 0x03);
        data[5] = static_cast<std::uint8_t>(msg.warning & 0x03);

        // Reserved
        data[6] = 0xFF;
        data[7] = 0xFF;

        return isobus::CANNetworkManager::CANNetwork.send_can_message(PGN_PROPRIETARY_NMEA, data.data(), data.size(),
                                                                      ecu_);
    }

    void shutdown() {
        if (initialized_) {
            isobus::CANHardwareInterface::stop();
            initialized_ = false;
        }
    }

    bool is_initialized() const { return initialized_; }

  private:
    std::shared_ptr<isobus::SocketCANInterface> can_driver_;
    std::shared_ptr<isobus::InternalControlFunction> ecu_;
    bool initialized_;
};

// ============================================================================
// Main
// ============================================================================

void print_usage(const char *prog_name) {
    std::cout << "Usage: " << prog_name << " [can_interface] [serial_port] [baud_rate]" << std::endl;
    std::cout << "  can_interface: CAN interface name (default: can0)" << std::endl;
    std::cout << "  serial_port:   Serial port for NMEA input (default: /dev/ttyUSB0)" << std::endl;
    std::cout << "  baud_rate:     Serial baud rate (default: 115200)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example: " << prog_name << " can0 /dev/ttyUSB0 115200" << std::endl;
}

int main(int argc, char *argv[]) {
    // Parse arguments
    std::string can_interface = "can0";
    std::string serial_port = "/dev/ttyUSB0";
    int baud_rate = 115200;

    if (argc >= 2) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        can_interface = argv[1];
    }
    if (argc >= 3) {
        serial_port = argv[2];
    }
    if (argc >= 4) {
        try {
            baud_rate = std::stoi(argv[3]);
        } catch (...) {
            std::cerr << "Invalid baud rate: " << argv[3] << std::endl;
            return 1;
        }
    }

    std::signal(SIGINT, signal_handler);

    std::cout << "=== ISOBUS NMEA PHTG Sender ===" << std::endl;
    std::cout << "CAN Interface: " << can_interface << std::endl;
    std::cout << "Serial Port:   " << serial_port << std::endl;
    std::cout << "Baud Rate:     " << baud_rate << std::endl;
    std::cout << std::endl;

    // Initialize ISOBUS
    IsobusNmeaSender sender;
    if (!sender.initialize(can_interface)) {
        std::cerr << "Failed to initialize ISOBUS sender" << std::endl;
        return 1;
    }

    // Open serial port
    SerialPort serial;
    if (!serial.open_port(serial_port, baud_rate)) {
        std::cerr << "Failed to open serial port: " << serial_port << std::endl;
        return 1;
    }

    std::cout << "Listening for NMEA PHTG messages. Press Ctrl+C to exit." << std::endl;
    std::cout << std::endl;

    // Statistics
    unsigned long phtg_count = 0;
    unsigned long other_count = 0;

    // Main loop
    std::string line;
    while (running && serial.read_line(line)) {
        if (line.empty() || line[0] != '$') {
            continue;
        }

        // Check if this is a PHTG message
        if (line.find("PHTG") != std::string::npos) {
            PhtgMessage phtg = parse_phtg(line);
            if (phtg.valid) {
                // Send binary format (8 bytes, single frame)
                if (sender.send_phtg_binary(phtg)) {
                    phtg_count++;
                    std::cout << "[PHTG] Sent: System=" << phtg.system << " Service=" << phtg.service
                              << " Auth=" << phtg.auth_result << " Warn=" << phtg.warning << " (count: " << phtg_count
                              << ")" << std::endl;
                } else {
                    std::cerr << "[PHTG] Failed to send message" << std::endl;
                }

                // Optionally also send the full NMEA string using Transport Protocol
                // This is useful if the receiver needs the complete NMEA sentence
                // Uncomment the following to enable:
                // sender.send_nmea_string(line);
            }
        } else {
            // Count other NMEA messages (handled by Python J1939 bridge)
            other_count++;
            if (other_count % 100 == 0) {
                std::cout << "[INFO] Received " << other_count << " other NMEA messages (forwarded to J1939)"
                          << std::endl;
            }
        }
    }

    std::cout << std::endl;
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "PHTG messages sent: " << phtg_count << std::endl;
    std::cout << "Other NMEA messages: " << other_count << std::endl;

    return 0;
}
