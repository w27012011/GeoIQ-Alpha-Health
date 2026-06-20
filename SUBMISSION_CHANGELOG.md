# GeoIQ & GeoHealth — Hackathon Submission Changelog
**USAII Global AI Hackathon 2026 — Team GeoIQ-Alpha**

This document details the modifications made to the system just now, the current state of the codebase, and how it aligns with the hackathon's scoring guidelines.

---

## 🛠️ Changes Made Just Now

### 1. Dynamic Tiered Data Serialization
* **File Modified**: [HtmlCommon.h](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/frontend/src/HtmlCommon.h)
* **Action**: Updated `serializeGroupToJS` and `serializeAllGroups` to accept a `tier` filter parameter (`1`, `2`, or `3`).
* **Implementation Details**:
  * **Tier 1 (Public Safety)**: The serializer now completely strips out all geology predictions (`geo` array is empty), suppresses the worst geology risk label (forced to `"LOW"`), removes all mine proximity metrics (no names, distances, or targets), and strips raw spatial Z-scores and formulas from the health evidence objects. This prevents any raw exploration target or geostatistical derivation data from leaking to browser developer tools.
  * **Tier 2 (Advanced Research)**: The serializer strips out all mine proximity and reference fields (`mine` properties) but retains health and geology Z-scores, predictions, and formulas.
  * **Tier 3 (Full Audit)**: The serializer exports the complete dataset including mine cross-references, formulas, Z-scores, and confidence parameters.

### 2. Frontend Popup Purification
* **File Modified**: [HtmlTier1.h](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/frontend/src/HtmlTier1.h)
* **Action**: Removed the "Geological Note" card from the client-side Leaflet marker detail rendering.
* **Result**: Public users viewing the Public Safety Map will never see references to mineral exploration anomalies (like Gold or Copper) or mine coordinates, ensuring zero speculative unauthorized wildcat mining risk.

### 3. Unified Execution Runner
* **File Created**: [run_pipeline.bat](file:///c:/Users/Wilson/Desktop/C++,C,Py/GeoIQ/run_pipeline.bat)
* **Action**: Created a single-click batch script at the root directory to automate the entire process for non-technical judges.
* **Execution Flow**:
  1. Runs the health assessment pipeline on the full 14,571-sample dataset (`Samples_Polution.csv`) to write the SQLite database (`audit_health.db`) and PMTiles.
  2. Runs the geology exploration pipeline on the 60-sample demo dataset (`samples.csv`) to populate mineral prediction targets (`predictions.geojson`).
  3. Executes the standalone statically-linked HTML dashboard generator (`HTMLWriter.exe`).
  4. Automatically opens the generated `tier1_public.html` dashboard in the default browser.

### 4. Compilation & Binary Relocation
* **File Copied**: Relocated `HTMLWriter.exe` from `frontend/output/` or `frontend/` to the root folder to sit adjacent to `GeoIQ.exe` and the batch runner.
* **Verification**: Verified compilation under MinGW C++17 with `-static` links so it executes on clean systems without DLL errors.

### 5. Full Static Recompilation of All Executables
* **Executables Recompiled**: `GeoIQ.exe`, `HTMLWriter.exe`, and `GeoIQFrontend.exe`.
* **Action**: Configured linking with `-static -static-libgcc -static-libstdc++` and linked static `libglfw3.a` for the frontend.
* **Result**: Completely eliminated dependency on dynamic compiler DLLs (`libgcc_s_dw2-1.dll`, `libstdc++-6.dll`, `libmcfgthread-2.dll`) and third-party libraries (`glfw3.dll`). The submission folder is now 100% standalone and portable.

---

## 🗺️ Current State of the System

The system is now **fully ready** for the USAII Global AI Hackathon. The workspace consists of two main parts:

### 1. The High-Performance Geostatistical Engine (`GeoIQ.exe`)
* Written in pure C++17 (compiled with `-O3` optimizations).
* Employs memory-stable **KD-Tree spatial indexing** to resolve geospatial neighborhood searches in $O(\log N)$ time.
* Interpolates concentrations across virtual grid spaces using **Inverse Distance Weighting (IDW)**.
* Executes **Cheng (2007) Power-Law Local Singularity regressions** to detect localized geogenic/anthropogenic spikes.
* Computes **8-sector directional dispersion vectors** to track contamination plumes back to geological sources.
* Compares values against actual WHO environmental toxicant thresholds and outputs SQLite databases and GeoJSON.

### 2. The Interactive Offline HTML Generator (`HTMLWriter.exe`)
* Merges geology predictions and SQLite health records within a **5 km Haversine radius**.
* Compiles **3 distinct HTML dashboards** utilizing Leaflet.js and Canvas rendering to handle tens of thousands of points smoothly.
* Enforces the **Responsible AI info-isolation models** across the three tiers of access (Public, Advanced, Auditor).

---

## 🔬 Alignment with Actual WHO Guidelines

The toxicology model is built on actual, peer-reviewed thresholds defined by the World Health Organization (WHO) and EPA:

1. **Arsenic ($As$)**: Water limit: `0.01 mg/L` (WHO 2022) | Soil limit: `10.0 ppm` (high risk)
2. **Lead ($Pb$)**: Water limit: `0.01 mg/L` (WHO 2017) | Soil limit: `400.0 ppm` (residential)
3. **Mercury ($Hg$)**: Water limit: `0.006 mg/L` (WHO 2017) | Soil limit: `1.0 ppm`
4. **Cadmium ($Cd$)**: Water limit: `0.003 mg/L` (WHO 2022) | Soil limit: `70.0 ppm`
5. **Chromium ($Cr$)**: Water limit: `0.05 mg/L` (WHO 2022) | Soil limit: `100.0 ppm`
6. **Fluoride ($F$)**: Water limit: `1.5 mg/L` (WHO 2017)

These metrics are embedded in the C++ binaries and the client-side JavaScript popup validator to generate direct warnings (e.g., "Arsenic value is 5.3x above WHO limits").
