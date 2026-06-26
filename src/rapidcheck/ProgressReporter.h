#ifndef RAPIDCHECK_PROGRESS_REPORTER_H
#define RAPIDCHECK_PROGRESS_REPORTER_H

#include <cstddef>
#include <chrono>

class ProgressReporter {
public:
    ProgressReporter();
    void update(std::size_t currentBytes, std::int64_t totalBytes);
    void finish(std::size_t finalBytes);

private:
    std::chrono::steady_clock::time_point lastPrintTime_;
    bool started_;
};

#endif // RAPIDCHECK_PROGRESS_REPORTER_H
