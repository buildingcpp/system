#include "./memory_mapping.h"

#include <include/file_descriptor.h>

#include <utility>
#include <chrono>
#include <string>

#include <sys/mman.h>


//=============================================================================
bcpp::system::memory_mapping::memory_mapping
(
    configuration const & config,
    event_handlers const & eventHandlers,
    file_descriptor const & fileDescriptor
):
    closeHandler_(eventHandlers.closeHandler_)
{
    if (config.size_)
    {
        std::int32_t prot = 0;
        switch (config.ioMode_)
        {
            case io_mode::none: prot = PROT_NONE; break;
            case io_mode::read: prot = PROT_READ; break;
            case io_mode::write: prot = PROT_WRITE; break;
            case io_mode::read_write: prot = PROT_READ | PROT_WRITE; break;
        }
        if (auto allocation = ::mmap(nullptr, config.size_, prot, config.mmapFlags_, fileDescriptor.get(), 0ull); allocation != MAP_FAILED)
            allocation_ = {reinterpret_cast<std::byte *>(allocation), config.size_};
    }
}


//=============================================================================
bcpp::system::memory_mapping::memory_mapping
(
    memory_mapping && other
):
    closeHandler_(other.closeHandler_),
    allocation_(other.allocation_)
{
    other.closeHandler_ = nullptr;
    other.allocation_ = {};
}


//=============================================================================
auto bcpp::system::memory_mapping::operator = 
(
    memory_mapping && other
) -> memory_mapping & 
{
    if (this != &other)
    {
        close();
        closeHandler_ = other.closeHandler_;
        allocation_ = other.allocation_;
        other.closeHandler_ = nullptr;
        other.allocation_ = {};
    }
    return *this;
}


//=============================================================================
bcpp::system::memory_mapping::~memory_mapping
(
)
{
    close();
}


//=============================================================================
void bcpp::system::memory_mapping::close
(
)
{
    if (auto closeHandler = std::exchange(closeHandler_, nullptr); closeHandler)
        closeHandler(*this);
    if (allocation_.data() != nullptr)
        ::munmap(allocation_.data(), allocation_.size());
    allocation_ = {};
}


//=============================================================================
std::byte * bcpp::system::memory_mapping::data
(
)
{
    return allocation_.data();
}


//=============================================================================
std::byte const * bcpp::system::memory_mapping::data
(
) const
{
    return allocation_.data();
}


//=============================================================================
std::size_t bcpp::system::memory_mapping::size
(
) const
{
    return allocation_.size();
}


//=============================================================================
bool bcpp::system::memory_mapping::is_valid
(
) const
{
    return (allocation_.data() != nullptr);
}


//=============================================================================
std::byte * bcpp::system::memory_mapping::begin
(
)
{
    return allocation_.data();
}


//=============================================================================
std::byte const * bcpp::system::memory_mapping::begin
(
) const
{
    return allocation_.data();
}


//=============================================================================
std::byte * bcpp::system::memory_mapping::end
(
)
{
    return (allocation_.data() + allocation_.size());
}


//=============================================================================
std::byte const * bcpp::system::memory_mapping::end
(
) const
{
    return (allocation_.data() + allocation_.size());
}
