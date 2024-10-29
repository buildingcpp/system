#include "./shared_memory.h"

#include <include/file_descriptor.h>

#include <utility>
#include <chrono>
#include <string>

#include <sys/mman.h>


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
            path_ = "bcpp.";
            path_ += std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        }
        auto prevUMask = ::umask(0);
        std::int32_t flags = 0;
        switch (config.ioMode_)
        {
            case io_mode::none: flags = O_CREAT; break;
            case io_mode::read: flags = O_CREAT | O_RDONLY; break;
            case io_mode::write: flags = O_CREAT | O_RDWR; break;
            case io_mode::read_write: flags = O_CREAT | O_RDWR; break;
        }

        file_descriptor fileDescriptor({::shm_open(path_.c_str(), flags | O_EXCL, 0666)});
        ::umask(prevUMask);
        if (fileDescriptor.is_valid())
        {
            if (auto ret = ::ftruncate(fileDescriptor.get(), config.size_); ret == 0)
            {
                memoryMapping_ = std::move(memory_mapping(
                        {
                            .size_ = config.size_,
                            .ioMode_ = config.ioMode_,
                            .mmapFlags_ = config.mmapFlags_ | MAP_SHARED,
                            .alignment_ = 0
                        },
                        {
                        }, fileDescriptor));
                if ((unlinkPolicy_ == unlink_policy::on_attach) || (memoryMapping_.data() == nullptr))
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
        std::int32_t flags = 0;
        switch (config.ioMode_)
        {
            case io_mode::none: flags = 0; break;
            case io_mode::read: flags = O_RDONLY; break;
            case io_mode::write: flags = O_RDWR; break;
            case io_mode::read_write: flags = O_RDWR; break;
        }
        file_descriptor fileDescriptor({::shm_open(path_.c_str(), flags, 0666)});
        ::umask(prevUMask);
        if (fileDescriptor.is_valid())
        {
            struct stat fileStat;
            ::fstat(fileDescriptor.get(), &fileStat);
            memoryMapping_ = std::move(memory_mapping(
                    {
                        .size_ = (unsigned)fileStat.st_size,
                        .ioMode_ = config.ioMode_,
                        .mmapFlags_ = config.mmapFlags_ | MAP_SHARED,
                        .alignment_ = 0
                    },
                    {
                    }, fileDescriptor));
            if ((unlinkPolicy_ == unlink_policy::on_attach) || (memoryMapping_.data() == nullptr))
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
    unlinkPolicy_(other.unlinkPolicy_),
    path_(other.path_),
    memoryMapping_(std::move(other.memoryMapping_))
{
    other.closeHandler_ = nullptr;
    other.unlinkHandler_ = nullptr;
    other.path_ = {};
    other.memoryMapping_ = {};
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
        unlinkPolicy_ = other.unlinkPolicy_;
        memoryMapping_ = std::move(other.memoryMapping_);
        path_ = other.path_;
        other.closeHandler_ = nullptr;
        other.unlinkHandler_ = nullptr;
        other.path_ = {};
        other.memoryMapping_ = {};
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
    if (unlinkPolicy_ == unlink_policy::on_detach)
        unlink();
    memoryMapping_ = {};
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
    return memoryMapping_.data();
}


//=============================================================================
std::byte const * bcpp::system::shared_memory::data
(
) const
{
    return memoryMapping_.data();
}


//=============================================================================
std::size_t bcpp::system::shared_memory::size
(
) const
{
    return memoryMapping_.size();
}


//=============================================================================
bool bcpp::system::shared_memory::is_valid
(
) const
{
    return (memoryMapping_.data() != nullptr);
}


//=============================================================================
std::byte * bcpp::system::shared_memory::begin
(
)
{
    return memoryMapping_.data();
}


//=============================================================================
std::byte const * bcpp::system::shared_memory::begin
(
) const
{
    return memoryMapping_.data();
}


//=============================================================================
std::byte * bcpp::system::shared_memory::end
(
)
{
    return (memoryMapping_.data() + memoryMapping_.size());
}


//=============================================================================
std::byte const * bcpp::system::shared_memory::end
(
) const
{
    return (memoryMapping_.data() + memoryMapping_.size());
}
