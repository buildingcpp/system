#pragma once

#include <library/system.h>

#include <cstddef>
#include <vector>
#include <span>
#include <fstream>
#include <condition_variable>
#include <mutex>
#include <filesystem>


class word_stream
{
public:

    word_stream
    (
        std::filesystem::path,
        std::uint32_t,
        std::uint32_t,
        bcpp::system::blocking_work_contract_group &
    );

    void start(){readPacketContract_.schedule();}

    std::tuple<std::uint64_t, std::uint64_t> await_result() const
    {
        std::unique_lock uniqueLock(mutex_);
        conditionVariable_.wait(uniqueLock, [&](){return (bytesProcessed_ >= streamSize_);}); 
        return {totalWords_, bytesProcessed_};
    }

private:

    void enqueue_packet
    (
        std::uint32_t
    );

    void process_next_packet();

    std::filesystem::path                   path_;
    std::ifstream                           inputStream_;
    bcpp::system::blocking_work_contract    readPacketContract_;
    bcpp::system::blocking_work_contract    processPacketContract_;
    std::atomic<std::uint64_t>              totalWords_{0};
    std::atomic<std::uint64_t>              bytesProcessed_{0};
    std::atomic<std::uint64_t>              streamSize_{0};

    std::condition_variable mutable         conditionVariable_;
    std::mutex mutable                      mutex_;

    std::vector<std::vector<char>>          queue_;
    std::atomic<std::size_t>                enqueCounter_{0};
    std::atomic<std::size_t>                dequeCounter_{0};
    std::size_t                             capacity_{0};
    std::size_t                             capacityMask_{0};

    char prev_{' '};
};


//=============================================================================
inline word_stream::word_stream
(
    std::filesystem::path path,
    std::uint32_t maxPacketSize,
    std::uint32_t maxPacketQueueCapacity,
    bcpp::system::blocking_work_contract_group & workContractGroup
):
    path_(path),
    processPacketContract_(workContractGroup.create_contract([this](){this->process_next_packet();})),
    readPacketContract_(workContractGroup.create_contract([this, maxPacketSize](){this->enqueue_packet(maxPacketSize);}))
{
    // ensure that queue capacity is a power of two
    auto i = 1;
    while (i < maxPacketQueueCapacity)
        i <<= 1;
    capacity_ = i;
    capacityMask_ = capacity_ - 1;
    queue_.resize(capacity_);

    inputStream_ = std::ifstream(path, std::ios_base::in | std::ios_base::binary);
    if (inputStream_.is_open())
        streamSize_ = std::filesystem::file_size(path_);
    else
        std::cerr << "word_stream:: failed to open path = " << path_ << "\n";
}


//=============================================================================
inline void word_stream::enqueue_packet
(
    // load next packet from the input stream and queue it for asynchronous processing
    std::uint32_t maxPacketSize
)
{
    auto enqueCounter = enqueCounter_.load();
    auto queueSpace = capacity_ - (enqueCounter - dequeCounter_.load());
    if (queueSpace > 0)
    {
        // load another packet and queue it for processing
        std::vector<char> packet(maxPacketSize);
        inputStream_.read(packet.data(), maxPacketSize);
        auto sizeRead = inputStream_.gcount();    
        if (sizeRead == 0)
        {
            // failed to read any data 
            if (inputStream_.eof())
                readPacketContract_.release();
            readPacketContract_.schedule();// re-schedule to read more packets
            return;
        }
        packet.resize(sizeRead);
        queue_[enqueCounter & capacityMask_] = std::move(packet);
        ++enqueCounter_;
        if (queueSpace > 1)
            readPacketContract_.schedule(); // re-schedule to read more packets
        processPacketContract_.schedule(); // schedule process packet contract
    }
}


//=============================================================================
inline void word_stream::process_next_packet
(
    // pop next packet from queue and scan it for number of words
)
{
    auto enqueCounter = enqueCounter_.load();
    auto dequeCounter = dequeCounter_.load();
    auto packetCount = (enqueCounter - dequeCounter);
    if (packetCount > 0)
    {
        auto packet = std::move(queue_[dequeCounter & capacityMask_]);
        ++dequeCounter_ ;
        for (auto c : packet)
        {
            if ((prev_ == ' ') && (c != ' '))
                totalWords_++;
            prev_ = c;
        }
        if ((bytesProcessed_ += packet.size()) >= streamSize_)
        {
            readPacketContract_.release();
            std::unique_lock uniqueLock(mutex_);
            conditionVariable_.notify_all(); 
        }
        if (packetCount > 1)
            processPacketContract_.schedule(); // more packets to process
        readPacketContract_.schedule();// re-schedule to read more packets
    }
}
