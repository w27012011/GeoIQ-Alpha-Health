# GeoIQ & GeoHealth — Geological Intelligence & Hazard Mapping Platform
## High-Performance C++17 Subsurface Exploration & Environmental Health Engine
### USAII Global AI Hackathon 2026 Submission

---

## ▶️ HOW TO RUN — Read This First!

> No installation required. No Python. No extra software. Just double-click and go.

---

### 🟢 Approach 1 — Simple (One Click, Recommended)

**Who this is for:** Anyone — a judge, a student, anyone. No technical knowledge needed.

**What to do:**
1. Open the `GeoIQ-M Health Alpha 1.0` folder (the one you're reading this from).
2. Find the file called **`run_pipeline.bat`**.
3. **Double-click it.**

That's it. The program will:
- Automatically process 14,571 real groundwater samples and run the health hazard analysis.
- Automatically run the geological exploration analysis.
- Automatically generate 3 interactive maps.
- **Open the Public Safety Map in your browser automatically** when done.

> ⏳ **Wait time:** About 2–3 minutes for the full pipeline to finish. A black console window will show the progress — don't close it.

**After it finishes, you can also open these maps manually in any browser:**
- `frontend/output/tier1_public.html` → Public Safety Map (for general users & health workers)
- `frontend/output/tier2_advanced.html` → Advanced Research Map (for researchers)
- `frontend/output/tier3_auditor.html` → Full Audit Map (for scientists & auditors)

---

### 🔵 Approach 2 — Manual (Step by Step)

**Who this is for:** Judges or developers who want to run each step individually to see exactly what happens.

**Step 1: Open a terminal in this folder**
> Right-click inside the `GeoIQ-M Health Alpha 1.0` folder → `Open in Terminal` (or PowerShell)

**Step 2: Run the health hazard pipeline**
This processes the 14,571-sample environmental dataset and writes the database:
```
.\GeoIQ.exe --config config_health.ini
```
✅ Done when you see: `GeoIQ Pipeline execution complete`

**Step 3: Run the geology exploration pipeline**
This processes the 60-sample geology dataset and writes the geology predictions:
```
.\GeoIQ.exe --config config_geology.ini
```
✅ Done when you see: `GeoIQ Pipeline execution complete`

**Step 4: Generate the interactive HTML maps**
This reads both databases and builds the 3 offline browser maps:
```
.\HTMLWriter.exe
```
✅ Done when you see: `Done! Open these files in your browser`

**Step 5: Open the maps**
Open any of these files in your browser (Chrome, Edge, Firefox — all work):
- `frontend/output/tier1_public.html` — Public Safety Map
- `frontend/output/tier2_advanced.html` — Advanced Research Map
- `frontend/output/tier3_auditor.html` — Full Audit Map

> 💡 **Note:** The maps are pre-generated and ready to open right now — you don't need to run the pipeline first if you just want to see the results.

---

## 🌍 Executive Summary
**GeoIQ** is a dual-purpose geostatistical exploration and public health advisory platform. The system ingests multi-element geochemical datasets to perform two distinct, high-impact tasks:
1. 🚨 **Public Health Protection**: Identify groundwater Arsenic ($As$), Lead ($Pb$), and other toxic hazards to map vulnerable community water supplies.
2. 💎 **Resource Identification**: Vector toward critical minerals including Gold, Copper, Lithium, Diamonds, Zinc-Lead, and Rare Earth Elements.

By analyzing the same raw soil and water geochemistry datasets that commercial mining companies use, GeoIQ maps geological resource potential while concurrently screening local communities for severe environmental toxicant risks.

---

## 🛠️ System Architecture & Technology Stack
* **Backend Engine (`GeoIQ.exe`)**: Pure C++17 (compiled with `-O3` optimizations). Handles KD-Tree spatial indexing, 2D IDW grid interpolation, Cheng singularity regressions, and multi-criteria rule scoring. Fully statically compiled with no external GCC runtime DLL dependencies.
* **Frontend HTML Generator (`HTMLWriter.exe`)**: Standalone C++17 application compiled **fully statically** (no external DLL dependencies). Pairs geology and health data within a 5 km Haversine radius, generating 3 zero-dependency interactive Leaflet.js dashboards.
* **Database & Serialization**: Manual spatial GeoJSON serialization and SQLite predictions logging.

### Module Map
```
GeoIQ-M Health Alpha/
├── data/
│   ├── raw/
│   │   ├── samples.csv           # 60-sample geology exploration demo dataset
│   │   └── Samples_Polution.csv  # 14,571-sample environmental health dataset
│   └── toxicology_database.json  # WHO/EPA reference thresholds database
├── src/                          # Backend Geostatistical Engine C++ Source Code
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
│   │   ├── SQLiteWriter.h/.cpp   # SQLite prediction logging writer
│   │   ├── PMTilesWriter.h/.cpp  # PMTiles binary archive compiler
│   │   ├── HTMLWriter.h/.cpp     # Decimated map dashboard serialization
│   │   └── ReportWriter.h/.cpp   # Text findings compiler with JIT evidence string formatting
│   └── utils/
│       ├── Constants.h           # Physics constants and default parameters
│       ├── Enums.h               # Core types, ElementField maps, and string helpers
│       ├── Structs.h             # POD structs for rule execution traces and audit trails
│       ├── Logger.h/.cpp         # Thread-safe console/file logging Meyers Singleton
│       ├── MathUtils.h/.cpp      # Haversine distance, log transforms, and OLS regression
│       ├── Statistics.h/.cpp     # Vector sorting, WindowStats solver, and risk maps
│       ├── Config.h/.cpp         # Pipeline config parameter parser
│       ├── sqlite3.c             # SQLite C amalgamation source
│       └── sqlite3.h             # SQLite header file
├── frontend/                     # Standalone HTML Generator C++ Source Code
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
│   ├── output/                   # Directory where generated HTML files are stored
│   │   ├── geo_health_groups.json# Merged database
│   │   ├── tier1_public.html     # Public safety map
│   │   ├── tier2_advanced.html   # Advanced research map
│   │   └── tier3_auditor.html     # Full auditor map
│   ├── build.ps1                 # Powershell build script (caches sqlite3.o for 1s recompiles)
│   └── main.cpp                  # Frontend Entry Point: loads databases & writes the HTML pages
├── output/                       # Output folder where SQLite databases and predictions go
│   ├── audit.db                  # SQLite database for geology target evaluations
│   ├── audit_health.db           # SQLite database for environmental health hazard evaluations
│   ├── cross_referenced_anomalies.json # Mine cross-reference indicators
│   ├── predictions.geojson       # Exploration prediction spatial records
│   ├── predictions.pmtiles       # Exploration PMTiles map
│   ├── predictions_health.pmtiles # Health PMTiles map
│   ├── report.txt                # Geology exploration text findings report
│   └── report_health.txt         # Environmental health findings report
├── GeoIQ.exe                     # Statically compiled, zero-dependency engine
├── HTMLWriter.exe                # Statically compiled, zero-dependency dashboard generator
├── config_geology.ini            # Geology target configuration parameters
├── config_health.ini             # Health target configuration parameters
├── geoiq_logo.png                # Asset logo
├── run_pipeline.bat              # One-click automated runner batch file
├── CMakeLists.txt                # Main CMakeLists configuration
├── config.txt                    # Default config properties
├── ABOUT.md                      # Detailed questionnaire reference
├── SUBMISSION_CHANGELOG.md       # Changelog of recent improvements
└── README.md                     # Platform manual and guides
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

## 🚀 Step-by-Step Execution Guide
Both the backend and frontend systems are pre-compiled and statically linked. You can run the entire pipeline with a single click:

### Option 1: One-Click Runner (Recommended)
Double-click `run_pipeline.bat` at the root of this folder.
* This automatically runs the health pipeline, runs the geology pipeline, compiles the interactive HTML maps, and opens the Public Safety map in your default browser.

### Option 2: Manual Console Commands
Open a terminal in this directory and execute the following:
```bash
# 1. Run geology targets (uses exploration data data/raw/samples.csv)
.\GeoIQ.exe --config config_geology.ini

# 2. Run health targets (uses environmental pollution data data/raw/Samples_Polution.csv)
.\GeoIQ.exe --config config_health.ini

# 3. Generate HTML Tiers
.\HTMLWriter.exe
```

### Option 3: Compile from Source (Optional)
If you wish to compile the C++ binaries yourself, make sure you have MinGW `g++` (supporting C++17) installed and run:

1. **Compile Backend Engine (`GeoIQ.exe`)**:
   ```bash
   g++ -std=c++17 -O3 -Wall -Wextra -Isrc src/main.cpp src/utils/MathUtils.cpp src/utils/Statistics.cpp src/utils/Logger.cpp src/utils/Config.cpp src/data/CSVLoader.cpp src/data/Dataset.cpp src/spatial/KDTree.cpp src/spatial/IDW.cpp src/spatial/MovingWindow.cpp src/spatial/SpatialAnalysis.cpp src/engine/RuleEngine.cpp src/engine/ScoreEngine.cpp src/engine/GridGenerator.cpp src/output/GeoJSONWriter.cpp src/output/HTMLWriter.cpp src/output/ReportWriter.cpp src/output/SQLiteWriter.cpp src/output/PMTilesWriter.cpp src/utils/sqlite3.c -lz -static -static-libgcc -static-libstdc++ -o GeoIQ.exe
   ```
   *Note: Linking requires the `zlib` library (-lz) and `-static` options for standalone execution.*

2. **Compile Frontend Generator (`HTMLWriter.exe`)**:
   ```bash
   cd frontend
   powershell -ExecutionPolicy Bypass -File .\build.ps1
   cd ..
   ```
   *(This downloads compiler assets dynamically and builds HTMLWriter.exe statically).*

---

## 📂 Output Analysis Files
Once the pipeline has completed, open the generated files under `frontend/output/` directly in any web browser:

1. **[tier1_public.html](frontend/output/tier1_public.html) (Public Safety Map)**:
   * Displays circle markers colored by WHO/EPA hazard thresholds.
   * Clicking a point shows an immediate action alert warning card, detailed symptoms, and mitigation steps (e.g., "Your Arsenic levels are 5.3x above WHO limits").
   * *Strictly suppresses mineral exploration targets and contains a prominent clinical disclaimer.*
2. **[tier2_advanced.html](frontend/output/tier2_advanced.html) (Advanced Research Map)**:
   * Layer selector to toggle between Health layers, Geology layers, or both overlaid.
   * Renders dual-marker rings (inner = health, outer = geology) and maps evidence details showing local/regional Z-scores and formula derivations.
3. **[tier3_auditor.html](frontend/output/tier3_auditor.html) (Full Auditor Map)**:
   * Maps all targets simultaneously, rendering mine symbols (⛏️) and drawing Haversine proximity guidelines from anomaly points to matching mines.
   * Includes a query filter box mimicking basic SQL `WHERE` clauses (e.g. `target = "GOLD" AND risk = "CRITICAL"`) to search and filter coordinates in real-time.
4. **`frontend/output/geo_health_groups.json`**:
   * The paired spatial database containing the 602 matched records.

---

## ⚖️ Responsible AI Guardrails
* **No False Complacency**: The system completely forbids the term `"SAFE"` inside the database and interface, labeling all baseline values as `"LOW RISK — Background level. Continue routine monitoring"`.
* **Resource Speculation Prevention**: Geology datasets are isolated and withheld from the public Tier 1 map to prevent dangerous, unauthorized wildcat mining speculation in rural zones.
* **Clinical Handoff**: The dashboards serve solely as screening decision-support indicators. Water warnings advise users to schedule laboratory assays, and clinical warnings advise consulting health professionals.
