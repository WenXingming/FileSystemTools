#ifndef RAPIDCHECK_FILE_VERIFIER_H
#define RAPIDCHECK_FILE_VERIFIER_H

#include <string>
#include <functional>
#include <cstddef>

struct VerifyResult {
    std::string hash;
    std::size_t bytesRead;
};

class FileVerifier {
public:
    using ProgressCallback = std::function<void(std::size_t currentBytes, std::int64_t totalBytes)>;

    VerifyResult calculateSha256(const std::string& filePath, ProgressCallback onProgress = nullptr);
};

#endif // RAPIDCHECK_FILE_VERIFIER_H
