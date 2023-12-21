

#include "./word_stream.h"

#include <cstddef>
#include <iostream>
#include <atomic>
#include <chrono>
#include <filesystem>


//=============================================================================
int main
(
    int argc,
    char ** args
)
{
    if (argc != 2)
    {
        std::cerr << "usage: word_count path/to/files\n";
        return -1;
    }

    std::filesystem::path dirPath(args[1]);
    if (!std::filesystem::is_directory(dirPath))
    {
        std::cerr << dirPath << " is not a directory\n";
        return -1;
    }

    std::vector<std::filesystem::path> paths;
    for (const auto & path : std::filesystem::directory_iterator(dirPath))
    {
        if (std::filesystem::is_regular_file(path))
            paths.push_back(path);
    }

    auto numWorkerThreads = std::thread::hardware_concurrency();
    std::cout << "thread count: " << numWorkerThreads << "\n";
    std::cout << "processing " << paths.size() << " test files\n";

    // create a non blocking work contract group
    bcpp::system::blocking_work_contract_group workContractGroup((paths.size() * 2));

    // create the thread pool
    std::vector<bcpp::system::thread_pool::thread_configuration> threadPoolConfiguration;
    auto workFunction = [&](std::stop_token const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();};
    for (auto i = 0; i < numWorkerThreads; ++i)
        threadPoolConfiguration.push_back({.function_ = workFunction});
    bcpp::system::thread_pool threadPool(threadPoolConfiguration);

    // create word streams for each of the input streams
    std::vector<std::unique_ptr<word_stream>> wordStreams;
    for (auto path : paths)
        wordStreams.push_back(std::make_unique<word_stream>(path, 4096, workContractGroup));

    // start the test
    auto start = std::chrono::system_clock::now();

    // start each word stream
    for (auto & wordStream : wordStreams)
        wordStream->start();

    // wait for word stream to be completed
    for (auto & wordStream : wordStreams)
    {
        std::cout << wordStream->get_path() << " done\n";
        wordStream->join();
    }

    // end test and calculate results
    auto finish = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start);

    auto totalWordCount = 0ull;
    auto totalBytesProcessed = 0ull;
    for (auto const & wordStream : wordStreams)
    {
        totalWordCount += wordStream->word_count();
        totalBytesProcessed += wordStream->bytes_processed();
    }

    // display results
    std::cout << "word count = " << totalWordCount << "\n";
    std::cout << "total bytes processed = " << totalBytesProcessed << "\n";
    auto seconds = ((double)elapsed.count() / decltype(elapsed)::period::den);
    std::cout << "elapsed time: " << seconds << " seconds\n";
    std::cout << "number of files processed: " << paths.size() << "\n";
    auto rate = (((double)totalBytesProcessed / (1 << 20)) / seconds);
    std::cout << "rate: " << rate << " MB per second\n";

    return 0;
}
