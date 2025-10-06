#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <iomanip>

class PerformanceTimer {
private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint m_start;
    std::string m_label;
    bool m_autoprint;

public:
    PerformanceTimer(const std::string& label = "", bool autoprint = true)
        : m_label(label), m_autoprint(autoprint) {
        start();
    }

    ~PerformanceTimer() {
        if (m_autoprint && !m_label.empty()) {
            printElapsed();
        }
    }

    void start() {
        m_start = Clock::now();
    }

    double elapsed() const {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        return duration.count() / 1000.0; // to milliseconds
    }

    void printElapsed() const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  " << m_label << ": " << elapsed() << " ms" << std::endl;
    }

    static void printHeader(const std::string& header) {
        std::cout << "\n=== " << header << " ===" << std::endl;
    }

    static void printSeparator() {
        std::cout << "----------------------------------------" << std::endl;
    }
};
