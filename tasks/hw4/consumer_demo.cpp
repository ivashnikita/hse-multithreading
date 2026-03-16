#include "mpsc_queue.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

enum msg_type : uint32_t {
    text = 1,
    number = 2,
    quit = 3,
};

int main() {
    ConsumerNode consumer("/hw4_demo", 65536);

    while (true) {
        uint32_t type;
        std::vector<uint8_t> data;

        while (!consumer.receive(type, data)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        switch (type) {
        case text:
            std::cout << "Received text: " << std::string(data.begin(), data.end()) << std::endl;
            break;
        case number: {
            uint32_t value;
            std::memcpy(&value, data.data(), sizeof(value));
            std::cout << "Received number: " << value << std::endl;
            break;
        }
        case quit:
            std::cout << "Received quit, exiting" << std::endl;
            return 0;
        default:
            std::cout << "Received unknown type: " << type << std::endl;
        }
    }
}
