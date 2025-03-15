#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <unistd.h> 
#include <cstring> 
#include <iomanip> 
#include <csignal>
#include <nlohmann/json.hpp>

// Linux-специфичные заголовки для SPI
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <linux/spi/spidev.h>

using json = nlohmann::json;

// Класс для управления SPI
class SpiController {
    public:
        SpiController(const std::string& device) : devicePath(device) {
            openDevice();
        }

        ~SpiController() {
            closeDevice();
        }

        void setSpeed(uint32_t speedHz) {
            if (ioctl(fileDescriptor, SPI_IOC_WR_MAX_SPEED_HZ, &speedHz) == -1) {
                throw std::runtime_error("Ошибка при установке скорости SPI");
            }
        }

        std::vector<uint8_t> transfer(const std::vector<uint8_t>& txData) {
            std::vector<uint8_t> rxData(txData.size());

            spi_ioc_transfer transfer;
            memset(&transfer, 0, sizeof(transfer));

            transfer.tx_buf = (unsigned long)txData.data();
            transfer.rx_buf = (unsigned long)rxData.data();
            transfer.len = txData.size();
            transfer.speed_hz = 0;

            if (ioctl(fileDescriptor, SPI_IOC_MESSAGE(1), &transfer) < 0) {
                throw std::runtime_error("Ошибка при передаче SPI");
            }

            return rxData;
        }

    private:
        void openDevice() {
            fileDescriptor = open(devicePath.c_str(), O_RDWR);
            if (fileDescriptor < 0) {
                std::stringstream ss;
                ss << "Ошибка открытия устройства SPI: " << devicePath;
                throw std::runtime_error(ss.str());
            }
        }

        void closeDevice() {
            if (fileDescriptor != -1) {
                close(fileDescriptor);
            }
        }

        std::string devicePath;
        int fileDescriptor = -1;
};

void freqTest(SpiController& spi) {
    std::vector<uint8_t> send = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
    std::string goodResult = "hello world";

    uint32_t kHz = 1000;
    uint32_t MHz = 1000000;

    std::vector<uint32_t> speeds = {
        10 * kHz,
        30 * kHz,
        100 * kHz,
        300 * kHz,
        1 * MHz,
        3 * MHz,
        10 * MHz,
        30 * MHz,
        40 * MHz,
        50 * MHz,
    };

    std::cout << std::left << std::setw(15) << "Speed, Hz" << std::setw(30) << "error_cnt" << std::endl;

    for (uint32_t speed : speeds) {
        spi.setSpeed(speed);

        int errorCnt = 0;
        for (int i = 0; i < 10; ++i) {
            std::vector<uint8_t> received = spi.transfer(send);
            std::string receivedString(received.begin(), received.end());
            if (receivedString != goodResult) {
                errorCnt++;
            }
        }
        std::cout << std::left << std::setw(15) << speed << std::setw(30) << errorCnt << std::endl;
    }
}

volatile sig_atomic_t stop = 0;

void handleSignal(int signal) {
    stop = 1;
}

std::vector<uint8_t> createMessage(const std::vector<uint32_t>& values) {
    std::vector<uint8_t> message;
    for (uint32_t value : values) {
        message.push_back((value >> 24) & 0xFF);
        message.push_back((value >> 16) & 0xFF);
        message.push_back((value >> 8) & 0xFF);
        message.push_back(value & 0xFF);
    }
    return message;
}

void continuousTransfer(SpiController& spi) {
	uint32_t value_1 = 0x75BCD15;
	uint32_t value_2 = 0x3ADE68B1;
	uint32_t value_3 = 0xACACAAAA;
    std::vector<uint32_t> values = {value_1, value_2, value_3};

    std::vector<uint8_t> send = createMessage(values);

    signal(SIGINT, handleSignal);

    uint32_t speedHz = 1000000; // 1 MHz
    spi.setSpeed(speedHz);

    while (!stop) {
        spi.transfer(send);
        sleep(0.5);
    }
}

void jsonTransfer(SpiController& spi) {
    std::string filename = "data.json";
    std::ifstream file(filename);
    json j;

    if (!file.is_open()) {
        j = {
            {"values", {0x75BCD15, 0x3ADE68B1, 0xACACAAAA}}
        };
        std::ofstream o(filename);
        o << std::setw(4) << j << std::endl;
    } else {
        file >> j;
    }

    std::vector<uint32_t> values = j["values"].get<std::vector<uint32_t>>();
    std::vector<uint8_t> send = createMessage(values);

    signal(SIGINT, handleSignal);

    uint32_t speedHz = 1000000; // 1 MHz
    spi.setSpeed(speedHz);

    while (!stop) {
        spi.transfer(send);
        sleep(0.5);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <mode>" << std::endl;
        std::cerr << "Mode 1: Frequency test" << std::endl;
        std::cerr << "Mode 2: Continuous transfer" << std::endl;
        std::cerr << "Mode 3: JSON transfer" << std::endl;
        return 1;
    }

    int mode = std::stoi(argv[1]);
    std::string device = "/dev/spidev1.0";

    try {
        SpiController spi(device);

        if (mode == 1) {
            freqTest(spi);
        } else if (mode == 2) {
            continuousTransfer(spi);
        } else if (mode == 3) {
            jsonTransfer(spi);
        } else {
            std::cerr << "Invalid mode selected" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
