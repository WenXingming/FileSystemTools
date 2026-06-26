#include <iostream>
#include <string>
#include <CLI/CLI.hpp>
#include "FileVerifier.h"
#include "ProgressReporter.h"

int main(int argc, char** argv) {
    CLI::App app{"RapidCheck: Ultra-Large File Fast Validator"};
    
    std::string filePath;
    app.add_option("file", filePath, "Path to the file to verify")
       ->required()
       ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    ProgressReporter reporter;
    FileVerifier verifier;

    try {
        VerifyResult result = verifier.calculateSha256(filePath, [&reporter](std::size_t cur, std::int64_t total) {
            reporter.update(cur, total);
        });
        
        reporter.finish(result.bytesRead);
        std::cout << result.hash << "  " << filePath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nHashing error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
