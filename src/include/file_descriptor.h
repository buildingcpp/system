#pragma once

#include <include/non_copyable.h>

#include <cstdint>
#include <utility>
#include <unistd.h>

#include <iostream>


namespace bcpp::system
{

    class file_descriptor :
        non_copyable
    {
    public:

        using value_type = std::int32_t;

        file_descriptor() = default;

        file_descriptor
        (
            value_type
        );

        file_descriptor
        (
            file_descriptor &&
        );

        file_descriptor & operator =
        (
            file_descriptor &&
        );

        ~file_descriptor();

        bool close();

        value_type get() const;

        bool is_valid() const;

    private:

        value_type   value_{0};
    };

} // namespace bcpp::system


//=============================================================================
static std::ostream & operator << 
(
    std::ostream & stream,
    bcpp::system::file_descriptor const & fileDescriptor
)
{
    stream << fileDescriptor.get();
    return stream;
}


//=============================================================================
inline bcpp::system::file_descriptor::file_descriptor
(
    value_type value
):
    value_(value)
{
}


//=============================================================================
inline bcpp::system::file_descriptor::file_descriptor
(
    file_descriptor && other
)
{
    value_ = other.value_;
    other.value_ = {};
}



//=============================================================================
inline bcpp::system::file_descriptor::~file_descriptor
(
)
{
    close();
}


//=============================================================================
inline auto bcpp::system::file_descriptor::operator =
(
    file_descriptor && other
) -> file_descriptor & 
{
    if (this != &other)
    {
        close();
        value_ = other.value_;
        other.value_ = {};
    }
    return *this;
}


//=============================================================================
inline bool bcpp::system::file_descriptor::close
(
)
{
    auto value = std::exchange(value_, 0);
    if (value != 0)
        ::close(value);
    return (value != 0);
}


//=============================================================================
inline auto bcpp::system::file_descriptor::get
(
) const -> value_type
{
    return value_;
}


//=============================================================================
inline bool bcpp::system::file_descriptor::is_valid
(
) const
{
    return (value_ > 0);
}
