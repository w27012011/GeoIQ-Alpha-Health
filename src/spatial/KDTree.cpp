#include "spatial/KDTree.h"
#include "utils/MathUtils.h"
#include <algorithm>
#include <limits>

namespace GeoIQ {

    void KDTree::build(const std::vector<SamplePoint>& points) {
        root_ = nullptr;
        size_ = 0;
        nodes_.clear();
        node_count_ = 0;

        if (points.empty()) {
            return;
        }

        // TRAP-01 Fix: Pre-resize memory pool vector to guarantee zero reallocations during tree assembly
        nodes_.resize(points.size());
        size_ = static_cast<int>(points.size());

        std::vector<const SamplePoint*> ptrs;
        ptrs.reserve(points.size());
        for (const auto& p : points) {
            ptrs.push_back(&p);
        }

        root_ = buildRecursive(ptrs, 0);
    }

    KDTree::Node* KDTree::buildRecursive(std::vector<const SamplePoint*>& pts, int depth) {
        if (pts.empty()) {
            return nullptr;
        }

        int split_axis = depth % 2; // 0 = split on latitude, 1 = split on longitude

        // Sort pointers based on split axis coordinate (strict weak ordering compliant)
        std::sort(pts.begin(), pts.end(), [split_axis](const SamplePoint* a, const SamplePoint* b) {
            if (split_axis == 0) {
                return a->lat < b->lat;
            } else {
                return a->lon < b->lon;
            }
        });

        size_t median_idx = pts.size() / 2;

        // Allocate a node from the contiguous vector pool (stable memory addresses)
        int curr_idx = node_count_++;
        Node* node = &nodes_[curr_idx];
        node->point = pts[median_idx];
        node->split_axis = split_axis;

        // Copy segments for recursive calls (safe tree size bounds)
        std::vector<const SamplePoint*> left_pts(pts.begin(), pts.begin() + median_idx);
        std::vector<const SamplePoint*> right_pts(pts.begin() + median_idx + 1, pts.end());

        node->left = buildRecursive(left_pts, depth + 1);
        node->right = buildRecursive(right_pts, depth + 1);

        return node;
    }

    std::vector<const SamplePoint*> KDTree::radiusSearch(double lat, double lon, double radius_km) const {
        std::vector<std::pair<double, const SamplePoint*>> results_with_dist;
        radiusSearchRecursive(root_, lat, lon, radius_km, results_with_dist);

        // Sort the matched neighbors by distance ascending
        std::sort(results_with_dist.begin(), results_with_dist.end(),
                  [](const std::pair<double, const SamplePoint*>& a, const std::pair<double, const SamplePoint*>& b) {
            return a.first < b.first;
        });

        std::vector<const SamplePoint*> final_results;
        final_results.reserve(results_with_dist.size());
        for (const auto& res : results_with_dist) {
            final_results.push_back(res.second);
        }
        return final_results;
    }

    void KDTree::radiusSearchRecursive(
        const Node* node,
        double query_lat, double query_lon, double radius_km,
        std::vector<std::pair<double, const SamplePoint*>>& results) const {
        if (node == nullptr) {
            return;
        }

        double dist = Math::haversine(query_lat, query_lon, node->point->lat, node->point->lon);
        if (dist <= radius_km) {
            results.push_back({dist, node->point});
        }

        double split_val = (node->split_axis == 0) ? node->point->lat : node->point->lon;
        double query_val = (node->split_axis == 0) ? query_lat : query_lon;
        double diff = query_val - split_val;

        // Determine which side of splitting plane is closer
        const Node* near_child = (diff <= 0.0) ? node->left : node->right;
        const Node* far_child  = (diff <= 0.0) ? node->right : node->left;

        // Traverse the closer branch first
        radiusSearchRecursive(near_child, query_lat, query_lon, radius_km, results);

        // Conservative splitting plane pruning: 1 deg latitude is exactly ~111 km.
        // We use 111.0 km/deg to guarantee we never prune valid neighbors.
        double plane_dist_km = std::abs(diff) * 111.0;
        if (plane_dist_km <= radius_km) {
            radiusSearchRecursive(far_child, query_lat, query_lon, radius_km, results);
        }
    }

    std::vector<const SamplePoint*> KDTree::knnSearch(double lat, double lon, int k) const {
        std::vector<const SamplePoint*> results;
        if (k <= 0 || root_ == nullptr) {
            return results;
        }

        std::priority_queue<std::pair<double, const SamplePoint*>> heap;
        knnSearchRecursive(root_, lat, lon, k, heap);

        // Extract elements from max-heap (returns them descending) and populate in ascending order
        results.resize(heap.size());
        for (int i = static_cast<int>(heap.size()) - 1; i >= 0; --i) {
            results[i] = heap.top().second;
            heap.pop();
        }
        return results;
    }

    void KDTree::knnSearchRecursive(
        const Node* node,
        double query_lat, double query_lon, int k,
        std::priority_queue<std::pair<double, const SamplePoint*>>& heap) const {
        if (node == nullptr) {
            return;
        }

        double dist = Math::haversine(query_lat, query_lon, node->point->lat, node->point->lon);

        if (static_cast<int>(heap.size()) < k) {
            heap.push({dist, node->point});
        } else if (dist < heap.top().first) {
            heap.pop();
            heap.push({dist, node->point});
        }

        double split_val = (node->split_axis == 0) ? node->point->lat : node->point->lon;
        double query_val = (node->split_axis == 0) ? query_lat : query_lon;
        double diff = query_val - split_val;

        const Node* near_child = (diff <= 0.0) ? node->left : node->right;
        const Node* far_child  = (diff <= 0.0) ? node->right : node->left;

        knnSearchRecursive(near_child, query_lat, query_lon, k, heap);

        double plane_dist_km = std::abs(diff) * 111.0;
        double worst_dist = (static_cast<int>(heap.size()) == k) ? heap.top().first : std::numeric_limits<double>::max();
        if (plane_dist_km < worst_dist) {
            knnSearchRecursive(far_child, query_lat, query_lon, k, heap);
        }
    }
}
