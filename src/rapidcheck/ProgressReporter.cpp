#include "ProgressReporter.h"
#include <iostream>
#include <iomanip>

constexpr double MB = 1024.0 * 1024.0;

ProgressReporter::ProgressReporter() : started_(false) {
}

void ProgressReporter::update(std::size_t currentBytes, std::int64_t totalBytes) {
    if (!started_) {
        lastPrintTime_ = std::chrono::steady_clock::now();
        started_ = true;
    }

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastPrintTime_);
    if (duration.count() < 1) return;

    double mbRead = static_cast<double>(currentBytes) / MB;

    // Handle unknown size
    if (!(totalBytes) > 0) {
        std::cerr << "\r[ ---% ] " << std::fixed << std::setprecision(1) << mbRead << " MB / ??? MB" << std::flush;
    }

    double progress = (static_cast<double>(currentBytes) / totalBytes) * 100.0;
    double mbTotal = static_cast<double>(totalBytes) / MB;
    std::cerr << "\r[ " << std::setw(3) << static_cast<int>(progress) << "% ] "
        << std::fixed << std::setprecision(1) << mbRead << " MB / "
        << mbTotal << " MB" << std::flush;

    lastPrintTime_ = now;
}

void ProgressReporter::finish(std::size_t finalBytes) {
    double mbTotal = static_cast<double>(finalBytes) / MB;
    std::cerr << "\r[ 100% ] " << std::fixed << std::setprecision(1) << mbTotal << " MB / " << mbTotal << " MB" << std::endl;
}
