#ifndef GEOIQ_CONFIG_H
#define GEOIQ_CONFIG_H

#include <string>

namespace GeoIQ {

    class Config {
    public:
        // Input and output file paths
        std::string input_file            = "data/raw/samples.csv";
        std::string output_dir            = "output/";

        // Spatial analysis window properties
        double      local_radius_km       = 10.0;
        double      regional_radius_km    = 100.0;
        double      idw_power             = 2.0;
        double      grid_cell_size_km     = 1.0;

        // Statistics neighborhood counts
        int         min_neighbours        = 3;
        int         target_neighbours     = 8;

        // Target rule switches
        bool        enable_arsenic        = true;
        bool        enable_gold           = true;
        bool        enable_diamond        = true;
        bool        enable_copper         = true;
        bool        enable_lithium        = true;
        bool        enable_ree            = true;
        bool        enable_zinc_lead      = true;
        bool        enable_gossan         = true;

        // Bounding grid interpolation switches
        bool        enable_grid           = true;
        double      grid_boundary_padding_km = 5.0;

        // Log outputs threshold settings ("DEBUG", "INFO", "WARNING", "ERROR")
        std::string log_level             = "INFO";

        // Mode switch: true = geology/exploration, false = health/hazards
        bool        is_geology            = true;

        Config() = default;

        // Loads values from file. If file does not exist, uses defaults.
        bool load(const std::string& filepath);
    };
}

#endif // GEOIQ_CONFIG_H
