#include <cstddef>
#include <iostream>
#include <chrono>
#include <map>
#include <functional>
#include <thread>
#include <cmath>

#include <include/spsc_fixed_queue.h>
#include <library/system.h>
#include <include/non_copyable.h>
#include <include/non_movable.h>


using namespace bcpp;
using namespace bcpp::system;


//=============================================================================
// a mock consumer connection that allows the producer to send integers to the mock consumer
class connection : non_copyable, non_movable
{
public:

    connection
    (
        blocking_work_contract_group & workContractGroup, 
        std::function<void(connection &)> selectedHandler
    ):
        queue_(1 << 10),
        workContract_(workContractGroup.create_contract([this, selectedHandler](){selectedHandler(*this);}))
    {
    }

    void send(int value){queue_.push(value); workContract_.schedule();}

    int receive()
    {
        int value = queue_.pop();
        if (!queue_.empty()) 
            workContract_.schedule();
        return value;
    }
private:

    spsc_fixed_queue<int> queue_;
    blocking_work_contract workContract_;
};


//=============================================================================
// mock consumer that accepts connections (really just queues of integers for demo purposes)
class consumer : non_copyable, non_movable
{
public:
    consumer(auto maxConnections):workContractGroup_(maxConnections){}
    auto connect(){return std::make_unique<connection>(workContractGroup_, 
            [this](auto & connection){auto message = connection.receive(); process_message(message);});}
    void receive(std::chrono::nanoseconds maxWait){workContractGroup_.execute_next_contract(maxWait);}
    std::uint64_t get_message_count() const{return messageCount_;}
private:
    void process_message(int value)
    {
        for (auto i = 2; i < value; ++i)
            if ((value % i) == 0)
                return;
        messageCount_++;
    };
    blocking_work_contract_group workContractGroup_;
    std::atomic<std::uint64_t> messageCount_;
};


//=============================================================================
// mock producer
class producer
{
public:
    producer(std::unique_ptr<connection> connection):connection_(std::move(connection)){}
    void send(int message){connection_->send(message);}
private:
    std::unique_ptr<connection> connection_;
};


//=============================================================================
int main()
{
    using namespace std::chrono;

    static auto constexpr num_worker_threads = 1;
    static auto constexpr max_producers = 256;
    static auto constexpr max_message_value = 100000;

    consumer myConsumer(max_producers);

    std::vector<std::jthread> workerThreads(num_worker_threads);
    for (auto & workerThread : workerThreads)
        workerThread = std::move(std::jthread([&](auto const & stopToken){while (!stopToken.stop_requested()) myConsumer.receive(10us);}));

    // make a bunch of connections
    std::vector<producer> producers;
    for (auto i = 0; i < max_producers; ++i)
        producers.emplace_back(myConsumer.connect());

    // send a bunch of messages
    std::jthread senderThread([&](auto const & stopToken)
            {
                std::uint64_t message = 2;
                while (message < max_message_value) 
                {
                    auto producerId = std::abs(rand()) % producers.size();
                    producers[producerId].send(message++);
                }
            });

    senderThread.join();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (auto & workerThread : workerThreads)
        workerThread.request_stop();
    for (auto & workerThread : workerThreads)
        workerThread.join();

    auto messageCount = myConsumer.get_message_count();
    std::cout << "recieved " << messageCount << " messages\n";

    return 0;
}
