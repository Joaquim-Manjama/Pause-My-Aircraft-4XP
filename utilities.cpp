#include "utilities.h"
#include <chrono>
#include <fstream>
#include <cmath>

// ZULU TIME
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

// WAYPOINT
void getCoordinates(std::string waypoint, float arr[2])
{
    // Create a text string, which is used to output the text file
    std::string text;
    std::string current_waypoint;
    float latitude, longitude;

    // Read from the text file
    std::ifstream MyReadFile("Files/Waypoints.txt");

    // Use a while loop together with the getline() function to read the file line by line
    while (std::getline(MyReadFile, text))
    {
        // Output the text from the file
        current_waypoint = text.substr(0, text.find(","));
        if (current_waypoint == waypoint)
        {
            latitude = std::stof(text.substr(text.find(",") + 1, text.find(",", text.find(",") + 1)));
            longitude = std::stof(text.substr(text.find(",", text.find(",") + 1) + 1));
            arr[0] = latitude;
            arr[1] = longitude;
        }
    }

    // Close the file
    MyReadFile.close();
}

bool is_valid_waypoint(std::string waypoint, std::string path)
{
    // Create a text string, which is used to output the text file
    std::string text;
    std::string current_waypoint;

    // Read from the text file
    std::ifstream MyReadFile(path);

    // Use a while loop together with the getline() function to read the file line by line
    while (std::getline(MyReadFile, text))
    {
        // Output the text from the file
        current_waypoint = text.substr(0, text.find(","));
        if (current_waypoint == waypoint)
        {
            return true;
        }
    }

    // Close the file
    MyReadFile.close();

    return false;
}

float nm_to_km(int value)
{
    return value * 1.852;
}

float haversine(float lat1, float lon1, float lat2, float lon2)
{
    const float R = 6371.0; // Radius of the Earth in kilometers
    const double PI = 2 * acos(0.0);
    float dLat = (lat2 - lat1) * PI / 180.0;
    float dLon = (lon2 - lon1) * PI / 180.0;

    lat1 = lat1 * PI / 180.0;
    lat2 = lat2 * PI / 180.0;

    float a = sin(dLat / 2) * sin(dLat / 2) +
        sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    float distance = R * c;

    return distance;
}

float detect_collision(float diameter, float player_coodrs[2], float waypoint_coords[2])
{
    float distance = haversine(player_coodrs[0], player_coodrs[1], waypoint_coords[0], waypoint_coords[1]);
    if (distance <= nm_to_km(int(diameter)))
    {
        return true;
    }
    return false;
}

std::string clean_path(char plugin_path[512], char * filename) {
    std::string basePath(plugin_path);
    size_t pos = basePath.find_last_of("\\/");
    if (pos != std::string::npos)
        basePath = basePath.substr(0, pos - 3);  // removes "\64\win.xpl"

    // Build full path to Waypoints.txt
    std::string file = basePath + "\\" + filename;

    return file;
}