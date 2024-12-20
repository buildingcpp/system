#include "./anonymous_mapping.h"


//=============================================================================
bcpp::system::anonymous_mapping::anonymous_mapping
(
    configuration const & config,
    event_handlers const & eventHandlers
):
    memory_mapping(
            memory_mapping::configuration{
                .size_ = config.size_,
                .ioMode_ = io_mode::read_write,
                .mmapFlags_ = config.mmapFlags_ | MAP_PRIVATE | MAP_ANONYMOUS,
                .alignment_ = config.alignment_
            },
            memory_mapping::event_handlers{
                .closeHandler_ = [closeHandler = eventHandlers.closeHandler_]
                        (
                            auto const & memoryMapping
                        ) mutable
                        {
                            if (auto handler = std::exchange(closeHandler, nullptr); handler != nullptr)
                                handler(reinterpret_cast<anonymous_mapping const &>(memoryMapping));
                        }
            },
            {})
{
}
