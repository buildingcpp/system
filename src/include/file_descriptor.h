#pragma once

#include <include/non_copyable.h>
#include <include/underlying.h>

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

        explicit operator value_type() const;

        bool is_valid() const;

    private:

        value_type   value_{0};
    };


    //=========================================================================
    inline auto to_underlying
    (
        file_descriptor const & fileDescriptor
    ) -> file_descriptor::value_type
    {
        return static_cast<file_descriptor::value_type>(fileDescriptor);
    }

} // namespace bcpp::system


namespace bcpp
{

    template <>
    struct underlying_type<system::file_descriptor>
    {
        using type = system::file_descriptor::value_type;
    };

} // namespace bcpp


//=============================================================================
[[maybe_unused]]
static std::ostream & operator << 
(
    std::ostream & stream,
    bcpp::system::file_descriptor const & fileDescriptor
)
{
    stream << bcpp::to_underlying_fundamental(fileDescriptor);
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
inline bcpp::system::file_descriptor::operator value_type
(
) const
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
