#pragma once

#include "./packet_queue.h"

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
        std::uint64_t,
        bcpp::system::blocking_work_contract_group &
    );

    void start();

    void join();

    std::uint64_t word_count() const;

    std::uint64_t bytes_processed() const;

    std::filesystem::path get_path() const;

private:

    void read_packet
    (
        std::uint64_t
    );

    void process_next_packet();

    void mark_complete();

    std::filesystem::path                   path_;
    packet_queue                            packetQueue_;
    std::ifstream                           inputStream_;
    bcpp::system::blocking_work_contract    readPacketContract_;
    std::atomic<std::uint64_t>              totalWords_{0};
    std::atomic<std::uint64_t>              bytesProcessed_{0};
    bool volatile                           eof_{false};
    bool volatile                           complete_{false};

    std::condition_variable                 conditionVariable_;
    std::mutex                              mutex_;

    char prev_{' '};
};


//=============================================================================
inline word_stream::word_stream
(
    std::filesystem::path path,
    std::uint64_t blockSize,
    bcpp::system::blocking_work_contract_group & workContractGroup
):
    path_(path),
    packetQueue_(32, workContractGroup.create_contract([this](){this->process_next_packet();})),
    readPacketContract_(workContractGroup.create_contract([this, blockSize](){this->read_packet(blockSize);}))
{
    complete_ = std::filesystem::is_empty(path);
    inputStream_ = std::ifstream(path, std::ios_base::in | std::ios_base::binary);
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
    conditionVariable_.wait(uniqueLock, [&](){return complete_;}); 
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
inline void word_stream::read_packet
(
    // load next packet from the input stream and queue it for asynchronous processing
    std::uint64_t blockSize
)
{
    if (!packetQueue_.full())
    {
        // load another packet and queue it for processing
        std::vector<char> packet(blockSize);
        inputStream_.read(packet.data(), blockSize);
        packet.resize(inputStream_.gcount());
        packetQueue_.push(std::move(packet));

        if (inputStream_.eof())
        {
            // if the entire file has been read then end this task
            eof_ = true;
            readPacketContract_.release();
        }
        // re-schedule to read more packets
        readPacketContract_.schedule();
    } 
}


//=============================================================================
inline void word_stream::process_next_packet
(
    // pop next packet from queue and scan it for number of words
)
{
    for (auto c : packetQueue_.front())
    {
        if ((prev_ == ' ') && (c != ' '))
            totalWords_++;
        prev_ = c;
    }
    bytesProcessed_ += packetQueue_.front().size();
    packetQueue_.pop();
    if ((eof_) && (packetQueue_.empty()))
        mark_complete();
    // reschedule the read-packet contract just in case it wasn't scheduled due to a full queue
    readPacketContract_.schedule();
}


//=============================================================================
inline void word_stream::mark_complete
(
    // stream has been completely processed.  
    // notify any waiting threads
)
{
    std::unique_lock uniqueLock(mutex_);
    complete_ = true;
    conditionVariable_.notify_all(); 
}
