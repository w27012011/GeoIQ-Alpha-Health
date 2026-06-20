# GeoIQ & GeoHealth — Geological Intelligence & Hazard Mapping Platform
## High-Performance C++17 Subsurface Exploration & Environmental Health Engine
### USAII Global AI Hackathon 2026 Submission

---

## 🌍 Executive Summary
**GeoIQ** is a dual-purpose geostatistical exploration and public health advisory platform engineered in pure C++17. The system ingests multi-element geochemical datasets to perform two distinct, high-impact tasks:
1. 🚨 **Public Health Protection**: Identify groundwater Arsenic ($As$), Lead ($Pb$), and other toxic hazards to map vulnerable community water supplies.
2. 💎 **Resource Identification**: Vector toward critical minerals including Gold, Copper, Lithium, Diamonds, Zinc-Lead, and Rare Earth Elements.

By analyzing the same raw soil and water geochemistry datasets that commercial mining companies use, GeoIQ maps geological resource potential while concurrently screening local communities for severe environmental toxicant risks.

---

## 🛠️ System Architecture & Technology Stack
* **Backend Engine**: Pure C++17 (compiled with `-O3` optimizations). Handles KD-Tree spatial indexing, 2D IDW grid interpolation, Cheng singularity regressions, and multi-criteria rule scoring.
* **Frontend HTML Generator**: Standalone C++17 application compiled **fully statically** (no external DLL dependencies). Pairs geology and health data within a 5 km Haversine radius, generating 3 zero-dependency interactive Leaflet.js dashboards.
* **Database & Serialization**: Manual spatial GeoJSON serialization and SQLite predictions logging.

### Module Map
```
GeoIQ/
├── src/                          # Backend Geostatistical Engine
│   ├── main.cpp                  # Command-line orchestrator and pipeline sequencer
│   ├── data/
│   │   ├── SamplePoint.h         # Member-pointer arrays mapping ElementFields to doubles
│   │   ├── CSVLoader.h/.cpp      # Quote-stripping CSV parser and oxide converter
│   │   └── Dataset.h/.cpp        # Data owner, KD-Tree query wrapper, and ratio calculator
│   ├── spatial/
│   │   ├── KDTree.h/.cpp         # Memory-stable spatial binary index
│   │   ├── IDW.h/.cpp            # Inverse Distance Weighting with duplicate location averages
│   │   ├── MovingWindow.h/.cpp   # Background stats solver and power-law singularity math
│   │   ├── SpatialAnalysis.h/.cpp# Local Z-score spatial voting & dispersion train vectoring
│   │   └── GridGenerator.h/.cpp  # Computational bounding grids and virtual samples
│   ├── engine/
│   │   ├── RuleEngine.h/.cpp     # Expert geochemical logic rules for hazards & resources
│   │   └── ScoreEngine.h/.cpp    # Multi-variable score consolidator and deposit categorizer
│   ├── output/
│   │   ├── GeoJSONWriter.h/.cpp  # Spatial GeoJSON serializer
│   │   └── ReportWriter.h/.cpp   # Text findings compiler with JIT evidence string formatting
│   └── utils/
│       ├── Constants.h           # Physics constants and default parameters
│       ├── Enums.h               # Core types, ElementField maps, and string helpers
│       ├── Structs.h             # POD structs for rule execution traces and audit trails
│       ├── Logger.h/.cpp         # Thread-safe console/file logging Meyers Singleton
│       ├── MathUtils.h/.cpp      # Haversine distance, log transforms, and OLS regression
│       ├── Statistics.h/.cpp     # Vector sorting, WindowStats solver, and risk maps
│       └── Config.h/.cpp         # Pipeline config parameter parser
├── frontend/                     # Standalone HTML Generator
│   ├── src/
│   │   ├── DataTypes.h           # Struct definitions (GeoFeature, HealthRecord)
│   │   ├── JsonParser.h          # Minimal single-header JSON parser (no deps)
│   │   ├── GeoReader.h           # Parser for GeoJSON predictions and mine proximity references
│   │   ├── HealthReader.h        # Reader for health predictions in audit_health.db via SQLite
│   │   ├── GeoHealthGrouper.h    # Location pairing module (Haversine pairing <= 5km)
│   │   ├── HtmlCommon.h          # Shared dark-mode CSS styles and Leaflet CDN scripts
│   │   ├── HtmlTier1.h           # Public Safety Map generator
│   │   ├── HtmlTier2.h           # Advanced Research Map generator
│   │   └── HtmlTier3.h           # Full Auditor Map generator
│   ├── output/                   # Directory where generated HTML files are written
│   ├── build.ps1                 # Powershell build script (caches sqlite3.o for 1s recompiles)
│   └── main.cpp                  # Frontend Entry Point: loads databases & writes the HTML pages
├── data/
│   └── raw/                      # Raw input CSV datasets: samples.csv (60) & samples1.csv (14,571)
├── output/                       # Output folder for SQLite databases, reports, and logs
├── config_geology.ini            # Geology target configuration parameters
├── config_health.ini             # Health target configuration parameters (points to samples1.csv)
├── ABOUT.md                      # Detailed Devpost questionnaire submission answers
└── README.md                     # Platform documentation
```

---

