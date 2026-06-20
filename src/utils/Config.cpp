#include "utils/Config.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace GeoIQ {

    // Helper to trim leading and trailing spaces from config lines
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    // Helper to parse bool from string values
    static bool parseBool(const std::string& str) {
        std::string s = trim(str);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        return (s == "true" || s == "1" || s == "yes" || s == "on");
    }

    bool Config::load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::getInstance().warn("Config", "Configuration file '" + filepath + "' not found. Using defaults.");
            return true; // Fallback to default constants
        }

        std::string line;
        int line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            std::string trimmed = trim(line);
            
            // Skip comments and empty lines
            if (trimmed.empty() || trimmed.front() == '#') {
                continue;
            }

            size_t eq_pos = trimmed.find('=');
            if (eq_pos == std::string::npos) {
                Logger::getInstance().warn("Config", "Invalid configuration line " + std::to_string(line_num) + " in " + filepath);
                continue;
            }

            std::string key = trim(trimmed.substr(0, eq_pos));
            std::string val = trim(trimmed.substr(eq_pos + 1));

            try {
                if (key == "input_file") {
                    input_file = val;
                } else if (key == "output_dir") {
                    output_dir = val;
                } else if (key == "local_radius_km") {
                    local_radius_km = std::stod(val);
                } else if (key == "regional_radius_km") {
                    regional_radius_km = std::stod(val);
                } else if (key == "idw_power") {
                    idw_power = std::stod(val);
                } else if (key == "grid_cell_size_km") {
                    grid_cell_size_km = std::stod(val);
                } else if (key == "min_neighbours") {
                    min_neighbours = std::stoi(val);
                } else if (key == "target_neighbours") {
                    target_neighbours = std::stoi(val);
                } else if (key == "enable_arsenic") {
                    enable_arsenic = parseBool(val);
                } else if (key == "enable_gold") {
                    enable_gold = parseBool(val);
                } else if (key == "enable_diamond") {
                    enable_diamond = parseBool(val);
                } else if (key == "enable_copper") {
                    enable_copper = parseBool(val);
                } else if (key == "enable_lithium") {
                    enable_lithium = parseBool(val);
                } else if (key == "enable_ree") {
                    enable_ree = parseBool(val);
                } else if (key == "enable_zinc_lead") {
                    enable_zinc_lead = parseBool(val);
                } else if (key == "enable_gossan") {
                    enable_gossan = parseBool(val);
                } else if (key == "enable_grid") {
                    enable_grid = parseBool(val);
                } else if (key == "grid_boundary_padding_km") {
                    grid_boundary_padding_km = std::stod(val);
                } else if (key == "log_level") {
                    log_level = val;
                } else if (key == "is_geology") {
                    is_geology = parseBool(val);
                } else {
                    Logger::getInstance().warn("Config", "Unknown configuration key '" + key + "' on line " + std::to_string(line_num));
                }
            } catch (const std::exception& e) {
                Logger::getInstance().warn("Config", "Error parsing value for '" + key + "' on line " + std::to_string(line_num) + ": " + e.what() + ". Using default.");
            }
        }

        Logger::getInstance().info("Config", "Configuration loaded successfully from '" + filepath + "'.");
        return true;
    }
}
