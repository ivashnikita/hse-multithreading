#include "mpsc_queue.h"

#include <cstring>
#include <iostream>
#include <string>

enum msg_type : uint32_t {
    text = 1,
    number = 2,
    quit = 3,
};

int main(int argc, char* argv[]) {
    bool create = true;
    if (argc > 1 && std::strcmp(argv[1], "--join") == 0) {
        create = false;
    }

    ProducerNode producer("/hw4_demo", 65536, create);

    std::string txt = create ? "Hello from producer 1!" : "Hello from producer 2!";
    producer.send(text, txt.data(), txt.size());
    std::cout << "Sent: text \"" << txt << "\"" << std::endl;

    uint32_t num = create ? 42 : 99;
    producer.send(number, &num, sizeof(num));
    std::cout << "Sent: number " << num << std::endl;

    if (!create) {
        producer.send(quit, nullptr, 0);
        std::cout << "Sent: quit" << std::endl;
    }
}
