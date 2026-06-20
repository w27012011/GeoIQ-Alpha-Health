#include "output/PMTilesWriter.h"
#include "utils/MathUtils.h"
#include "utils/Constants.h"
#include "utils/Logger.h"
#include "utils/Statistics.h"
#include <map>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <zlib.h>

namespace GeoIQ {

    // Simple Protobuf Varint writer
    static void writeVarint(std::vector<uint8_t>& buf, uint64_t val) {
        do {
            uint8_t byte = val & 0x7F;
            val >>= 7;
            if (val > 0) byte |= 0x80;
            buf.push_back(byte);
        } while (val > 0);
    }

    static int64_t zigzag(int64_t val) {
        return (val << 1) ^ (val >> 63);
    }

    static void writeString(std::vector<uint8_t>& buf, uint32_t tag, const std::string& str) {
        writeVarint(buf, (tag << 3) | 2);
        writeVarint(buf, str.size());
        buf.insert(buf.end(), str.begin(), str.end());
    }

    // Gzip Compression Helper using zlib
    static bool gzipCompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        
        // 16 + MAX_WBITS sets window for Gzip output format
        if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            return false;
        }
        
        zs.next_in = (Bytef*)input.data();
        zs.avail_in = input.size();
        
        int ret;
        char outbuffer[32768];
        
        do {
            zs.next_out = (Bytef*)outbuffer;
            zs.avail_out = sizeof(outbuffer);
            
            ret = deflate(&zs, Z_FINISH);
            
            size_t written = sizeof(outbuffer) - zs.avail_out;
            if (written > 0) {
                output.insert(output.end(), outbuffer, outbuffer + written);
            }
        } while (ret == Z_OK);
        
        deflateEnd(&zs);
        return ret == Z_STREAM_END;
    }

    // Hilbert Curve Helpers for PMTiles v3 Tile ID Addressing
    static void rotate(uint64_t n, uint64_t rx, uint64_t ry, uint64_t& x, uint64_t& y) {
        if (ry == 0) {
            if (rx == 1) {
                x = n - 1 - x;
                y = n - 1 - y;
            }
            uint64_t t = x;
            x = y;
            y = t;
        }
    }

    static uint64_t zxyToTileId(uint32_t z, uint32_t x, uint32_t y) {
        if (z == 0) return 0;
        uint64_t acc = (((uint64_t)1 << z) * ((uint64_t)1 << z) - 1) / 3;
        uint64_t d = 0;
        uint64_t s = (uint64_t)1 << (z - 1);
        uint64_t rx, ry;
        uint64_t tx = x;
        uint64_t ty = y;
        while (s > 0) {
            rx = (tx & s) > 0 ? 1 : 0;
            ry = (ty & s) > 0 ? 1 : 0;
            d += s * s * ((3 * rx) ^ ry);
            rotate(s, rx, ry, tx, ty);
            s >>= 1;
        }
        return acc + d;
    }

    // Web Mercator projection calculations
    static void wgs84ToWebMercator(double lat, double lon, double& mx, double& my) {
        const double LimitY = 85.05112877980659;
        double clamped_lat = std::max(-LimitY, std::min(LimitY, lat));
        mx = lon * PI / 180.0;
        my = std::log(std::tan(PI / 4.0 + clamped_lat * PI / 360.0));
    }

    // Encodes a single MVT tile.
    static std::vector<uint8_t> encodeMvtTile(uint32_t z, uint32_t x, uint32_t y, 
                                              const std::vector<const PredictionRecord*>& points, 
                                              bool is_geology) {
        
        // Web Mercator tile bounding box
        double tile_size_rad = 2.0 * PI / (1 << z);
        double min_mx = -PI + x * tile_size_rad;
        double max_mx = -PI + (x + 1) * tile_size_rad;
        double min_my = PI - (y + 1) * tile_size_rad;
        double max_my = PI - y * tile_size_rad;

        // Collect keys and values for attributes mapping
        std::vector<std::string> keys = {"target", "risk_level", "score", "confidence"};
        std::vector<std::pair<int, std::string>> values_str; // string values map
        std::vector<std::pair<int, double>> values_double;  // number values map

        auto getStringValIndex = [&](const std::string& val) -> uint32_t {
            for (size_t i = 0; i < values_str.size(); ++i) {
                if (values_str[i].second == val) return i;
            }
            values_str.push_back({values_str.size(), val});
            return values_str.size() - 1;
        };

        auto getDoubleValIndex = [&](double val) -> uint32_t {
            for (size_t i = 0; i < values_double.size(); ++i) {
                if (std::abs(values_double[i].second - val) < 1e-6) return i;
            }
            values_double.push_back({values_double.size(), val});
            return values_double.size() - 1;
        };

        // Geometry coordinate packaging
        std::vector<uint8_t> features_buf;
        int32_t cursor_x = 0;
        int32_t cursor_y = 0;

        uint64_t fid = 1;
        for (const auto* rec : points) {
            double mx, my;
            wgs84ToWebMercator(rec->lat, rec->lon, mx, my);

            // Quantize coordinates to standard tile grid extent 4096
            int32_t px = static_cast<int32_t>(std::round((mx - min_mx) / (max_mx - min_mx) * 4096.0));
            int32_t py = static_cast<int32_t>(std::round((max_my - my) / (max_my - min_my) * 4096.0));
            px = std::clamp(px, 0, 4096);
            py = std::clamp(py, 0, 4096);

            for (const auto& res : rec->results) {
                // Determine if this target belongs to the active output mode
                bool is_hazard_target = (res.target == Target::ARSENIC_HAZARD || res.target == Target::FLUORIDE_HAZARD);
                if (is_geology && is_hazard_target) continue;
                if (!is_geology && !is_hazard_target) continue;

                // Feature container
                std::vector<uint8_t> feat;
                // id tag (1)
                writeVarint(feat, (1 << 3) | 0);
                writeVarint(feat, fid++);

                // tags mapping (2)
                std::vector<uint32_t> tags_array;
                // target -> tag 0
                tags_array.push_back(0);
                tags_array.push_back(getStringValIndex(Stats::targetToString(res.target)));
                // risk_level -> tag 1
                tags_array.push_back(1);
                tags_array.push_back(getStringValIndex(Stats::riskLevelToString(res.risk_level)));
                // score -> tag 2
                tags_array.push_back(2);
                tags_array.push_back(getDoubleValIndex(res.score));
                // confidence -> tag 3
                tags_array.push_back(3);
                tags_array.push_back(getDoubleValIndex(res.confidence));

                // Serialize tags as packed varints (tag = 2, length-delimited)
                std::vector<uint8_t> tags_bytes;
                for (auto t : tags_array) writeVarint(tags_bytes, t);
                writeVarint(feat, (2 << 3) | 2);
                writeVarint(feat, tags_bytes.size());
                feat.insert(feat.end(), tags_bytes.begin(), tags_bytes.end());

                // type tag (3): Point = 1
                writeVarint(feat, (3 << 3) | 0);
                writeVarint(feat, 1);

                // geometry tag (4): Packed commands
                int32_t dx = px - cursor_x;
                int32_t dy = py - cursor_y;
                cursor_x = px;
                cursor_y = py;

                std::vector<uint8_t> geom;
                // Command MoveTo (1), count 1: MoveTo = (1 & 7) | (1 << 3) = 9
                writeVarint(geom, 9);
                writeVarint(geom, zigzag(dx));
                writeVarint(geom, zigzag(dy));

                writeVarint(feat, (4 << 3) | 2);
                writeVarint(feat, geom.size());
                feat.insert(feat.end(), geom.begin(), geom.end());

                // Write feature tag to Layer features buffer
                writeVarint(features_buf, (2 << 3) | 2);
                writeVarint(features_buf, feat.size());
                features_buf.insert(features_buf.end(), feat.begin(), feat.end());
            }
        }

        if (fid == 1) {
            return {}; // Empty tile
        }

        // Build Layer proto structure
        std::vector<uint8_t> layer_buf;
        
        // name (1)
        writeString(layer_buf, 1, is_geology ? "resources" : "hazards");

        // version (15)
        writeVarint(layer_buf, (15 << 3) | 0);
        writeVarint(layer_buf, 2);

        // extent (5)
        writeVarint(layer_buf, (5 << 3) | 0);
        writeVarint(layer_buf, 4096);

        // keys (3)
        for (const auto& k : keys) {
            writeString(layer_buf, 3, k);
        }

        // values (4)
        for (const auto& v : values_str) {
            std::vector<uint8_t> val;
            // string_value (1)
            writeString(val, 1, v.second);
            
            writeVarint(layer_buf, (4 << 3) | 2);
            writeVarint(layer_buf, val.size());
            layer_buf.insert(layer_buf.end(), val.begin(), val.end());
        }

        for (const auto& v : values_double) {
            std::vector<uint8_t> val;
            // double_value (3): 8 bytes
            writeVarint(val, (3 << 3) | 1);
            uint64_t bytes;
            std::memcpy(&bytes, &v.second, 8);
            for (int i = 0; i < 8; ++i) {
                val.push_back((bytes >> (i * 8)) & 0xFF);
            }

            writeVarint(layer_buf, (4 << 3) | 2);
            writeVarint(layer_buf, val.size());
            layer_buf.insert(layer_buf.end(), val.begin(), val.end());
        }

        // Append features_buf
        layer_buf.insert(layer_buf.end(), features_buf.begin(), features_buf.end());

        // Final vector tile buffer
        std::vector<uint8_t> tile_buf;
        writeVarint(tile_buf, (3 << 3) | 2);
        writeVarint(tile_buf, layer_buf.size());
        tile_buf.insert(tile_buf.end(), layer_buf.begin(), layer_buf.end());

        return tile_buf;
    }

    bool PMTilesWriter::write(const std::string& output_path, const std::vector<PredictionRecord>& records, bool is_geology) {
        Logger::getInstance().info("PMTilesWriter", "Initiating PMTiles compilation for geology=" + std::to_string(is_geology));

        // 1. Group points into spatial tiles (Z0 to Z14)
        struct TileCoord {
            uint32_t z, x, y;
            bool operator<(const TileCoord& o) const {
                if (z != o.z) return z < o.z;
                if (x != o.x) return x < o.x;
                return y < o.y;
            }
        };

        std::map<TileCoord, std::vector<const PredictionRecord*>> tile_map;

        for (const auto& rec : records) {
            for (uint32_t z = 0; z <= 14; ++z) {
                double mx, my;
                wgs84ToWebMercator(rec.lat, rec.lon, mx, my);

                double tile_size_rad = 2.0 * PI / (1 << z);
                int32_t x = static_cast<int32_t>(std::floor((mx + PI) / tile_size_rad));
                int32_t y = static_cast<int32_t>(std::floor((PI - my) / tile_size_rad));
                x = std::max(0, std::min((1 << z) - 1, x));
                y = std::max(0, std::min((1 << z) - 1, y));

                tile_map[{z, static_cast<uint32_t>(x), static_cast<uint32_t>(y)}].push_back(&rec);
            }
        }

        // 2. Generate and compress individual tiles
        struct TileEntry {
            uint64_t tile_id;
            uint64_t offset;
            uint32_t length;
        };
        std::vector<TileEntry> entries;
        std::vector<uint8_t> tile_data_section;

        for (const auto& pair : tile_map) {
            std::vector<uint8_t> raw_mvt = encodeMvtTile(pair.first.z, pair.first.x, pair.first.y, pair.second, is_geology);
            if (raw_mvt.empty()) continue;

            std::vector<uint8_t> gzip_mvt;
            if (!gzipCompress(raw_mvt, gzip_mvt)) {
                Logger::getInstance().error("PMTilesWriter", "Failed to compress tile Z=" + std::to_string(pair.first.z));
                return false;
            }

            uint64_t tile_id = zxyToTileId(pair.first.z, pair.first.x, pair.first.y);
            entries.push_back({tile_id, tile_data_section.size(), static_cast<uint32_t>(gzip_mvt.size())});
            tile_data_section.insert(tile_data_section.end(), gzip_mvt.begin(), gzip_mvt.end());
        }

        if (entries.empty()) {
            Logger::getInstance().warn("PMTilesWriter", "No valid anomalies generated. PMTiles output will be blank.");
        }

        // Sort entries by Hilbert Tile ID (Mandatory spec compliance)
        std::sort(entries.begin(), entries.end(), [](const TileEntry& a, const TileEntry& b) {
            return a.tile_id < b.tile_id;
        });

        // 3. Serialize Root Directory (Varint arrays)
        std::vector<uint8_t> raw_dir;
        // Number of entries
        writeVarint(raw_dir, entries.size());
        
        // TileID deltas
        uint64_t last_id = 0;
        for (const auto& e : entries) {
            writeVarint(raw_dir, e.tile_id - last_id);
            last_id = e.tile_id;
        }

        // Run lengths (all 1)
        for (size_t i = 0; i < entries.size(); ++i) {
            writeVarint(raw_dir, 1);
        }

        // Lengths
        for (const auto& e : entries) {
            writeVarint(raw_dir, e.length);
        }

        // Offsets (Run-length delta encoding)
        uint64_t last_offset = 0;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i == 0) {
                writeVarint(raw_dir, 0);
            } else {
                writeVarint(raw_dir, entries[i].offset - last_offset + entries[i-1].length);
            }
            last_offset = entries[i].offset;
        }

        std::vector<uint8_t> compressed_dir;
        gzipCompress(raw_dir, compressed_dir);

        // 4. Serialize Metadata
        std::stringstream meta;
        meta << "{"
             << "  \"vector_layers\": ["
             << "    {"
             << "      \"id\": \"" << (is_geology ? "resources" : "hazards") << "\","
             << "      \"minzoom\": 0,"
             << "      \"maxzoom\": 14"
             << "    }"
             << "  ],"
             << "  \"name\": \"geoiq_" << (is_geology ? "geology" : "health") << "\","
             << "  \"type\": \"overlay\","
             << "  \"version\": \"1\""
             << "}";

        std::string meta_str = meta.str();
        std::vector<uint8_t> raw_meta(meta_str.begin(), meta_str.end());
        std::vector<uint8_t> compressed_meta;
        gzipCompress(raw_meta, compressed_meta);

        // 5. File Assembly & Offset Calculation
        uint64_t header_len = 127;
        uint64_t root_offset = header_len;
        uint64_t root_len = compressed_dir.size();
        uint64_t meta_offset = root_offset + root_len;
        uint64_t meta_len = compressed_meta.size();
        uint64_t tile_offset = meta_offset + meta_len;
        uint64_t tile_len = tile_data_section.size();

        // PMTiles Header struct alignment
        uint8_t header[127];
        std::memset(header, 0, 127);
        std::memcpy(header, "PMTiles", 7);
        header[7] = 3; // version

        auto writeHeaderUint64 = [&](size_t offset, uint64_t val) {
            for (int i = 0; i < 8; ++i) {
                header[offset + i] = (val >> (i * 8)) & 0xFF;
            }
        };

        writeHeaderUint64(8, root_offset);
        writeHeaderUint64(16, root_len);
        writeHeaderUint64(24, meta_offset);
        writeHeaderUint64(32, meta_len);
        writeHeaderUint64(40, 0); // leaf_dirs_offset
        writeHeaderUint64(48, 0); // leaf_dirs_len
        writeHeaderUint64(56, tile_offset);
        writeHeaderUint64(64, tile_len);
        writeHeaderUint64(72, entries.size()); // addressed_tiles
        writeHeaderUint64(80, entries.size()); // tile_entries
        writeHeaderUint64(88, entries.size()); // tagged_tiles
        writeHeaderUint64(96, 0); // keyed_values

        header[104] = 0; // sametile_count
        header[105] = 1; // internal_compression = gzip
        header[106] = 1; // tile_compression = gzip
        header[107] = 1; // tile_type = Vector MVT
        header[108] = 0; // min_zoom
        header[109] = 14; // max_zoom

        // Boundary box (min/max scale 10^7)
        int32_t min_lon = -180000000;
        int32_t min_lat = -90000000;
        int32_t max_lon = 180000000;
        int32_t max_lat = 90000000;
        std::memcpy(header + 110, &min_lon, 4);
        std::memcpy(header + 114, &min_lat, 4);
        std::memcpy(header + 118, &max_lon, 4);
        std::memcpy(header + 122, &max_lat, 4);

        header[126] = 0; // center zoom

        // 6. Write final file stream
        std::ofstream f(output_path, std::ios::binary);
        if (!f.is_open()) {
            Logger::getInstance().error("PMTilesWriter", "Failed to open output file: " + output_path);
            return false;
        }

        f.write((char*)header, 127);
        f.write((char*)compressed_dir.data(), compressed_dir.size());
        f.write((char*)compressed_meta.data(), compressed_meta.size());
        f.write((char*)tile_data_section.data(), tile_data_section.size());
        f.close();

        Logger::getInstance().info("PMTilesWriter", "Completed PMTiles write to: " + output_path);
        return true;
    }

}
