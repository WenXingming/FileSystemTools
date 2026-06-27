#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <CLI/CLI.hpp>
#include "FileVerifier.h"
#include "ProgressReporter.h"

int main(int argc, char** argv) {
    CLI::App app{ "RapidCheck: Ultra-Large File Fast Validator" };

    std::string filePath;
    app.add_option("file", filePath, "Path to the file to verify")
        ->required()
        ->check(CLI::ExistingFile);

    std::string expectedHash;
    app.add_option("--expect", expectedHash, "Expected SHA-256 hash to verify against");

    CLI11_PARSE(app, argc, argv);

    // Normalize expected hash to lowercase
    if (!expectedHash.empty()) {
        std::transform(expectedHash.begin(), expectedHash.end(), expectedHash.begin(),
            [](unsigned char c) { return std::tolower(c); });
    }

    ProgressReporter reporter;
    FileVerifier verifier;

    try {
        VerifyResult result = verifier.calculateSha256(filePath, [&reporter](std::size_t cur, std::int64_t total) {
            reporter.update(cur, total);
            });

        reporter.finish(result.bytesRead);

        if (!expectedHash.empty()) {
            if (result.hash == expectedHash) {
                std::cout << "\033[32m[ OK ]\033[0m " << result.hash << "  " << filePath << std::endl;
            }
            else {
                std::cerr << "\033[31m[ MISMATCH ]\033[0m\n"
                    << "  Expected: " << expectedHash << "\n"
                    << "  Actual:   " << result.hash << "\n"
                    << "  File:     " << filePath << std::endl;
                return 1;
            }
        }
        else {
            std::cout << result.hash << "  " << filePath << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "\nHashing error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
