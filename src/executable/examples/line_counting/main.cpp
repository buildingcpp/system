#include <library/system.h>
#include <include/spsc_fixed_queue.h>

#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>


//=============================================================================
class sv_queue :
    bcpp::non_movable,
    bcpp::non_copyable
{
public:
    sv_queue(bcpp::system::blocking_work_contract_tree & workContractGroup): queue_(1 << 8), 
            workContract_(workContractGroup.create_contract([this](auto & token){this->consume(token);})){}
    void push(std::string_view const & value){while (!queue_.push(value)) true; workContract_.schedule();}
    bool empty() const{return queue_.empty();}
private:
    void consume(auto & token){std::string_view sv; if (queue_.pop(sv) > 1) token.schedule(); }
    bcpp::system::blocking_work_contract workContract_;
    bcpp::spsc_fixed_queue<std::string_view> queue_;
};


//=============================================================================
int main
(
    int argc,
    char ** args
)
{
    const auto path = std::string(args[1]);
    std::ifstream stream(path.c_str(), std::ios::binary | std::ios::in);
    std::string text(std::istreambuf_iterator<char>(stream), {});

    bcpp::system::blocking_work_contract_tree workContractGroup(256);
    sv_queue queue(workContractGroup);

    bcpp::system::thread_pool tp({{.function_ = [&](std::stop_token const & stopToken)
            {
                while (!stopToken.stop_requested())
                    workContractGroup.execute_next_contract();
                while (!queue.empty())
                    workContractGroup.execute_next_contract();
            }}});

    auto start = std::chrono::system_clock::now();

    std::string_view::size_type pos = 0;
    std::string_view::size_type prev = 0;
    while ((pos = text.find('\n', prev)) != std::string::npos) 
    {
        queue.push(text.substr(prev, pos - prev));
        prev = pos + 1;
    }
    queue.push(text.substr(prev));

    tp.stop();
    auto finish = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
    std::cout << "elapsed time = " << elapsed << " usec.\n";

    return 0;
}
