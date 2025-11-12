#pragma once
#include <string>

// ZULU TIME
bool check_time_to_pause(int target_hour, int target_minute);

// WAYPOINT
bool is_valid_waypoint(std::string waypoint, std::string path);
void getCoordinates(std::string waypoint, float arr[2]);
float nm_to_km(int value);
float haversine(float lat1, float lon1, float lat2, float lon2);
float detect_collision(float diameter, float player_coodrs[2], float waypoint_coords[2]);
std::string clean_path(char plugin_path[512], char * filename);