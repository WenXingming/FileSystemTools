#include "Hasher.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

#include "Blake3Hash.h"

Hasher::Hasher(size_t bufferSize)
    : bufferSize_(bufferSize == 0 ? 1 : bufferSize) {
}

// NOTE: Parallel Called
HashResult Hasher::hash_file(const FileInfo& file) const {
    // 1. 以二进制模式打开文件 (非常重要，防止 Windows 下的 \r\n 转换破坏文件数据)
    std::ifstream input(file.path.c_str(), std::ios::binary);
    if (!input.is_open()) {
        return HashResult{ false, file, Blake3Value{}, FileError{ FileError::Phase::HASH, file.path, "failed to open file: " + std::string(std::strerror(errno)) } };
    }

    // 2. 分配读取缓冲区，分块循环读取 (Chunked Reading)
    // 内存友好（OOM 防范）：采用了基于固定大小 Buffer 的分块读取策略。决不会因为大文件撑爆内存。
    Blake3Hash hash;
    // 关键优化：使用 unique_ptr<char[]> 避免 std::vector 的 zero-initialization 开销。
    // 为什么不把它变成类的成员变量来复用内存？
    // 答：因为 hash_file() 是被 ThreadPool 并行调用的（const 方法）。
    // 每个 worker 线程必须拥有自己独立的 buffer，以保证彻底的线程安全（Thread-safety）。
    std::unique_ptr<char[]> buffer(new char[bufferSize_]);
    while (input) {
        input.read(buffer.get(), static_cast<std::streamsize>(bufferSize_));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0) {
            hash.update(buffer.get(), static_cast<size_t>(bytesRead)); // 将读到的数据块不断喂给哈希算法，更新 hash 状态
        }
    }

    // 3. 严谨的错误校验
    // 循环退出时，正常情况必须是因为遇到了文件尾 (EOF)，如果不是 EOF 导致的退出，说明发生了 I/O 错误（如磁盘掉线）
    if (!input.eof()) {
        return HashResult{ false, file, Blake3Value{}, FileError{ FileError::Phase::HASH, file.path, "failed to read file: " + std::string(std::strerror(errno)) } };
    }

    return HashResult{ true, file, hash.value(), FileError{} };
}

HashResult Hasher::hash_partial_file(const FileInfo& file, size_t maxBytes) const {
    std::ifstream input(file.path.c_str(), std::ios::binary);
    if (!input.is_open()) {
        return HashResult{ false, file, Blake3Value{}, FileError{ FileError::Phase::HASH, file.path, "failed to open file: " + std::string(std::strerror(errno)) } };
    }

    Blake3Hash hash;
    std::unique_ptr<char[]> buffer(new char[bufferSize_]);
    
    if (file.size <= maxBytes) {
        // 场景 A：文件不够大，从头读到尾（作为局部哈希）
        size_t totalRead = 0;
        while (input && totalRead < maxBytes) {
            size_t toRead = std::min(bufferSize_, maxBytes - totalRead);
            input.read(buffer.get(), static_cast<std::streamsize>(toRead));
            const std::streamsize bytesRead = input.gcount();
            if (bytesRead > 0) {
                hash.update(buffer.get(), static_cast<size_t>(bytesRead));
                totalRead += bytesRead;
            }
        }
    } else {
        // 场景 B：文件足够大，执行“首尾混合（Head+Tail）”抽样哈希
        const size_t chunkSize = maxBytes / 2;

        // 1. 读取头部块
        size_t headRead = 0;
        while (input && headRead < chunkSize) {
            size_t toRead = std::min(bufferSize_, chunkSize - headRead);
            input.read(buffer.get(), static_cast<std::streamsize>(toRead));
            const std::streamsize bytesRead = input.gcount();
            if (bytesRead > 0) {
                hash.update(buffer.get(), static_cast<size_t>(bytesRead));
                headRead += bytesRead;
            }
        }

        if (input.bad()) {
            return HashResult{ false, file, Blake3Value{}, FileError{ FileError::Phase::HASH, file.path, "failed to read file head: " + std::string(std::strerror(errno)) } };
        }

        // 2. 清除 EOF 等状态，精准跳转到尾部块的起始点
        input.clear();
        input.seekg(file.size - chunkSize, std::ios::beg);

        // 3. 读取尾部块
        size_t tailRead = 0;
        while (input && tailRead < chunkSize) {
            size_t toRead = std::min(bufferSize_, chunkSize - tailRead);
            input.read(buffer.get(), static_cast<std::streamsize>(toRead));
            const std::streamsize bytesRead = input.gcount();
            if (bytesRead > 0) {
                hash.update(buffer.get(), static_cast<size_t>(bytesRead));
                tailRead += bytesRead;
            }
        }
    }

    // 检测是否有底层的严重读取错误 (例如磁盘损坏)
    if (input.bad()) {
        return HashResult{ false, file, Blake3Value{}, FileError{ FileError::Phase::HASH, file.path, "failed to read file: " + std::string(std::strerror(errno)) } };
    }

    return HashResult{ true, file, hash.value(), FileError{} };
}
