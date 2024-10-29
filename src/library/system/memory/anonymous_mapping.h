#pragma once

#include "./memory_mapping.h"


namespace bcpp::system
{

    class anonymous_mapping final :
        public memory_mapping,
        virtual non_copyable
    {
    public:

        using close_handler = std::function<void(anonymous_mapping const &)>;

        struct configuration
        {
            std::size_t     size_;
            std::size_t     mmapFlags_ = {MAP_PRIVATE | MAP_ANONYMOUS};
            std::size_t     alignment_{1024};
        };

        struct event_handlers
        {
            close_handler   closeHandler_;
        };

        anonymous_mapping
        (
            configuration const &,
            event_handlers const &
        );

        anonymous_mapping() = default;
        anonymous_mapping(anonymous_mapping &&) = default;
        anonymous_mapping & operator = (anonymous_mapping &&) = default;
        ~anonymous_mapping() = default;

    }; // class anonymous_mapping

} // namespace bcpp::system