## ⚙️ Core Scientific Algorithms
1. **Memory-Pooled KD-Tree Indexing**: Raw input arrays are loaded into contiguous memory. The 2D KD-Tree operates on an index-pointing structure that guarantees zero memory allocations during spatial search, resolving spatial queries in $O(\log N)$ time.
2. **Inverse Distance Weighting (IDW)**: Computes compositional estimates at virtual grid nodes using:
   $$\hat{Z}(x) = \frac{\sum_{i=1}^{k} w_i Z(x_i)}{\sum_{i=1}^{k} w_i}, \quad w_i = d(x, x_i)^{-p}$$
   *Duplicate coordinate checks (stacked boreholes) average exact matches at $d < 10^{-6}$ to prevent division-by-zero errors.*
3. **Cheng (2007) Power-Law Local Singularity**: Characterizes localized geological enrichments using a scale-invariant OLS linear regression of log-concentration vs log-radius:
   $$\log(C(r)) = (d - \alpha)\log(r) + c$$
   - $\alpha < 2.0$: High geogenic singularity spike (potential ore or localized contaminant).
   - $\alpha > 2.0$: Background or depleted zone.
4. **Compass-Sector Dispersion Analysis**: Scans neighboring samples in 8 compass directions. If a spatial gradient is detected, it utilizes an exponential decay model:
   $$C(d) = C_0 e^{-\lambda d}$$
   to vector back toward the probable geological source, outputting a bearing and estimated distance in kilometers.
5. **Haversine Spatiotemporal Pairing**: Filters health and geology sample points across independent datasets, matching nearest coordinate sites within a **5 km** spatial radius.

---

## 🚀 Step-by-Step Compilation & Running Guide
Judges can build and run both backend and frontend systems locally. Ensure you run these from a terminal inside the project root:

### 1. Compile the Backend Engine (GeoIQ)
Compile the C++17 command orchestrator directly using `g++` with high optimizations (`-O3`):
```bash
g++ -std=c++17 -O3 -Wall -Wextra -Isrc src/main.cpp src/utils/MathUtils.cpp src/utils/Statistics.cpp src/utils/Logger.cpp src/utils/Config.cpp src/data/CSVLoader.cpp src/data/Dataset.cpp src/spatial/KDTree.cpp src/spatial/IDW.cpp src/spatial/MovingWindow.cpp src/spatial/SpatialAnalysis.cpp src/engine/RuleEngine.cpp src/engine/ScoreEngine.cpp src/engine/GridGenerator.cpp src/output/GeoJSONWriter.cpp src/output/ReportWriter.cpp -o GeoIQ.exe
```

### 2. Run the Pipelines & Generate Databases
Execute both geological exploration and environmental safety pipelines to write the database and report outputs:
```bash
# Run geology targets (uses mock data data/raw/samples.csv)
.\GeoIQ.exe --config config_geology.ini

# Run health targets (uses real environmental pollution data data/raw/samples1.csv)
.\GeoIQ.exe --config config_health.ini
```
*Note: This generates `output/predictions.geojson` (geology predictions), `output/audit_health.db` (SQLite database with 14k+ health results), and `output/cross_referenced_anomalies.json` (mine proximity indices).*

### 3. Build & Run the Standalone HTML Generator (HTMLWriter)
Move into the `frontend` folder and execute the PowerShell build script. It automatically copies `sqlite3` amalgamation files, caches compilations, compiles `HTMLWriter.exe` statically, and executes it:
```bash
cd frontend
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Run
cd ..
```
*This compiles `frontend/HTMLWriter.exe` statically with zero external runtime DLL requirements, and writes the 3 dashboard HTML files in `frontend/output/`.*

---

## 📂 Output Analysis Files
Open the generated files under `frontend/output/` directly in any web browser:

1. **`tier1_public.html` (Public Safety Map)**:
   * Displays circle markers colored by WHO/EPA hazard thresholds.
   * Clicking a point shows an immediate action alert warning card, detailed symptoms, and mitigation steps (e.g., "Your Arsenic levels are 5.3x above WHO limits").
   * *Strictly suppresses mineral exploration targets and contains a prominent clinical disclaimer.*
2. **`tier2_advanced.html` (Advanced Research Map)**:
   * Layer selector to toggle between Health layers, Geology layers, or both overlaid.
   * Renders dual-marker rings (inner = health, outer = geology) and maps evidence details showing local/regional Z-scores and formula derivations.
3. **`tier3_auditor.html` (Full Auditor Map)**:
   * Maps all targets simultaneously, rendering mine symbols (⛏️) and drawing Haversine proximity guidelines from anomaly points to matching mines.
   * Includes a query filter box mimicking basic SQL `WHERE` clauses (e.g. `target = "GOLD" AND risk = "CRITICAL"`) to search and filter coordinates in real-time.
4. **`geo_health_groups.json`**:
   * The paired spatial database containing the 602 matched records.

---

## ⚖️ Responsible AI Guardrails
* **No False Complacency**: The system completely forbids the term `"SAFE"` inside the database and interface, labeling all baseline values as `"LOW RISK — Background level. Continue routine monitoring"`.
* **Resource Speculation Prevention**: Geology datasets are isolated and withheld from the public Tier 1 map to prevent dangerous, unauthorized wildcat mining speculation in rural zones.
* **Clinical Handoff**: The dashboards serve solely as screening decision-support indicators. Water warnings advise users to schedule laboratory assays, and clinical warnings advise consulting health professionals.
