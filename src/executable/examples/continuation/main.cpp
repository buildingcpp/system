#include <include/spsc_fixed_queue.h>
#include <library/system.h>

#include <cstddef>
#include <iostream>
#include <chrono>
#include <tuple>
#include <type_traits>
#include <concepts>
#include <utility>
#include <functional>


template <typename R, typename ... Ts>
class task_queue;


//=============================================================================
template <typename ... Ts>
class task_queue<void, Ts ...> :
    bcpp::non_movable,
    bcpp::non_copyable
{
private:

    using result_type = void;
    using type = std::tuple<Ts ...>;
    using queue_type = bcpp::spsc_fixed_queue<type>;

public:

    task_queue
    (
        bcpp::system::work_contract_tree & workContractGroup,
        std::invocable<Ts ...> auto && work
    ):
        queue_(1024),
        workContract_(workContractGroup.create_contract(
                [this, work = std::forward<decltype(work)>(work)]
                (
                    auto & token
                )
                {
                    this->on_invoked(token, work, std::make_index_sequence<sizeof ... (Ts)>());
                }))
    {
    }

    void enqueue
    (
        Ts && ... args
    )
    {
        queue_.push(std::make_tuple(std::forward<Ts>(args) ...));
        workContract_.schedule();
    }

    bool empty() const{return queue_.empty();}

private:

    template <std::size_t ... N>
    void on_invoked
    (
        auto & token,
        auto & work,
        std::index_sequence<N ...>
    )
    {
        type value;
        auto remaining = queue_.pop(value) - 1;
        work(std::get<N>(value) ...);
        if (remaining > 0)
            token.schedule();
    }

    queue_type                  queue_;

    bcpp::system::work_contract workContract_;
};



//=============================================================================
template <typename R, typename ... Ts>
requires (!std::is_same_v<R, void>)
class task_queue<R, Ts ...> :
    bcpp::non_movable,
    bcpp::non_copyable
{
private:

    using result_type = R;
    using type = std::tuple<Ts ...>;
    using queue_type = bcpp::spsc_fixed_queue<type>;

public:

    task_queue
    (
        bcpp::system::work_contract_tree & workContractGroup,
        std::invocable<Ts ...> auto && work
    ):
        queue_(1024),
        workContract_(workContractGroup.create_contract(
                [this, work = std::forward<decltype(work)>(work)]
                (
                    auto & token
                )
                {
                    this->on_invoked(token, work, std::make_index_sequence<sizeof ... (Ts)>());
                }))
    {
    }

    void enqueue
    (
        Ts && ... args
    )
    {
        queue_.push(std::make_tuple(std::forward<Ts>(args) ...));
        workContract_.schedule();
    }

    bool empty() const{return queue_.empty();}

    template <typename R_>
    auto & then
    (
        bcpp::system::work_contract_tree & workContractTree,
        std::invocable<result_type> auto && work
    )
    {
        auto taskQueue = std::make_shared<task_queue<R_, result_type>>(workContractTree, work);
        then_ = [taskQueue](auto && value){taskQueue->enqueue(std::forward<decltype(value)>(value));};
        return *taskQueue;
    }

private:

    template <std::size_t ... N>
    void on_invoked
    (
        auto & token,
        auto & work,
        std::index_sequence<N ...>
    )
    {
        type value;
        auto remaining = queue_.pop(value) - 1;
        if  constexpr (std::is_same_v<result_type, void>)
        {
            work(std::get<N>(value) ...);
        }
        else
        {
            auto result = work(std::get<N>(value) ...);
            if (then_)
                then_(result);
        }
        if (remaining > 0)
            token.schedule();
    }

    queue_type                  queue_;

    std::function<void(result_type)> then_;

    bcpp::system::work_contract workContract_;
};


//=============================================================================
template <typename R, typename ... Ts>
class task :
    bcpp::non_copyable
{
private:

    using result_type = R;
    using queue_type = task_queue<result_type, Ts ...>;

public:

    task
    (
        bcpp::system::work_contract_tree & workContractGroup,
        std::invocable<Ts ...> auto && work
    ):
        queue_(std::make_shared<queue_type>(workContractGroup, std::forward<decltype(work)>(work)))
    {
    }

    void enqueue
    (
        Ts && ... args
    )
    {
        queue_->enqueue(std::forward<Ts>(args) ...);
    }

    bool empty() const{return queue_->empty();}

    auto & then
    (
        bcpp::system::work_contract_tree & workContractTree,
        std::invocable<result_type> auto && work
    )
    {
        using R_ = std::invoke_result_t<std::decay_t<decltype(work)>, result_type>;
        return (queue_->template then<R_>(workContractTree, std::forward<std::decay_t<decltype(work)>>(work)));
    }

private:

    std::shared_ptr<queue_type> queue_;

};


//=============================================================================
template <typename ... Ts>
auto make_task
(
    bcpp::system::work_contract_tree & workContractTree,
    std::invocable<Ts ...> auto && work
)
{
    using result_type = std::invoke_result_t<std::decay_t<decltype(work)>, Ts ...>;
    return task<result_type, Ts ...>(workContractTree, std::forward<std::decay_t<decltype(work)>>(work));
}


//=============================================================================
int main()
{
    // create a non blocking work contract tree
    bcpp::system::work_contract_tree workContractTree(256);

    auto mult = make_task<int, int>(workContractTree, [](int a, int b){return a * b;});
    mult.then(workContractTree, [](int product){std::cout << "the product = " << product << "\n"; return product / 2;});
  //  div.then([](int value){std::cout << "which when divided by 2 = " << value << "\n";});
    mult.enqueue(5, 6);
    mult.enqueue(5, 7);
    mult.enqueue(5, 8);
    mult.enqueue(5, 9);

    // create async thread to process the work contract
    std::jthread workerThread([&]
            (
                std::stop_token const & stopToken
            )
            {
                while (!stopToken.stop_requested()) 
                    workContractTree.execute_next_contract();
            });

    while (!mult.empty())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return 0;
}
