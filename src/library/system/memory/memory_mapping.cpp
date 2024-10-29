#include "./memory_mapping.h"

#include <include/file_descriptor.h>
#include <include/bit.h>

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
        auto alignment = config.alignment_ ? minimum_power_of_two(config.alignment_) : 0;
        auto allocationSize = config.size_ + alignment;
        std::int32_t prot = 0;
        switch (config.ioMode_)
        {
            case io_mode::none: prot = PROT_NONE; break;
            case io_mode::read: prot = PROT_READ; break;
            case io_mode::write: prot = PROT_WRITE; break;
            case io_mode::read_write: prot = PROT_READ | PROT_WRITE; break;
        }
        if (auto unalignedAddress = ::mmap(nullptr, allocationSize, prot, config.mmapFlags_, fileDescriptor.get(), 0ull); unalignedAddress != MAP_FAILED)
        {
            unalignedAllocation_ = {reinterpret_cast<std::byte *>(unalignedAddress), allocationSize};
            auto alignedAddress = unalignedAllocation_.data();
            if (alignment != 0)
                alignedAddress = reinterpret_cast<std::byte *>((reinterpret_cast<std::size_t>(unalignedAddress) + alignment - 1) & ~(alignment - 1));
            alignedAllocation_ = {alignedAddress, config.size_};
        }
    }
}


//=============================================================================
bcpp::system::memory_mapping::memory_mapping
(
    memory_mapping && other
):
    closeHandler_(other.closeHandler_),
    alignedAllocation_(other.alignedAllocation_),
    unalignedAllocation_(other.unalignedAllocation_)
{
    other.closeHandler_ = nullptr;
    other.alignedAllocation_ = {};
    other.unalignedAllocation_ = {};
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
        alignedAllocation_ = other.alignedAllocation_;
        unalignedAllocation_ = other.unalignedAllocation_;
        other.closeHandler_ = nullptr;
        other.alignedAllocation_ = {};
        other.unalignedAllocation_ = {};
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
    if (unalignedAllocation_.data() != nullptr)
        ::munmap(unalignedAllocation_.data(), unalignedAllocation_.size());
    unalignedAllocation_ = {};
    alignedAllocation_ = {};
}


//=============================================================================
std::byte * bcpp::system::memory_mapping::data
(
)
{
    return alignedAllocation_.data();
}


//=============================================================================
std::byte const * bcpp::system::memory_mapping::data
(
) const
{
    return alignedAllocation_.data();
}


//=============================================================================
std::size_t bcpp::system::memory_mapping::size
(
) const
{
    return alignedAllocation_.size();
}


//=============================================================================
bool bcpp::system::memory_mapping::is_valid
(
) const
{
    return (alignedAllocation_.data() != nullptr);
}


//=============================================================================
std::byte * bcpp::system::memory_mapping::begin
(
)
{
    return alignedAllocation_.data();
}


//=============================================================================
std::byte const * bcpp::system::memory_mapping::begin
(
) const
{
    return alignedAllocation_.data();
}


//=============================================================================
std::byte * bcpp::system::memory_mapping::end
(
)
{
    return (alignedAllocation_.data() + alignedAllocation_.size());
}


//=============================================================================
std::byte const * bcpp::system::memory_mapping::end
(
) const
{
    return (alignedAllocation_.data() + alignedAllocation_.size());
}
