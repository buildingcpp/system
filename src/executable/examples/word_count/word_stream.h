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

    void start();

    void join();

    std::uint64_t word_count() const;

    std::uint64_t bytes_processed() const;

    std::filesystem::path get_path() const;

private:

    void enqueue_packet
    (
        std::uint32_t
    );

    void process_next_packet();

    void mark_complete();

    std::filesystem::path                   path_;
    std::ifstream                           inputStream_;
    bcpp::system::blocking_work_contract    readPacketContract_;
    bcpp::system::blocking_work_contract    processPacketContract_;
    std::atomic<std::uint64_t>              totalWords_{0};
    std::atomic<std::uint64_t>              bytesProcessed_{0};
    std::atomic<std::uint64_t>              streamSize_{0};

    std::condition_variable                 conditionVariable_;
    std::mutex                              mutex_;

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
inline void word_stream::start
(
    // trigger start of processing the stream by 
    // scheduling the read packet contract for the first time
)
{
    readPacketContract_.schedule();
}


//=============================================================================
inline void word_stream::join
(
    // wait for the stream to be completely processed before returning
)
{
    std::unique_lock uniqueLock(mutex_);
    conditionVariable_.wait(uniqueLock, [&](){return (bytesProcessed_ >= streamSize_);}); 
}


//=============================================================================
inline std::uint64_t word_stream::word_count
(
    // return total number of words found in the input stream
) const
{
    return totalWords_;
}


//=============================================================================
inline std::uint64_t word_stream::bytes_processed
(
    // return the total number of bytes processed from the input stream
) const
{
    return bytesProcessed_;
}


//=============================================================================
inline std::filesystem::path word_stream::get_path
(
) const
{
    return path_;
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
            mark_complete();
        if (packetCount > 1)
            processPacketContract_.schedule(); // more packets to process
        readPacketContract_.schedule();// re-schedule to read more packets
    }
}


//=============================================================================
inline void word_stream::mark_complete
(
    // stream has been completely processed.  
    // notify any waiting threads
)
{
    readPacketContract_.release();
    std::unique_lock uniqueLock(mutex_);
    conditionVariable_.notify_all(); 
}
