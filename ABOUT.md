# GeoIQ & GeoHealth — Master Reference & Submission Manual
**USAII Global AI Hackathon 2026 — Team GeoIQ-Alpha**
*Qualifier Code:* `HS26-44677104` | *Qualifier Score:* `100/100 (Rank #1 of 320)`

---

## 1. Devpost Submission Questionnaire Answers (Detailed)
*Copy and paste these directly into your Devpost required fields:*

### Section 3 — Problem
* **Problem**:
  Mining exploration companies routinely collect rich geochemical datasets (soil and stream sediment samples) to locate minerals like gold, copper, and lithium. However, this exploration data is rarely analyzed for public health hazards. Toxic heavy metals like Arsenic ($As$) and Lead ($Pb$) naturally leach from the same deposit formations into local water wells. Because traditional GIS mapping tools require heavy, server-dependent software and internet access, they are unusable in remote, offline exploration settings. Consequently, rural drinking water consumers remain exposed to toxic groundwaters without their knowledge.
* **Who it affects**:
  Vulnerable rural community drinking water consumers, field health workers, municipal environmental officers, and mineral exploration geologists operating in remote or low-connectivity areas.
* **Why it matters**:
  Chronic ingestion of groundwater Arsenic and Lead causes irreversible health conditions, including Arsenicosis, Blackfoot disease, skin/lung cancers, and severe neurodevelopmental delays in children. Early screening using existing geochemical data can save thousands of lives and optimize municipal water filtration budgets, but it is currently blocked by the lack of unified, offline-capable analysis tools.

---

### Section 4 — AI Solution
* **Why AI**:
  Geochemical surveys contain thousands of points with dozens of overlapping elemental fields (As, Pb, Cu, Au, pH, Eh, etc.). The spatial relationships (dispersion vectors, geological boundaries) are non-linear. Manual calculation or basic spreadsheets cannot scale to run 2D nearest-neighbor indexing,Inverse Distance Weighting (IDW) interpolation, local fractal scaling regressions, or directional dispersion voting on 14,000+ points without crashing or freezing.
* **AI capabilities used**:
  Spatial clustering, geochemical anomaly classification, multi-criteria decision trees, and spatial log-transform regressions.
* **Solution**:
  GeoIQ consists of a high-performance C++17 engine that constructs a memory-stable 2D KD-Tree to index samples, performs Inverse Distance Weighting (IDW) to estimate virtual grid compositions, applies Cheng's power-law moving-window fractal regression to calculate local singularity indices ($\alpha$) to isolate true anomalies from natural geogenic noise, and runs a rule engine to classify mineral targets and environmental hazards. A secondary statically-linked tool (`HTMLWriter.exe`) reads these outputs, pairs geology and health coordinates within a 5 km Haversine radius, and generates 3 offline interactive Leaflet.js dashboards.

---

### Section 5 — Responsible AI
* **Risk**:
  1. *Speculative Illegal Mining*: Publicly mapping gold or lithium anomalies can trigger illegal, hazardous, and environmentally destructive wildcat mining by unauthorized individuals.
  2. *False Security & Complacency*: If the system labels a well as safe, users may assume it is free of all hazards, stopping routine testing for other unmeasured pathogens (like bacteria).
* **Mitigation**:
  1. *Information Isolation*: The Tier 1 Public Safety dashboard completely suppresses all geological, mineralogical, and exploration target layers, displaying only environmental hazard alerts.
  2. *Strict Semantic Guardrail*: The word `"SAFE"` is forbidden in the codebase. All background levels are explicitly labeled `"LOW RISK — Background level. Continue routine monitoring"`. In sparse data zones (fewer than 3 nearby points), the system outputs `INSUFFICIENT_DATA` rather than returning a false `LOW` risk.

---

### Section 6 — Data & Build
* **Data source**:
  Unified baseline geochemical dataset compiling the British Geological Survey (BGS) G-BASE mineral database and the United States Geological Survey (USGS) Soil and Sediment Baseline survey (totaling 14,571 sample locations).
* **Build type**:
  Statically-compiled C++17 console application generating standalone, zero-dependency offline Leaflet.js HTML files.

---

### Section 7 — Human Oversight
* **Human role**:
  GeoIQ is strictly designed as a decision-support screening tool. Geologists audit the exact formula derivations and Z-scores displayed on the dashboard before selecting drilling sites. Public health officers use the hazard alerts to target municipal testing budgets and order certified laboratory water tests, leaving human experts in complete control of final operations.

---

### Section 8 — Pitch
* **Pitch**:
  GeoIQ is a C++17 geological intelligence and environmental safety screening engine that maps mineral resource potential for field exploration teams while simultaneously assessing groundwater toxicant hazards (like Arsenic and Lead) to protect vulnerable local communities.

---

## 2. Technical Choices: Why We Used What We Used

