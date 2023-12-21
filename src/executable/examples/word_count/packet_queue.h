#pragma once


#include <library/system.h>
#include <include/spsc_fixed_queue.h>

#include <cstddef>
#include <vector>


//=============================================================================
class packet_queue
{
public:

    packet_queue
    (
        std::size_t capacity,
        bcpp::system::blocking_work_contract dequeContract
    ):
        queue_(capacity),
        dequeContract_(std::move(dequeContract))
    {
    }

    auto empty() const{return queue_.empty();}
    auto full() const{return (queue_.capacity() == queue_.size());}
    auto front() const{return queue_.front();}

    void push(std::vector<char> packet)
    {
        queue_.push(packet);
        dequeContract_.schedule(); 
    }

    auto pop()
    {
        std::vector<char> packet;
        auto sizePriorToPop = queue_.pop(packet);
        if (sizePriorToPop > 1)
            dequeContract_.schedule();
        return packet;
    }

private:

    bcpp::spsc_fixed_queue<std::vector<char>>   queue_;
    bcpp::system::blocking_work_contract        dequeContract_;
};
