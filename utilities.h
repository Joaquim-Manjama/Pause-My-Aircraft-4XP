#pragma once
#include <string>

// ZULU TIME
bool check_time_to_pause(int target_hour, int target_minute);

// WAYPOINT
bool is_valid_waypoint(std::string waypoint, std::string path);
void get_current_utc_time(int& hour, int& minute);
void get_coordinates(std::string waypoint, float* arr, std::string path);
float nm_to_km(int value);
float km_to_nm(float value);
float get_distance_km(float player_coords[2], float waypoint_coords[2]);
float haversine(float lat1, float lon1, float lat2, float lon2);
bool detect_collision(int diameter, float player_coords[2], float waypoint_coords[2]);
std::string clean_path(char plugin_path[512], char * filename);