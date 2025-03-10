#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <unistd.h> // Для usleep
#include <cstring> // Для memcpy
#include <iomanip> //  Добавлен для setw

// Linux-специфичные заголовки для SPI
#include <fcntl.h> // Для open
#include <sys/ioctl.h> // Для ioctl
#include <linux/spi/spidev.h> // Для структуры spi_ioc_transfer

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
        memset(&transfer, 0, sizeof(transfer)); // Инициализация нулями

        transfer.tx_buf = (unsigned long)txData.data();
        transfer.rx_buf = (unsigned long)rxData.data();
        transfer.len = txData.size();
        transfer.speed_hz = 0; // Используем установленную скорость

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


int main() {
    std::string device = "/dev/spidev1.0"; // Укажите устройство SPI

    try {
        SpiController spi(device);
        freqTest(spi);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
