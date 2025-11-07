#include "utilities.h"
#include <chrono>

bool check_time_to_pause(int target_hour, int target_minute)
{
    // Get current system time
    auto now = std::chrono::system_clock::now();

    // Convert to time_t
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    std::tm utc_tm{};
#if defined(_WIN32) || defined(_WIN64)
    // Windows version (thread-safe)
    gmtime_s(&utc_tm, &now_c);
#else
    // Linux/macOS version (thread-safe)
    gmtime_r(&now_c, &utc_tm);
#endif

    int hour = utc_tm.tm_hour;
    int minute = utc_tm.tm_min;

    return (target_hour == hour) && (target_minute == minute);
}