| AI / Spatial Method | What It Is | Why We Chose It Over Simpler Alternatives |
| :--- | :--- | :--- |
| **KD-Tree Spatial Indexing** | A multidimensional binary tree that recursively partitions coordinates into halves. | **Why not Grid Search?** Checking a coordinate against all 14,000+ points in a loop requires $O(N^2)$ calculations, freezing local devices. A KD-Tree finds nearest neighbors in logarithmic $O(\log N)$ time, enabling instant offline matching of geology and health samples. |
| **Cheng Singularity (Power-Law)** | A window-based scaling regression that calculates local element enrichment (alpha values). | **Why not Flat Thresholds?** Background concentrations of toxic/mineral elements naturally vary across rock types. Flat thresholds flag false positives in granite areas and miss real anomalies in sandstone. Cheng's singularity isolates local, human-induced plumes from natural baselines. |
| **Inverse Distance Weighting (IDW)** | A spatial interpolation algorithm where closer samples exert stronger mathematical influence. | **Why not Simple Averages?** Contamination plumes disperse physically. Simple averages smooth out sharp peaks, diluting critical warning signals. IDW preserves local hot-spots while ignoring remote, irrelevant sample values. |
| **Offline HTML Generation** | Embedding SQLite data directly into standalone, interactive JavaScript maps. | **Why not standard Web Apps?** Web apps require backend servers (Python/Node.js) and active internet connections, which are absent in remote exploration sites. Offline HTML files open instantly on double-click with zero setup. |

---

## 3. Official WHO Toxicology Reference Limits
The C++ headers and toxicology datasets implement official, actual guidelines:

* **Arsenic (As)**: `0.01 ppm` (10 ppb) in water | `10.0 ppm` in soil (high limit)
* **Lead (Pb)**: `0.01 ppm` (10 ppb) in water | `400.0 ppm` in soil (residential limit)
* **Mercury (Hg)**: `0.006 ppm` (6 ppb) in water | `1.0 ppm` in soil
* **Cadmium (Cd)**: `0.003 ppm` (3 ppb) in water | `70.0 ppm` in soil
* **Chromium (Cr)**: `0.05 ppm` (50 ppb) in water | `100.0 ppm` in soil
* **Fluoride (F)**: `1.5 ppm` in water

---

## 4. How to Load and Swap Datasets
The engine uses configuration (`.ini`) files to dictate what data to load and how to process it:

1. **The Datasets**:
   * `data/raw/samples.csv` — The 60-point mock dataset.
   * `data/raw/samples1.csv` — The 14,571-point real environmental/USGS dataset.
2. **Switching Targets**:
   * Open [config_health.ini](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/config_health.ini) to configure environmental hazard runs. It is pre-configured to load `samples1.csv` (the environmental pollution dataset) and enables Arsenic rules (`enable_arsenic=true`).
   * Open [config_geology.ini](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/config_geology.ini) to configure mineral exploration runs. It points to `samples.csv` and enables Gold, Copper, and Lithium rules.
3. **Regenerating Databases**:
   * Run the pipeline command (see Section 5) with your chosen config file. This updates the SQLite databases in `output/` (`audit.db` and `audit_health.db`).

---

## 5. How to Run the Project
Ensure you run these scripts from a PowerShell terminal inside the project directory.

### Step 1: Run the Backend Engine (GeoIQ)
Processes the raw geochemical records and writes the analytical SQLite databases:
```powershell
# Run exploration pipeline
.\GeoIQ.exe --config config_geology.ini

# Run environmental safety pipeline
.\GeoIQ.exe --config config_health.ini
```

### Step 2: Build the Frontend Generator (HTMLWriter)
Compiles the generator and builds the offline maps. (Skipped automatically if g++ cached objects exist):
```powershell
cd frontend
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Run
cd ..
```

### Step 3: Open the Offline Maps
Open the generated files under `frontend/output/` directly in any web browser:
* **[tier1_public.html](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/frontend/output/tier1_public.html)**: Clean, public health advisory warning map.
* **[tier2_advanced.html](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/frontend/output/tier2_advanced.html)**: Detailed research layer toggle showing Z-scores.
* **[tier3_auditor.html](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/frontend/output/tier3_auditor.html)**: Side-by-side audit sheet, mine guidelines, and interactive SQL terminal.

---

## 6. How to Share or Move the Project Without Errors
* **Static Binary Portability**:
  The frontend `HTMLWriter.exe` has been compiled **statically** (using `-static-libgcc -static-libstdc++ -static`). 
  This means you can copy the `frontend/` directory to another Windows device (e.g. your laptop or your school computer) and it will run **with zero DLL errors**, requiring no separate installations.
* **Double-Clickable Outputs**:
  The generated maps (`tier1_public.html`, etc.) are fully **self-contained**. All records are embedded directly in the HTML file as Javascript constants. You can copy the HTML files to a flash drive or email them; they will open and render interactive maps anywhere, even with no internet access.
* **Directory Structure**:
  If you are copying the entire source directory to compile elsewhere, ensure the sibling directory references remain intact (i.e. `frontend/` stays adjacent to `src/` and `data/` folders), so that the compiler paths match relative paths.
