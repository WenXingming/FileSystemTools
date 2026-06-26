#include "FileVerifier.h"
#include "Sha256Hash.h"
#include <fstream>
#include <vector>
#include <stdexcept>

VerifyResult FileVerifier::calculateSha256(const std::string& filePath, ProgressCallback onProgress) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open file " + filePath);
    }

    std::streamsize rawSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::int64_t fileSize = -1;
    if (rawSize != -1) {
        fileSize = static_cast<std::int64_t>(rawSize);
    }

    Sha256Hash hasher;
    const size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);
    std::streamsize totalBytesRead = 0;

    while (file) {
        file.read(buffer.data(), buffer.size());
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            hasher.update(buffer.data(), bytesRead);
            totalBytesRead += bytesRead;

            if (onProgress) {
                onProgress(totalBytesRead, fileSize);
            }
        }
    }

    VerifyResult result;
    result.hash = hasher.finalize();
    result.bytesRead = totalBytesRead;
    return result;
}
