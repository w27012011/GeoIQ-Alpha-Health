#include "engine/GridGenerator.h"
#include "spatial/IDW.h"
#include "utils/Logger.h"
#include "utils/Constants.h"
#include <cmath>
#include <sstream>

namespace GeoIQ {

    std::vector<SamplePoint> GridGenerator::generate(const Dataset& dataset) const {
        if (dataset.size() == 0) {
            return {};
        }

        double min_lat = dataset.minLat();
        double max_lat = dataset.maxLat();
        double min_lon = dataset.minLon();
        double max_lon = dataset.maxLon();

        double center_lat = (min_lat + max_lat) / 2.0;
        double center_lat_rad = center_lat * PI / 180.0;

        double cos_lat = std::cos(center_lat_rad);
        if (std::abs(cos_lat) < 1e-6) {
            cos_lat = 1e-6; // Safety guard to avoid division by zero near poles
        }

        double lat_pad_deg = boundary_padding_km / 111.0;
        double lon_pad_deg = boundary_padding_km / (111.0 * cos_lat);

        double padded_min_lat = min_lat - lat_pad_deg;
        double padded_max_lat = max_lat + lat_pad_deg;
        double padded_min_lon = min_lon - lon_pad_deg;
        double padded_max_lon = max_lon + lon_pad_deg;

        double step_lat = cell_size_km / 111.0;
        double step_lon = cell_size_km / (111.0 * cos_lat);

        int n_lat = static_cast<int>((padded_max_lat - padded_min_lat) / step_lat) + 1;
        int n_lon = static_cast<int>((padded_max_lon - padded_min_lon) / step_lon) + 1;

        std::vector<SamplePoint> grid_points;
        if (n_lat > 0 && n_lon > 0) {
            grid_points.reserve(n_lat * n_lon);
        }

        for (double lat = padded_min_lat; lat <= padded_max_lat; lat += step_lat) {
            for (double lon = padded_min_lon; lon <= padded_max_lon; lon += step_lon) {
                std::vector<const SamplePoint*> neighbours = dataset.queryRadius(lat, lon, idw_search_radius_km);
                if (neighbours.empty()) {
                    continue;
                }

                SamplePoint gp = IDW::interpolateAllElements(lat, lon, neighbours, idw_power);
                Dataset::applyDerivedRatios(gp);
                gp.sample_type = SampleType::VIRTUAL_GRID;

                grid_points.push_back(std::move(gp));
            }
        }

        std::stringstream ss;
        ss << "Generated " << grid_points.size() << " grid cells (" << n_lat << "x" << n_lon << ") with size " << cell_size_km << "km";
        Logger::getInstance().info("GridGenerator", ss.str());

        return grid_points;
    }
}
