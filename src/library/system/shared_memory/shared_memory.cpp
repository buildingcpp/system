#include "./shared_memory.h"

#include <include/file_descriptor.h>

#include <utility>
#include <sys/mman.h>
#include <chrono>
#include <format>


//=============================================================================
auto bcpp::system::shared_memory::create
(
    create_configuration const & config,
    event_handlers const & eventHandlers
) -> shared_memory
{
    return {config, eventHandlers};
}


//=============================================================================
auto bcpp::system::shared_memory::join
(
    join_configuration const & config,
    event_handlers const & eventHandlers
) -> shared_memory
{
    return {config, eventHandlers};
}


//=============================================================================
bcpp::system::shared_memory::shared_memory
(
    create_configuration const & config,
    event_handlers const & eventHandlers
):
    closeHandler_(eventHandlers.closeHandler_),
    unlinkHandler_(eventHandlers.unlinkHandler_),
    unlinkPolicy_(config.unlinkPolicy_),
    path_(config.path_)
{
    if (config.size_)
    {
        if (path_.empty())
        {
            // random path
            path_ = std::format("bcpp.{}", std::chrono::system_clock::now().time_since_epoch().count());
        }
        auto prevUMask = ::umask(0);
        std::int32_t prot = 0;
        std::int32_t flags = 0;
        switch (config.ioMode_)
        {
            case io_mode::none: prot = PROT_NONE; flags = O_CREAT; break;
            case io_mode::read: prot = PROT_READ; flags = O_CREAT | O_RDONLY; break;
            case io_mode::write: prot = PROT_WRITE; flags = O_CREAT | O_RDWR; break;
            case io_mode::read_write: prot = PROT_READ | PROT_WRITE; flags = O_CREAT | O_RDWR; break;
        }

        file_descriptor fileDescriptor({::shm_open(path_.c_str(), flags | O_EXCL, 0666)});
        ::umask(prevUMask);
        if (fileDescriptor.is_valid())
        {
            if (auto ret = ::ftruncate(fileDescriptor.get(), config.size_); ret == 0)
            {
                if (auto allocation = ::mmap(nullptr, config.size_, prot, config.mapFlags_, fileDescriptor.get(), 0ull); allocation != MAP_FAILED)
                    allocation_ = {reinterpret_cast<std::byte *>(allocation), config.size_};
                if ((unlinkPolicy_ == unlink_policy::on_attach) || (allocation_.data() == nullptr))
                    unlink();
            }
        }
    }
}


//=============================================================================
bcpp::system::shared_memory::shared_memory
(
    join_configuration const & config,
    event_handlers const & eventHandlers
):
    closeHandler_(eventHandlers.closeHandler_),
    unlinkHandler_(eventHandlers.unlinkHandler_),
    unlinkPolicy_(config.unlinkPolicy_),
    path_(config.path_)
{
    if (!path_.empty())
    {
        auto prevUMask = ::umask(0);
        std::int32_t prot = 0;
        std::int32_t flags = 0;
        switch (config.ioMode_)
        {
            case io_mode::none: prot = PROT_NONE; flags = 0; break;
            case io_mode::read: prot = PROT_READ; flags = O_RDONLY; break;
            case io_mode::write: prot = PROT_WRITE; flags = O_RDWR; break;
            case io_mode::read_write: prot = PROT_READ | PROT_WRITE; flags = O_RDWR; break;
        }
        file_descriptor fileDescriptor({::shm_open(path_.c_str(), flags, 0666)});
        ::umask(prevUMask);
        if (fileDescriptor.is_valid())
        {
            struct stat fileStat;
            ::fstat(fileDescriptor.get(), &fileStat);
            if (auto allocation = ::mmap(nullptr, fileStat.st_size, prot, config.mapFlags_, fileDescriptor.get(), 0ull); allocation != MAP_FAILED)
                    allocation_ = {reinterpret_cast<std::byte *>(allocation), (unsigned)fileStat.st_size};
            if ((unlinkPolicy_ == unlink_policy::on_attach) || (allocation_.data() == nullptr))
                unlink();
        }
    }
}


//=============================================================================
bcpp::system::shared_memory::shared_memory
(
    shared_memory && other
):
    closeHandler_(other.closeHandler_),
    unlinkHandler_(other.unlinkHandler_),
    allocation_(other.allocation_),
    unlinkPolicy_(other.unlinkPolicy_),
    path_(other.path_)
{
    other.closeHandler_ = nullptr;
    other.unlinkHandler_ = nullptr;
    other.allocation_ = {};
    other.path_ = {};
}


//=============================================================================
auto bcpp::system::shared_memory::operator = 
(
    shared_memory && other
) -> shared_memory & 
{
    if (this != &other)
    {
        close();
        closeHandler_ = other.closeHandler_;
        unlinkHandler_ = other.unlinkHandler_;
        allocation_ = other.allocation_;
        unlinkPolicy_ = other.unlinkPolicy_;
        path_ = other.path_;
        other.closeHandler_ = nullptr;
        other.unlinkHandler_ = nullptr;
        other.allocation_ = {};
        other.path_ = {};
    }
    return *this;
}


//=============================================================================
bcpp::system::shared_memory::~shared_memory
(
)
{
    close();
}


//=============================================================================
void bcpp::system::shared_memory::close
(
)
{
    if (auto closeHandler = std::exchange(closeHandler_, nullptr); closeHandler)
        closeHandler(*this);
    detach();
}


//=============================================================================
void bcpp::system::shared_memory::unlink
(
)
{
    if (!path_.empty())
    {
        if (auto unlinkHandler = std::exchange(unlinkHandler_, nullptr); unlinkHandler)
            unlinkHandler(*this);
        ::shm_unlink(path_.c_str());
        path_ = {};
    }
}


//=============================================================================
void bcpp::system::shared_memory::detach
(
)
{
    if (unlinkPolicy_ == unlink_policy::on_detach)
        unlink();
    if (allocation_.data() != nullptr)
        ::munmap(allocation_.data(), allocation_.size());
    allocation_ = {};
}


//=============================================================================
std::string bcpp::system::shared_memory::path
(
) const
{
    return path_;
}


//=============================================================================
std::byte * bcpp::system::shared_memory::data
(
)
{
    return allocation_.data();
}


//=============================================================================
std::byte const * bcpp::system::shared_memory::data
(
) const
{
    return allocation_.data();
}


//=============================================================================
std::size_t bcpp::system::shared_memory::size
(
) const
{
    return allocation_.size();
}


//=============================================================================
bool bcpp::system::shared_memory::is_valid
(
) const
{
    return (allocation_.data() != nullptr);
}
