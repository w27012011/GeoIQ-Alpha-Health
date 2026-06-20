#ifndef GEOIQ_KDTREE_H
#define GEOIQ_KDTREE_H

#include <vector>
#include <queue>
#include "data/SamplePoint.h"

namespace GeoIQ {

    class KDTree {
    public:
        KDTree() = default;

        // Build the KD-Tree from a vector of SamplePoints.
        // Stores pointers to points. The caller (Dataset) must keep the points alive.
        void build(const std::vector<SamplePoint>& points);

        // Return all points within radius_km of (lat, lon), sorted by distance ascending.
        std::vector<const SamplePoint*> radiusSearch(double lat, double lon, double radius_km) const;

        // Return the K nearest points to (lat, lon), sorted by distance ascending.
        std::vector<const SamplePoint*> knnSearch(double lat, double lon, int k) const;

        bool empty() const { return root_ == nullptr; }
        int  size()  const { return size_; }

    private:
        struct Node {
            const SamplePoint* point = nullptr;
            int   split_axis = 0;   // 0 = split on lat, 1 = split on lon
            Node* left  = nullptr;
            Node* right = nullptr;
        };

        Node* root_ = nullptr;
        int   size_ = 0;

        // Memory pool variables to avoid segmented heap allocations (TRAP-01 Fix)
        std::vector<Node> nodes_;
        int node_count_ = 0;

        // Recursive tree builder
        Node* buildRecursive(std::vector<const SamplePoint*>& pts, int depth);

        // Recursive radius search helper
        void radiusSearchRecursive(
            const Node* node,
            double query_lat, double query_lon, double radius_km,
            std::vector<std::pair<double, const SamplePoint*>>& results) const;

        // Recursive KNN search helper
        void knnSearchRecursive(
            const Node* node,
            double query_lat, double query_lon, int k,
            std::priority_queue<std::pair<double, const SamplePoint*>>& heap) const;
    };
}

#endif // GEOIQ_KDTREE_H
