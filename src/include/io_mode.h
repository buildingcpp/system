#pragma once

#include <cstdint>


namespace bcpp::system
{

    enum class io_mode : std::uint32_t
    {
        undefined       = 0,
        none            = 0,
        read            = 1,
        write           = 2,
        read_write      = 3
    }; // io_mode

} // namespace bcpp::system