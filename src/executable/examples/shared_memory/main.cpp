#include <library/system.h>

#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>



//=============================================================================
int main
(
    int argc,
    char ** args
)
{
    using namespace bcpp::system;

    auto sharedMemory = shared_memory::create(
            {
                .path_ = "",
                .size_ = 1024,
                .ioMode_ = io_mode::write,
                .unlinkPolicy_ = shared_memory::unlink_policy::on_detach
            },
            {
                .closeHandler_ = [](auto const &){std::cout << "memory closed\n";},
                .unlinkHandler_ = [](auto const &){std::cout << "creater unlinked\n";}
            });
    if (sharedMemory.is_valid())
        std::cout << "address = " << sharedMemory.data() << ", size = " << sharedMemory.size() << "\n";

    auto joinMemory = shared_memory::join(
            {
                .path_ = sharedMemory.path(),
                .ioMode_ = io_mode::write,
                .unlinkPolicy_ = shared_memory::unlink_policy::on_attach
            },
            {
                .closeHandler_ = [](auto const &){std::cout << "join memory closed\n";},
                .unlinkHandler_ = [](auto const &){std::cout << "joiner unlinked\n";}
            });

    auto & flagWrite = sharedMemory.as<bool volatile>();
    flagWrite = false;
    auto & flagRead = joinMemory.as<bool volatile>();
    flagRead = false;

    std::jthread t([&](){while (flagRead == false); std::cout << "Got message\n";});

    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "sending message\n";
    flagWrite = true;

    return 0;
}
