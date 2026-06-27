#include <benchmark/benchmark.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "Sha256Hash.h"
#include "FileVerifier.h"

std::string g_testFilePath;

static bool evictFileCache(const std::string& filePath) {
    int fileDescriptor = ::open(filePath.c_str(), O_RDONLY);
    if (fileDescriptor == -1) {
        std::cerr << "Error: Could not open file for cache eviction: "
            << std::strerror(errno) << "\n";
        return false;
    }

    int result = ::posix_fadvise(fileDescriptor, 0, 0, POSIX_FADV_DONTNEED);
    ::close(fileDescriptor);

    if (result != 0) {
        std::cerr << "Error: Could not evict file cache: "
            << std::strerror(result) << "\n";
        return false;
    }

    return true;
}

// 1. Raw CPU Hash Throughput (Memory only)
static void BM_Sha256Hash_Memory(benchmark::State& state) {
    size_t dataSize = 16 * 1024 * 1024; // 16MB buffer to dilute loop overhead
    std::vector<char> buffer(dataSize, 'x');
    Sha256Hash hasher;

    for (auto _ : state) {
        hasher.reset();
        hasher.update(buffer.data(), buffer.size());
        benchmark::DoNotOptimize(hasher.finalize());
    }
    state.SetBytesProcessed(state.iterations() * dataSize);
}
BENCHMARK(BM_Sha256Hash_Memory)->Unit(benchmark::kMillisecond);

// 2. File Read Throughput (run independently for cache-sensitive measurements)
static void BM_FileRead_Only(benchmark::State& state) {
    const size_t bufferSize = 1024 * 1024;
    std::vector<char> buffer(bufferSize);
    std::streamsize totalBytesRead = 0;

    for (auto _ : state) {
        std::ifstream file(g_testFilePath, std::ios::binary);
        if (!file) {
            state.SkipWithError("File not found or cannot be opened");
            return;
        }

        while (file) {
            file.read(buffer.data(), buffer.size());
            totalBytesRead += file.gcount();
        }

        if (file.bad()) {
            state.SkipWithError("Fatal I/O error while reading file");
            return;
        }
    }

    state.SetBytesProcessed(totalBytesRead);
}
BENCHMARK(BM_FileRead_Only)->Unit(benchmark::kMillisecond)->UseRealTime()->Iterations(1);

// 3. File Read + SHA256 Hashing (Real World Scenario)
static void BM_Sha256Hash_Disk(benchmark::State& state) {
    std::ifstream check_file(g_testFilePath, std::ios::binary | std::ios::ate);
    if (!check_file) {
        state.SkipWithError("ISO file not found or cannot be opened");
        return;
    }
    std::size_t fileSize = check_file.tellg();
    check_file.close();

    for (auto _ : state) {
        FileVerifier verifier;
        auto result = verifier.calculateSha256(g_testFilePath, nullptr);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * fileSize);
}
BENCHMARK(BM_Sha256Hash_Disk)->Unit(benchmark::kMillisecond)->UseRealTime()->Iterations(1);

int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);

    bool shouldEvictFileCache = false;
    for (int index = 1; index < argc; ++index) {
        std::string argument = argv[index];
        if (argument == "--evict-file-cache") {
            shouldEvictFileCache = true;
        }
        else if (g_testFilePath.empty()) {
            g_testFilePath = argument;
        }
        else {
            std::cerr << "Error: Unexpected argument: " << argument << "\n";
            return 1;
        }
    }

    if (g_testFilePath.empty()) {
        std::cerr << "Error: Test file path is required.\n";
        std::cerr << "Usage: " << argv[0]
            << " [benchmark_options] [--evict-file-cache] <file_path>\n";
        return 1;
    }

    if (shouldEvictFileCache && !evictFileCache(g_testFilePath)) {
        return 1;
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
