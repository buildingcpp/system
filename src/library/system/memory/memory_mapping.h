#pragma once

#include <include/file_descriptor.h>
#include <include/io_mode.h>
#include <include/non_copyable.h>

#include <span>
#include <functional>
#include <cstdint>
#include <string>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>


namespace bcpp::system
{

    class memory_mapping :
        virtual non_copyable
    {
    public:

        using close_handler = std::function<void(memory_mapping const &)>;

        struct configuration
        {
            std::size_t     size_;
            io_mode         ioMode_{PROT_READ | PROT_WRITE};
            std::size_t     mmapFlags_;
        };

        struct event_handlers
        {
            close_handler   closeHandler_;
        };

        memory_mapping
        (
            configuration const &,
            event_handlers const &,
            file_descriptor const &
        );

        memory_mapping() = default;
        
        memory_mapping(memory_mapping &&);

        memory_mapping & operator = (memory_mapping &&);

        virtual ~memory_mapping();

        void close();

        template <typename T>
        T const & as() const;

        template <typename T>
        T & as();

        std::byte const * data() const;

        std::byte * data();

        std::size_t size() const;

        bool is_valid() const;

        std::byte * begin();
        std::byte const * begin() const;
        std::byte * end();
        std::byte const * end() const;

    private:

        close_handler           closeHandler_;

        std::span<std::byte>    allocation_; 

    }; // class memory_mapping

} // namespace bcpp::system


//=============================================================================
template <typename T>
T const & bcpp::system::memory_mapping::as
(
) const
{
    return *(reinterpret_cast<T const *>(data()));
}


//=============================================================================
template <typename T>
T & bcpp::system::memory_mapping::as
(
)
{
    return *(reinterpret_cast<T *>(data()));
}
