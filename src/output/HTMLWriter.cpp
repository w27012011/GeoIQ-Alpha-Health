#include "output/HTMLWriter.h"
#include "utils/MathUtils.h"
#include "utils/Statistics.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace GeoIQ {

    namespace {
        std::string formatDouble(double v) {
            if (Math::isNoData(v) || !std::isfinite(v)) return "null";
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(6) << v;
            std::string s = ss.str();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') {
                s.pop_back();
            }
            return s;
        }

        std::string getUnitString(ElementField field) {
            if (field == ElementField::Au) return "ppb";
            int idx = static_cast<int>(field);
            if (idx >= static_cast<int>(ElementField::Fe) && idx <= static_cast<int>(ElementField::LOI)) {
                return "%";
            }
            if (field == ElementField::pH) return "pH";
            if (field == ElementField::Eh) return "mV";
            if (field == ElementField::elevation_m) return "m";
            return "ppm";
        }

        std::string escapeStringJS(const std::string& str) {
            std::string s = "";
            s.reserve(str.size());
            for (char c : str) {
                if (c == '"') {
                    s += "\\\"";
                } else if (c == '\\') {
                    s += "\\\\";
                } else if (c == '\n') {
                    s += "\\n";
                } else if (c == '\r') {
                    s += "\\r";
                } else if (c == '\t') {
                    s += "\\t";
                } else {
                    s += c;
                }
            }
            return s;
        }

        std::string serializeRecordToGeoJSON(const PredictionRecord& rec, const RuleResult& res) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(6);

            ss << "    {\n"
               << "      \"type\": \"Feature\",\n"
               << "      \"geometry\": {\n"
               << "        \"type\": \"Point\",\n"
               << "        \"coordinates\": [" << rec.lon << ", " << rec.lat << "]\n"
               << "      },\n"
               << "      \"properties\": {\n"
               << "        \"sample_id\": \"" << escapeStringJS(rec.sample_id) << "\",\n"
               << "        \"depth_m\": " << formatDouble(rec.depth_m) << ",\n"
               << "        \"elevation_m\": " << formatDouble(rec.elevation_m) << ",\n"
               << "        \"target\": \"" << Stats::targetToString(res.target) << "\",\n"
               << "        \"risk_level\": \"" << Stats::riskLevelToString(res.risk_level) << "\",\n"
               << "        \"score\": " << formatDouble(res.score) << ",\n"
               << "        \"confidence\": " << formatDouble(res.confidence) << ",\n"
               << "        \"annotation\": \"" << escapeStringJS(res.annotation) << "\",\n";

            // triggered_rules
            ss << "        \"triggered_rules\": [";
            for (size_t i = 0; i < res.triggered_rules.size(); ++i) {
                ss << "\"" << escapeStringJS(res.triggered_rules[i]) << "\"";
                if (i + 1 < res.triggered_rules.size()) ss << ", ";
            }
            ss << "],\n";

            // depth_zone
            if (res.depth_info.applicable && !res.depth_info.zone_name.empty()) {
                ss << "        \"depth_zone\": \"" << escapeStringJS(res.depth_info.zone_name) << "\",\n";
            } else {
                ss << "        \"depth_zone\": null,\n";
            }

            // spatial consistency, dispersion, quality notes
            ss << "        \"spatial_consistency\": " << formatDouble(res.spatial_consistency) << ",\n"
               << "        \"dispersion_detected\": " << (rec.dispersion_detected ? "true" : "false") << ",\n"
               << "        \"source_bearing_deg\": " << formatDouble(rec.source_bearing_deg) << ",\n"
               << "        \"source_distance_km\": " << formatDouble(rec.source_distance_km) << ",\n"
               << "        \"data_quality_note\": \"" << escapeStringJS(res.data_quality_note) << "\",\n";

            // evidence
            ss << "        \"evidence\": [\n";
            if (res.risk_level != RiskLevel::LOW && res.risk_level != RiskLevel::INSUFFICIENT_DATA) {
                for (size_t i = 0; i < res.evidence.size(); ++i) {
                    const auto& ev = res.evidence[i];
                    std::string el_name = elementFieldToString(ev.element);
                    std::string formula = Math::formulaString(el_name, ev.raw_value, ev.log_value,
                                                              ev.local_median_log, ev.local_stddev_log, ev.local_z);

                    ss << "          {\n"
                       << "            \"element\": \"" << el_name << "\",\n"
                       << "            \"unit\": \"" << getUnitString(ev.element) << "\",\n"
                       << "            \"raw_value\": " << formatDouble(ev.raw_value) << ",\n"
                       << "            \"local_z\": " << formatDouble(ev.local_z) << ",\n"
                       << "            \"regional_z\": " << formatDouble(ev.regional_z) << ",\n"
                       << "            \"alpha\": " << formatDouble(ev.alpha) << ",\n"
                       << "            \"singularity_state\": \"" << Stats::singularityStateToString(ev.singularity_state) << "\",\n"
                       << "            \"formula\": \"" << escapeStringJS(formula) << "\",\n"
                       << "            \"data_quality_note\": \"" << escapeStringJS(ev.data_quality_note) << "\"\n"
                       << "          }";
                    if (i + 1 < res.evidence.size()) ss << ",\n";
                    else ss << "\n";
                }
            }
            ss << "        ]\n"
               << "      }\n"
               << "    }";

            return ss.str();
        }
    }

    bool HTMLWriter::write(const std::string& filepath, const std::vector<PredictionRecord>& records) const {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        // 1. Calculate map center
        double min_lat = 90.0, max_lat = -90.0;
        double min_lon = 180.0, max_lon = -180.0;
        int valid_coords = 0;

        for (const auto& rec : records) {
            if (Math::hasData(rec.lat) && Math::hasData(rec.lon)) {
                if (rec.lat < min_lat) min_lat = rec.lat;
                if (rec.lat > max_lat) max_lat = rec.lat;
                if (rec.lon < min_lon) min_lon = rec.lon;
                if (rec.lon > max_lon) max_lon = rec.lon;
                valid_coords++;
            }
        }

        double center_lat = valid_coords > 0 ? (min_lat + max_lat) / 2.0 : 0.0;
        double center_lon = valid_coords > 0 ? (min_lon + max_lon) / 2.0 : 0.0;

        // 2. HTML map only shows MEDIUM and above — full data is in predictions.geojson.
        // We iterate all records below; the inner loop skips LOW/INSUFFICIENT_DATA results.

        // 3. Output HTML header and Leaflet links
        file << R"html(<!DOCTYPE html>
<html>
<head>
  <title>GeoIQ Geological Intelligence Map</title>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  
  <!-- Leaflet CSS and JS -->
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <!-- Leaflet MarkerCluster: GPU-friendly clustering for large point datasets -->
  <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.5.3/dist/MarkerCluster.css" />
  <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.5.3/dist/MarkerCluster.Default.css" />
  <script src="https://unpkg.com/leaflet.markercluster@1.5.3/dist/leaflet.markercluster.js"></script>
  <!-- Premium typography from Google Fonts -->
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700;800&family=JetBrains+Mono:wght@400;700&display=swap" rel="stylesheet">

  <style>
    body {
      margin: 0;
      padding: 0;
      font-family: 'Outfit', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background: #0d1117;
      color: #c9d1d9;
      overflow: hidden;
    }
    #map {
      height: 100vh;
      width: 100vw;
    }
    
    /* Modern Dashboard Overlay */
    .map-header {
      position: absolute;
      top: 15px;
      left: 60px;
      z-index: 1000;
      background: rgba(22, 27, 34, 0.85);
      backdrop-filter: blur(12px);
      border: 1px solid rgba(48, 54, 61, 0.8);
      border-radius: 12px;
      padding: 12px 20px;
      box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.5);
      pointer-events: auto;
      display: flex;
      align-items: center;
      gap: 15px;
    }
    .map-header img {
      height: 40px;
      width: auto;
      border-radius: 6px;
    }
    .map-header-text {
      display: flex;
      flex-direction: column;
    }
    .map-header h1 {
      margin: 0;
      font-size: 1.35em;
      font-weight: 800;
      letter-spacing: -0.5px;
      background: linear-gradient(135deg, #58a6ff, #bc8cff);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .map-header p {
      margin: 2px 0 0 0;
      font-size: 0.8em;
      color: #8b949e;
      font-weight: 400;
    }

    /* Custom styled Leaflet Controls */
    .leaflet-bar {
      border: 1px solid #30363d !important;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3) !important;
      border-radius: 8px !important;
      overflow: hidden;
    }
    .leaflet-bar a {
      background-color: #161b22 !important;
      color: #c9d1d9 !important;
      border-bottom: 1px solid #30363d !important;
    }
    .leaflet-bar a:hover {
      background-color: #21262d !important;
      color: #58a6ff !important;
    }
    
    .leaflet-control-layers {
      background: rgba(22, 27, 34, 0.9) !important;
      backdrop-filter: blur(12px);
      color: #c9d1d9 !important;
      border: 1px solid #30363d !important;
      border-radius: 10px !important;
      box-shadow: 0 8px 32px rgba(0,0,0,0.5) !important;
      padding: 12px !important;
      font-size: 0.9em;
    }
    .leaflet-control-layers-list label {
      margin-bottom: 6px;
      display: block;
      cursor: pointer;
    }

    /* ── Popup: scrollable, dark, constrained height ── */
    .leaflet-popup-content-wrapper {
      background: #161b22;
      color: #c9d1d9;
      border: 1px solid #30363d;
      border-radius: 14px;
      box-shadow: 0 12px 36px rgba(0,0,0,0.6);
      padding: 0;
      overflow: hidden;        /* clip rounded corners cleanly */
    }
    .leaflet-popup-tip {
      background: #161b22;
    }
    /* The inner content area: scroll when content is tall */
    .leaflet-popup-content {
      max-height: 62vh;
      overflow-y: auto;
      overflow-x: hidden;
      margin: 0;
      padding: 12px 14px;
      box-sizing: border-box;
    }
    .leaflet-popup-content::-webkit-scrollbar { width: 5px; }
    .leaflet-popup-content::-webkit-scrollbar-track { background: #0d1117; }
    .leaflet-popup-content::-webkit-scrollbar-thumb {
      background: #30363d;
      border-radius: 4px;
    }
    .leaflet-popup-content::-webkit-scrollbar-thumb:hover { background: #58a6ff; }

    .popup-container {
      min-width: 300px;
      max-width: 420px;
    }
    .popup-title-bar {
      border-bottom: 1px solid #30363d;
      padding-bottom: 8px;
      margin-bottom: 10px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
    }
    .popup-title {
      font-weight: 800;
      font-size: 1.05em;
      color: #f0f6fc;
      margin: 0;
      line-height: 1.3;
    }
    .popup-badge {
      font-size: 0.68em;
      font-weight: 800;
      padding: 3px 9px;
      border-radius: 6px;
      text-transform: uppercase;
      letter-spacing: 0.6px;
      white-space: nowrap;
      flex-shrink: 0;
    }
    .popup-meta-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 5px;
      margin-bottom: 10px;
      font-size: 0.78em;
      background: rgba(13, 17, 23, 0.6);
      padding: 8px;
      border-radius: 8px;
      border: 1px solid rgba(48, 54, 61, 0.4);
    }
    .popup-meta-item b { color: #8b949e; }
    .popup-annotation {
      font-size: 0.83em;
      line-height: 1.45;
      color: #c9d1d9;
      margin: 8px 0;
    }

    /* Geochemical evidence — monospace terminal style */
    .evidence-section {
      margin-top: 10px;
      border-top: 1px dashed #30363d;
      padding-top: 8px;
    }
    .evidence-title {
      font-weight: 700;
      font-size: 0.75em;
      color: #8b949e;
      text-transform: uppercase;
      letter-spacing: 0.6px;
      margin-bottom: 6px;
    }
    .evidence-item {
      font-family: 'JetBrains Mono', monospace;
      font-size: 0.73em;
      background: #0d1117;
      padding: 5px 7px;
      border-radius: 5px;
      border: 1px solid rgba(48, 54, 61, 0.5);
      margin-bottom: 5px;
      white-space: pre-wrap;
      word-break: break-all;
      line-height: 1.5;
    }

    /* ── Legend ── */
    .map-legend {
      position: absolute;
      bottom: 20px;
      right: 20px;
      z-index: 1000;
      background: rgba(22, 27, 34, 0.9);
      backdrop-filter: blur(12px);
      border: 1px solid #30363d;
      border-radius: 10px;
      padding: 14px;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5);
      font-size: 0.8em;
      pointer-events: auto;
    }
    .legend-title {
      font-weight: 700;
      margin-bottom: 8px;
      color: #f0f6fc;
      border-bottom: 1px solid #30363d;
      padding-bottom: 4px;
    }
    .legend-item {
      display: flex;
      align-items: center;
      margin-bottom: 5px;
    }
    .legend-color {
      width: 11px;
      height: 11px;
      border-radius: 50%;
      margin-right: 9px;
      border: 1.5px solid rgba(255,255,255,0.6);
      flex-shrink: 0;
    }

    /* ── MarkerCluster dark theme override ── */
    .marker-cluster-small  { background-color: rgba(88,166,255,0.12); }
    .marker-cluster-small div  { background-color: rgba(88,166,255,0.55); }
    .marker-cluster-medium { background-color: rgba(188,140,255,0.12); }
    .marker-cluster-medium div { background-color: rgba(188,140,255,0.55); }
    .marker-cluster-large  { background-color: rgba(204,0,0,0.12); }
    .marker-cluster-large div  { background-color: rgba(204,0,0,0.55); }
    .marker-cluster div {
      color: #f0f6fc;
      font-family: 'Outfit', sans-serif;
      font-weight: 700;
      font-size: 12px;
      border-radius: 50%;
    }

    /* ── Side findings panel ── */
    .side-panel {
      position: absolute; top: 0; right: 0; height: 100vh; width: 340px;
      background: rgba(13,17,23,0.96);
      backdrop-filter: blur(18px) saturate(180%);
      border-left: 1px solid #30363d;
      z-index: 2000;
      display: flex; flex-direction: column;
      transform: translateX(340px);
      transition: transform 0.28s cubic-bezier(0.4,0,0.2,1);
      pointer-events: auto;
      box-shadow: -10px 0 40px rgba(0,0,0,0.5);
    }
    .side-panel.open { transform: translateX(0); }
    .panel-toggle {
      position: absolute; top: 50%; left: -42px;
      transform: translateY(-50%);
      width: 42px; height: 54px;
      background: rgba(22,27,34,0.95);
      backdrop-filter: blur(8px);
      border: 1px solid #30363d; border-right: none;
      border-radius: 10px 0 0 10px;
      cursor: pointer;
      display: flex; align-items: center; justify-content: center;
      color: #8b949e; font-size: 20px;
      transition: color 0.2s, background 0.2s;
      pointer-events: auto; user-select: none;
    }
    .panel-toggle:hover { color: #58a6ff; background: rgba(88,166,255,0.08); }
    .panel-header {
      padding: 18px 16px 12px;
      border-bottom: 1px solid #30363d;
      flex-shrink: 0;
    }
    .panel-header h2 {
      margin: 0 0 10px 0; font-size: 1.05em; font-weight: 800;
      background: linear-gradient(135deg,#58a6ff,#bc8cff);
      -webkit-background-clip: text; -webkit-text-fill-color: transparent;
      letter-spacing: -0.3px;
    }
    .panel-stats { display: flex; gap: 5px; flex-wrap: wrap; }
    .stat-badge {
      font-size: 0.67em; font-weight: 800;
      padding: 2px 7px; border-radius: 5px;
      text-transform: uppercase; letter-spacing: 0.4px;
    }
    .panel-search {
      margin: 10px 14px 4px;
      padding: 7px 10px;
      background: rgba(13,17,23,0.8);
      border: 1px solid #30363d;
      border-radius: 8px;
      color: #c9d1d9;
      font-family: 'Outfit', sans-serif;
      font-size: 0.82em;
      width: calc(100% - 28px);
      box-sizing: border-box;
      outline: none;
      flex-shrink: 0;
    }
    .panel-search:focus { border-color: #58a6ff; }
    .panel-search::placeholder { color: #484f58; }
    .panel-list {
      flex: 1; overflow-y: auto; overflow-x: hidden; padding: 4px 0;
    }
    .panel-list::-webkit-scrollbar { width: 4px; }
    .panel-list::-webkit-scrollbar-thumb { background: #30363d; border-radius: 4px; }
    .panel-list::-webkit-scrollbar-thumb:hover { background: #58a6ff; }
    .panel-item {
      padding: 10px 16px; cursor: pointer;
      border-bottom: 1px solid rgba(48,54,61,0.35);
      transition: background 0.14s;
    }
    .panel-item:hover   { background: rgba(88,166,255,0.06); }
    .panel-item.active  { background: rgba(88,166,255,0.1); border-left: 3px solid #58a6ff; padding-left: 13px; }
    .panel-item-header  { display: flex; align-items: center; gap: 7px; margin-bottom: 4px; }
    .panel-item-badge {
      font-size: 0.6em; font-weight: 800;
      padding: 2px 6px; border-radius: 4px;
      text-transform: uppercase; letter-spacing: 0.4px;
      flex-shrink: 0; white-space: nowrap;
    }
    .panel-item-target {
      font-size: 0.8em; font-weight: 700; color: #f0f6fc;
      white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    }
    .panel-item-meta {
      font-size: 0.7em; color: #8b949e;
      font-family: 'JetBrains Mono', monospace; line-height: 1.45;
    }
    .panel-item-note {
      font-size: 0.67em; color: #ffcc00;
      margin-top: 3px; line-height: 1.3;
    }
    .panel-footer {
      padding: 9px 16px;
      border-top: 1px solid #30363d;
      font-size: 0.71em; color: #484f58;
      flex-shrink: 0; text-align: center;
    }
  </style>
</head>
<body>
  <div class="map-header">
    <img src="geoiq_logo.png" alt="GeoIQ Logo" onerror="this.style.display='none'" />
    <div class="map-header-text">
      <h1>GeoIQ Geological Intelligence</h1>
      <p>Spatial Predictive Exploration Dashboard</p>
    </div>
  </div>

  <div class="map-legend">
    <div class="legend-title">Risk & Potential</div>
    <div class="legend-item"><div class="legend-color" style="background:#CC0000;"></div>Critical</div>
    <div class="legend-item"><div class="legend-color" style="background:#FF4400;"></div>High</div>
    <div class="legend-item"><div class="legend-color" style="background:#FF8800;"></div>Medium-High</div>
    <div class="legend-item"><div class="legend-color" style="background:#FFCC00;"></div>Medium</div>
    <div class="legend-item"><div class="legend-color" style="background:#00AA44;"></div>Low</div>
    <div class="legend-item"><div class="legend-color" style="background:#888888;"></div>Insufficient Data</div>
  </div>

  <div id="map"></div>

  <!-- Slide-in findings panel — toggle with the ≡ button on its left edge -->
  <div class="side-panel" id="sidePanel">
    <div class="panel-toggle" id="panelToggle" title="Toggle findings panel">&#9776;</div>
    <div class="panel-header">
      <h2>GeoIQ Findings</h2>
      <div class="panel-stats" id="panelStats"></div>
    </div>
    <input class="panel-search" id="panelSearch" type="text" placeholder="Filter by ID, target, location…" />
    <div class="panel-list" id="panelList"></div>
    <div class="panel-footer" id="panelFooter"></div>
  </div>

  <script>
    // Embedded GeoJSON dataset (MEDIUM+ findings only; full data in predictions.geojson)
    var geoiq_data = {
      "type": "FeatureCollection",
      "features": [
)html";

        // 4. Inject GeoJSON features — MEDIUM and above only.
        // LOW and INSUFFICIENT_DATA results are excluded from the map for clarity and size.
        // The complete dataset (all risk levels) is preserved in predictions.geojson.
        bool first_feature = true;
        for (const auto& rec : records) {
            for (const auto& res : rec.results) {
                // Skip LOW and INSUFFICIENT_DATA for all record types — map shows findings only
                if (res.risk_level == RiskLevel::LOW ||
                    res.risk_level == RiskLevel::INSUFFICIENT_DATA) {
                    continue;
                }

                if (!first_feature) {
                    file << ",\n";
                }
                first_feature = false;
                file << serializeRecordToGeoJSON(rec, res);
            }
        }

        // 5. Close GeoJSON; output compact sample-location array; then JS
        file << "\n      ]\n"
             << "    };\n\n";

        // Compact [lat,lon] array for all survey sites — used for background dot layer
        file << "    var all_samples=[";
        bool first_s = true;
        for (const auto& rec : records) {
            if (!Math::hasData(rec.lat) || !Math::hasData(rec.lon)) continue;
            if (!first_s) file << ",";
            first_s = false;
            file << std::fixed << std::setprecision(4)
                 << "[" << rec.lat << "," << rec.lon << "]";
        }
        file << "];\n\n";

        file << "    var center_lat=" << std::fixed << std::setprecision(6) << center_lat << ";\n";
        file << "    var center_lon=" << center_lon << ";\n";
        file << R"js(
    var map = L.map('map',{preferCanvas:true}).setView([center_lat,center_lon],5);

    L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',{
      attribution:'&copy; OpenStreetMap contributors &copy; CartoDB',
      subdomains:'abcd', maxZoom:20
    }).addTo(map);

    function getColor(r){
      return r==='CRITICAL'?'#CC0000':r==='HIGH'?'#FF4400':r==='MEDIUM_HIGH'?'#FF8800':r==='MEDIUM'?'#FFCC00':'#888888';
    }
    function getTxt(r){ return r==='MEDIUM'?'#111':'#fff'; }

    var markerStore={};
    var targetGroups={};

    L.geoJSON(geoiq_data,{
      pointToLayer:function(feat,latlng){
        var p=feat.properties;
        var isGrid=p.sample_id.indexOf('GRID_')===0;
        var mk=L.circleMarker(latlng,{
          radius:isGrid?5:8,
          fillColor:getColor(p.risk_level),
          color:isGrid?'#1f2937':'#ffffff',
          weight:isGrid?0.5:2, opacity:1,
          fillOpacity:isGrid?0.75:0.95
        });

        var bg=getColor(p.risk_level), tc=getTxt(p.risk_level);
        var pc="<div class='popup-container'>";
        pc+="<div class='popup-title-bar'>";
        pc+="<span class='popup-title'>"+p.target.replace(/_/g,' ')+"</span>";
        pc+="<span class='popup-badge' style='background:"+bg+";color:"+tc+";'>"+p.risk_level.replace(/_/g,' ')+"</span>";
        pc+="</div>";

        pc+="<div class='popup-meta-grid'>";
        pc+="<div class='popup-meta-item'><b>Sample ID:</b> "+p.sample_id+"</div>";
        pc+="<div class='popup-meta-item'><b>Coords:</b> "+latlng.lat.toFixed(5)+", "+latlng.lng.toFixed(5)+"</div>";
        if(p.depth_m!==null)     pc+="<div class='popup-meta-item'><b>Depth:</b> "+p.depth_m+" m</div>";
        if(p.elevation_m!==null) pc+="<div class='popup-meta-item'><b>Elevation:</b> "+p.elevation_m+" m</div>";
        pc+="<div class='popup-meta-item'><b>Score:</b> "+p.score.toFixed(3)+"</div>";
        pc+="<div class='popup-meta-item'><b>Confidence:</b> "+p.confidence.toFixed(2)+"</div>";
        if(p.spatial_consistency!==null)
          pc+="<div class='popup-meta-item'><b>Spatial Cons.:</b> "+p.spatial_consistency.toFixed(2)+"</div>";
        if(p.triggered_rules&&p.triggered_rules.length)
          pc+="<div class='popup-meta-item'><b>Rules:</b> "+p.triggered_rules.join(', ')+"</div>";
        pc+="</div>";

        pc+="<div class='popup-annotation'>"+p.annotation+"</div>";
        if(p.data_quality_note)
          pc+="<div class='popup-annotation' style='color:#ffcc00;font-weight:600;font-size:0.8em;border-top:1px dashed #30363d;padding-top:6px;'>&#9888; "+p.data_quality_note+"</div>";

        if(p.evidence&&p.evidence.length){
          pc+="<div class='evidence-section'><div class='evidence-title'>Geochemical Evidence Trail</div>";
          p.evidence.forEach(function(ev){
            var rv=ev.raw_value!==null?ev.raw_value.toFixed(2):'\u2014';
            var zv=(ev.local_z!==null&&ev.local_z!==undefined)?ev.local_z.toFixed(2):'N/A';
            var av=(ev.alpha!==null&&ev.alpha!==undefined)?ev.alpha.toFixed(3):'N/A';
            var sv=ev.singularity_state||'INVALID';
            pc+="<div class='evidence-item'>"+
              ev.element+": "+rv+"\u00a0"+ev.unit+
              "\u2002|\u2002Z\u202f=\u202f"+zv+
              "\u2002|\u2002\u03b1\u202f=\u202f"+av+
              "\u2002["+sv+"]\n"+ev.formula;
            if(ev.data_quality_note) pc+="\n\u26a0 "+ev.data_quality_note;
            pc+="</div>";
          });
          pc+="</div>";
        }
        pc+="</div>";

        mk.bindPopup(pc,{maxWidth:450});

        var fid=p.sample_id+'|'+p.target;
        markerStore[fid]=mk;

        if(!targetGroups[p.target]){
          targetGroups[p.target]=L.markerClusterGroup({
            chunkedLoading:true, maxClusterRadius:55,
            spiderfyOnMaxZoom:true, showCoverageOnHover:false,
            zoomToBoundsOnClick:true
          });
        }
        targetGroups[p.target].addLayer(mk);
        return mk;
      }
    });

    // All survey-site background dots (canvas-rendered for performance)
    var bgCanvas=L.canvas({padding:0.5});
    var bgGroup=L.featureGroup();
    all_samples.forEach(function(s){
      L.circleMarker([s[0],s[1]],{
        renderer:bgCanvas, radius:2,
        fillColor:'#58a6ff', color:'transparent',
        fillOpacity:0.22, interactive:false
      }).addTo(bgGroup);
    });
    bgGroup.addTo(map);

    // Layer control
    var overlayMaps={};
    overlayMaps['All Sample Sites ('+all_samples.length+')']=bgGroup;
    for(var tkey in targetGroups){
      overlayMaps[tkey.replace(/_/g,' ')]=targetGroups[tkey];
      targetGroups[tkey].addTo(map);
    }
    L.control.layers(null,overlayMaps,{collapsed:false}).addTo(map);

    // Fit bounds to anomaly features
    var bnds=geoiq_data.features.map(function(f){
      return [f.geometry.coordinates[1],f.geometry.coordinates[0]];
    });
    if(bnds.length>0) map.fitBounds(bnds,{padding:[40,40]});

    // ── Side panel ──────────────────────────────────────────────────
    var panelOpen=false, activeItem=null;

    document.getElementById('panelToggle').addEventListener('click',function(){
      panelOpen=!panelOpen;
      document.getElementById('sidePanel').classList.toggle('open',panelOpen);
      this.innerHTML=panelOpen?'&#10005;':'&#9776;';
    });

    var riskOrd={CRITICAL:0,HIGH:1,MEDIUM_HIGH:2,MEDIUM:3};
    var sorted=geoiq_data.features.slice().sort(function(a,b){
      var ra=riskOrd[a.properties.risk_level]!==undefined?riskOrd[a.properties.risk_level]:99;
      var rb=riskOrd[b.properties.risk_level]!==undefined?riskOrd[b.properties.risk_level]:99;
      return ra!==rb?ra-rb:b.properties.score-a.properties.score;
    });

    var cnts={CRITICAL:0,HIGH:0,MEDIUM_HIGH:0,MEDIUM:0};
    sorted.forEach(function(f){var r=f.properties.risk_level;if(cnts[r]!==undefined)cnts[r]++;});

    var statsEl=document.getElementById('panelStats');
    [{k:'CRITICAL',bg:'#CC0000',c:'#fff'},
     {k:'HIGH',bg:'#FF4400',c:'#fff'},
     {k:'MEDIUM_HIGH',bg:'#FF8800',c:'#fff',lbl:'MED-HIGH'},
     {k:'MEDIUM',bg:'#FFCC00',c:'#111'}
    ].forEach(function(d){
      if(!cnts[d.k]) return;
      var sp=document.createElement('span');
      sp.className='stat-badge';
      sp.style.cssText='background:'+d.bg+';color:'+d.c;
      sp.textContent=cnts[d.k]+' '+(d.lbl||d.k.replace(/_/g,' '));
      statsEl.appendChild(sp);
    });
    var sb=document.createElement('span');
    sb.className='stat-badge';
    sb.style.cssText='background:#30363d;color:#8b949e';
    sb.textContent=all_samples.length+' sites surveyed';
    statsEl.appendChild(sb);

    // Notice about targets with zero findings
    var targFound={};
    geoiq_data.features.forEach(function(f){targFound[f.properties.target]=true;});
    var allTargs=['ARSENIC_HAZARD','GOLD','DIAMOND_KIMBERLITE','COPPER_PORPHYRY',
                  'COPPER_VMS','LITHIUM_PEGMATITE','LITHIUM_BRINE','REE_CARBONATITE',
                  'NICKEL_PGE','ZINC_LEAD','GOSSAN'];
    var zeroTargs=allTargs.filter(function(t){return !targFound[t];});

    var listEl=document.getElementById('panelList'), itemEls=[];

    if(zeroTargs.length>0){
      var notice=document.createElement('div');
      notice.style.cssText='padding:10px 16px;font-size:0.71em;color:#8b949e;border-bottom:1px solid #30363d;line-height:1.55;';
      notice.innerHTML='<b style="color:#c9d1d9">No anomalies detected:</b><br>'+
        zeroTargs.map(function(t){return t.replace(/_/g,' ');}).join(', ')+'<br>'+
        '<span style="opacity:0.7">Dataset lacks required element coverage for these targets.</span>';
      listEl.appendChild(notice);
    }

    sorted.forEach(function(f){
      var p=f.properties, co=f.geometry.coordinates;
      var bg=getColor(p.risk_level), tc=getTxt(p.risk_level);
      var fid=p.sample_id+'|'+p.target;

      var note='';
      if(p.data_quality_note&&p.data_quality_note.length>0)
        note='<div class="panel-item-note">&#9888; '+
             p.data_quality_note.substring(0,90)+(p.data_quality_note.length>90?'&hellip;':'')+
             '</div>';

      var el=document.createElement('div');
      el.className='panel-item';
      el.innerHTML=
        '<div class="panel-item-header">'+
        '<span class="panel-item-badge" style="background:'+bg+';color:'+tc+'">'+p.risk_level.replace(/_/g,' ')+'</span>'+
        '<span class="panel-item-target">'+p.target.replace(/_/g,' ')+'</span>'+
        '</div>'+
        '<div class="panel-item-meta">'+p.sample_id+'<br>'+
        co[1].toFixed(4)+'&deg;, '+co[0].toFixed(4)+'&deg;&ensp;&middot;&ensp;Score: '+p.score.toFixed(2)+'</div>'+
        note;

      el.addEventListener('click',(function(lat,lon,sid){
        return function(){
          if(activeItem) activeItem.classList.remove('active');
          this.classList.add('active'); activeItem=this;
          map.flyTo([lat,lon],13,{animate:true,duration:0.8});
          var mk2=markerStore[sid];
          if(mk2) setTimeout(function(){mk2.openPopup();},900);
        };
      })(co[1],co[0],fid));

      listEl.appendChild(el);
      itemEls.push(el);
    });

    // Search
    document.getElementById('panelSearch').addEventListener('input',function(){
      var q=this.value.toLowerCase();
      itemEls.forEach(function(el){
        el.style.display=(!q||el.textContent.toLowerCase().indexOf(q)>=0)?'':'none';
      });
    });

    document.getElementById('panelFooter').textContent=
      sorted.length+' findings \u00b7 Low-risk records in predictions.geojson';
  </script>
</body>
</html>
)js";

        file.close();
        return true;
    }
}
