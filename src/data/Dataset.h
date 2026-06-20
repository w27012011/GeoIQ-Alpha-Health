#ifndef GEOIQ_DATASET_H
#define GEOIQ_DATASET_H

#include <vector>
#include "data/SamplePoint.h"
#include "spatial/KDTree.h"

namespace GeoIQ {

    class Dataset {
    public:
        // Constructor: takes ownership of loaded points, builds KDTree, computes ratios
        explicit Dataset(std::vector<SamplePoint> points);

        // Disallow copying to avoid heavy memory allocation (TRAP-01)
        Dataset(const Dataset&) = delete;
        Dataset& operator=(const Dataset&) = delete;

        // Allow moving
        Dataset(Dataset&&) noexcept = default;
        Dataset& operator=(Dataset&&) noexcept = default;

        // Radius query: returns pointers to all SamplePoints within radius_km of (lat, lon)
        // Results are sorted by distance ascending
        std::vector<const SamplePoint*> queryRadius(
            double lat, double lon, double radius_km) const;

        // Radius query with adaptive expansion:
        // If fewer than min_count points found at radius_km,
        // expand radius up to MAX_RADIUS_EXPAND_FACTOR × radius_km until min_count found.
        // Returns the radius actually used via radius_used_out.
        // Logs a WARNING if radius was expanded.
        std::vector<const SamplePoint*> queryRadiusAdaptive(
            double lat, double lon,
            double radius_km,
            int    min_count,
            double& radius_used_out) const;

        // K-nearest neighbours (regardless of radius)
        std::vector<const SamplePoint*> queryKNearest(
            double lat, double lon, int k) const;

        // Accessors
        const std::vector<SamplePoint>& getPoints() const;
        int size() const;
        int getExpandedRadiusCount() const;

        // Bounding box of all loaded points
        double minLat() const;
        double maxLat() const;
        double minLon() const;
        double maxLon() const;

        // Computes and stamps all derived ratios onto a SamplePoint
        static void applyDerivedRatios(SamplePoint& p);

    private:
        std::vector<SamplePoint> points_;
        KDTree                   kdtree_;

        void computeAllRatios();     // calls applyDerivedRatios() for each point in points_
        void buildBoundingBox();

        double min_lat_ = 90.0;
        double max_lat_ = -90.0;
        double min_lon_ = 180.0;
        double max_lon_ = -180.0;
        mutable int expanded_radius_count_ = 0;
    };
}

#endif // GEOIQ_DATASET_H
