#include <cstddef>
#include <iostream>
#include <atomic>
#include <chrono>
#include <map>
#include <functional>
#include <thread>
#include <cmath>

#include <include/spsc_fixed_queue.h>
#include <library/system.h>
#include <include/non_copyable.h>
#include <include/non_movable.h>



//=============================================================================
// a mock connection that allows the client to send integers to the mock server
class connection
{
public:

    using message_type = int;
    static auto constexpr receive_capacity = (1ull << 4);

    connection
    (
        bcpp::system::blocking_work_contract workContract
    ):
        workContract_(std::move(workContract)), 
        queue_(receive_capacity)
    {
    }


    bool send
    (
        message_type message
    )
    {
        if (queue_.push(message))
        { 
            workContract_.schedule();
            return true;
        }
        return false;
    }


    message_type receive
    (
    )
    {
        message_type message;
        if (auto sizePriorToPop = queue_.pop(message); sizePriorToPop > 1)
            workContract_.schedule(); // more in the queue
        return message;
    }

private:
    bcpp::spsc_fixed_queue<message_type>    queue_;
    bcpp::system::blocking_work_contract    workContract_;
};


//=============================================================================
// mock server that accepts connections (really just queues of integers for demo purposes)
class server :
    bcpp::non_copyable,
    bcpp::non_movable
{
public:

    server():workContractGroup_(1024){}

    auto connect
    (
        // create a new connection.  set it up with a work contract to invoke async receive whenever the connection has data in it
    )
    {
        int streamId = nextStreamId_++;
        auto workContract = workContractGroup_.create_contract([this, streamId](){this->receive(streamId);});
        auto c = std::make_shared<connection>(std::move(workContract));
        std::lock_guard lockGuard(mutex_);
        return streams_[streamId] = c;
    }

    void poll_connections(){workContractGroup_.execute_next_contract(std::chrono::milliseconds(10));}

private:

    void receive
    (
        // this is the async work that the work contract executes when it is scheduled
        int streamId
    )
    {
        if (auto iter = streams_.find(streamId); iter != streams_.end())
        {
            static std::mutex m; // this lock is only here to sync output to terminal. not required for anything else.
            std::lock_guard lg(m);
            std::cout << "consumed " << iter->second->receive() << " from connection " << streamId << std::endl;
        }
    }

    std::mutex mutable mutex_;
    std::atomic<int> nextStreamId_;    
    bcpp::system::blocking_work_contract_group workContractGroup_;
    std::map<int, std::shared_ptr<connection>> streams_;
};


//=============================================================================
int main()
{
    static auto constexpr num_worker_threads = 8;
    static auto constexpr max_connections = 256;

    // construct a server
    server myServer;

    // create a pool of worker threads to poll the server for connections
    std::vector<std::jthread> workerThreads(num_worker_threads);
    for (auto & workerThread : workerThreads)
        workerThread = std::move(std::jthread([&](auto const & stopToken){while (!stopToken.stop_requested()) myServer.poll_connections();}));

    // make a bunch of connections
    std::vector<std::shared_ptr<connection>> connections(max_connections);
    for (auto & connection : connections)
        connection = myServer.connect();

    // send a bunch of messages
    for (auto i = 0; i < 1024; ++i)
        for (auto & connection : connections)
            while (!connection->send(i))
                ;

    // give async work a chance to finish up
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
