#pragma once

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

    class shared_memory final :
        non_copyable
    {
    public:

        using close_handler = std::function<void(shared_memory const &)>;
        using unlink_handler = std::function<void(shared_memory const &)>;

        enum class unlink_policy
        {
            never,
            on_attach,
            on_detach
        };
        static auto constexpr default_unlink_policy = unlink_policy::never;

        struct create_configuration
        {
            std::string     path_;
            std::size_t     size_;
            io_mode         ioMode_;
            std::size_t     mmapFlags_ = {MAP_SHARED};
            unlink_policy   unlinkPolicy_{default_unlink_policy};
        };

        struct join_configuration
        {
            std::string     path_;
            io_mode         ioMode_;
            std::size_t     mmapFlags_ = {MAP_SHARED};
            unlink_policy   unlinkPolicy_{default_unlink_policy};
        };

        struct event_handlers
        {
            close_handler   closeHandler_;
            unlink_handler  unlinkHandler_;
        };

        static shared_memory create
        (
            create_configuration const &,
            event_handlers const &
        );

        static shared_memory join
        (
            join_configuration const &,
            event_handlers const &
        );

        shared_memory() = default;
        
        shared_memory(shared_memory &&);

        shared_memory & operator = (shared_memory &&);

        ~shared_memory();

        void close();

        template <typename T>
        T const & as() const;

        template <typename T>
        T & as();

        std::byte const * data() const;

        std::byte * data();

        std::size_t size() const;

        bool is_valid() const;

        void unlink();

        std::string path() const;

        std::byte * begin();
        std::byte const * begin() const;
        std::byte * end();
        std::byte const * end() const;

    private:

        shared_memory
        (
            create_configuration const &,
            event_handlers const &
        );

        shared_memory
        (
            join_configuration const &,
            event_handlers const &
        );

        void detach();

        close_handler           closeHandler_;

        unlink_handler          unlinkHandler_;

        std::span<std::byte>    allocation_; 

        unlink_policy           unlinkPolicy_{default_unlink_policy};

        std::string             path_;

    }; // class shared_memory

} // namespace bcpp::system


//=============================================================================
template <typename T>
T const & bcpp::system::shared_memory::as
(
) const
{
    return *(reinterpret_cast<T const *>(data()));
}


//=============================================================================
template <typename T>
T & bcpp::system::shared_memory::as
(
)
{
    return *(reinterpret_cast<T *>(data()));
}
