#ifndef GEOIQ_KNOWNMINES_H
#define GEOIQ_KNOWNMINES_H

#include <string>
#include <vector>

namespace GeoIQ {
    struct SeedMine {
        std::string name;
        std::string state_country;
        double lat;
        double lon;
        std::string classification;
        std::string target_type;
    };

    inline const std::vector<SeedMine> SEED_MINES = {
        // GOLD
        {"Carlin Trend", "Nevada, USA", 40.6700, -116.2300, "Sediment-hosted Carlin-type gold", "GOLD"},
        {"Cortez Mine", "Nevada, USA", 40.2500, -116.6300, "Carlin-type sediment-hosted", "GOLD"},
        {"Round Mountain", "Nevada, USA", 38.7000, -117.0700, "Low-sulfidation epithermal gold", "GOLD"},
        {"Cripple Creek & Victor", "Colorado, USA", 38.7300, -105.1800, "Alkaline intrusion-hosted epithermal", "GOLD"},
        {"Grasberg Mine", "Papua, Indonesia", -4.0500, 137.1100, "Giant Cu-Au porphyry / skarn system", "GOLD"},
        {"Boddington Mine", "Western Australia", -32.7400, 116.3500, "Archean dioritic porphyry gold", "GOLD"},
        {"Muruntau Mine", "Uzbekistan", 41.5000, 64.5700, "Giant orogenic quartz-vein system", "GOLD"},
        {"Oyu Tolgoi", "Mongolia", 43.0070, 106.8430, "Carboniferous Cu-Au porphyry", "GOLD"},
        {"Kalgoorlie Golden Mile", "Western Australia", -30.7700, 121.5000, "Archean shear-hosted orogenic gold", "GOLD"},
        {"Homestake Mine", "South Dakota, USA", 44.3500, -103.7500, "Paleoproterozoic iron-formation hosted", "GOLD"},
        {"Kibali Mine", "DRC", 3.1200, 29.5800, "Archean greenstone-hosted gold", "GOLD"},
        {"Cadia East", "New South Wales, Aus", -33.4600, 149.0100, "Alkalic porphyry Au-Cu system", "GOLD"},

        // COPPER PORPHYRY
        {"Bingham Canyon", "Utah, USA", 40.5230, -112.1510, "Tertiary monzonite porphyry Cu-Mo-Au", "COPPER_PORPHYRY"},
        {"Morenci Mine", "Arizona, USA", 33.0900, -109.3700, "Laramide porphyry Cu-Mo clay leach", "COPPER_PORPHYRY"},
        {"Sierrita Mine", "Arizona, USA", 31.8400, -110.9600, "Mesozoic granodiorite porphyry Cu-Mo", "COPPER_PORPHYRY"},
        {"Bagdad Mine", "Arizona, USA", 34.5900, -113.2000, "Laramide quartz monzonite porphyry", "COPPER_PORPHYRY"},
        {"Resolution Copper", "Arizona, USA", 33.3000, -111.1000, "Deep Laramide supergene Cu-Mo porphyry", "COPPER_PORPHYRY"},
        {"Pinto Valley Mine", "Arizona, USA", 33.3900, -110.9600, "Laramide Schultze Granite porphyry", "COPPER_PORPHYRY"},
        {"Chino Mine (Santa Rita)", "New Mexico, USA", 32.7800, -108.0700, "Laramide quartz-diorite porphyry", "COPPER_PORPHYRY"},
        {"Minera Escondida", "Chile", -24.2700, -69.0700, "Oligocene supergene-enriched porphyry", "COPPER_PORPHYRY"},
        {"Doña Inés de Collahuasi", "Chile", -20.9700, -68.6300, "Oligocene porphyry Cu-Mo system", "COPPER_PORPHYRY"},
        {"El Teniente", "Chile", -34.0800, -70.3500, "Pliocene megabreccia pipe Cu-Mo", "COPPER_PORPHYRY"},
        {"Cerro Verde", "Peru", -16.5300, -71.5800, "Paleocene porphyry copper-moly", "COPPER_PORPHYRY"},
        {"Cananea (Buenavista)", "Sonora, Mexico", 30.9800, -110.2800, "Laramide porphyry Cu-Mo system", "COPPER_PORPHYRY"},
        {"Butte District", "Montana, USA", 46.0100, -112.5300, "Boulder Batholith porphyry-lode Cu-Mo", "COPPER_PORPHYRY"},

        // COPPER VMS
        {"United Verde (Jerome)", "Arizona, USA", 34.7500, -112.1200, "Precambrian rhyolite-hosted massive sulfide", "COPPER_VMS"},
        {"Kidd Creek", "Ontario, Canada", 48.6800, -81.3700, "Archean felsic-volcanic hosted Cu-Zn-Ag", "COPPER_VMS"},
        {"Neves-Corvo", "Portugal", 37.6000, -8.0800, "Iberian Pyrite Belt massive Cu-Sn-Zn", "COPPER_VMS"},
        {"Flin Flon Mine", "Manitoba, Canada", 54.7700, -101.8800, "Proterozoic juvenile arc massive sulfide", "COPPER_VMS"},
        {"Garpenberg", "Sweden", 60.3300, 16.2000, "Felsic volcanic-hosted Zn-Pb-Cu-Ag", "COPPER_VMS"},
        {"Mount Lyell", "Tasmania, Australia", -42.0700, 145.5600, "Cambrian felsic volcanic massive Cu-Au", "COPPER_VMS"},
        {"Matsumine", "Akita, Japan", 40.3200, 140.5500, "Miocene Kuroko-type rhyolitic Zn-Pb-Cu", "COPPER_VMS"},

        // LITHIUM BRINE
        {"Clayton Valley (Silver Peak)", "Nevada, USA", 37.7500, -117.5800, "Quaternary playa brine aquifer", "LITHIUM_BRINE"},
        {"Salton Sea Geothermal", "California, USA", 33.3800, -115.6500, "Geothermal reservoir brine (Fe-Zn-Pb-Li)", "LITHIUM_BRINE"},
        {"Salar de Uyuni", "Bolivia", -20.1300, -67.4800, "Altiplano closed-basin brine", "LITHIUM_BRINE"},
        {"Salar de Atacama", "Chile", -23.5000, -68.2500, "Hyper-arid salar halite brine", "LITHIUM_BRINE"},
        {"Salar de Hombre Muerto", "Argentina", -25.4500, -67.1500, "Puna plateau closed-basin brine", "LITHIUM_BRINE"},
        {"Qaidam Basin playas", "Qinghai, China", 37.2000, 95.2000, "High-magnesium saline lake brine", "LITHIUM_BRINE"},

        // LITHIUM PEGMATITE
        {"Kings Mountain", "North Carolina, USA", 35.2200, -81.3500, "Tin-Spodumene Belt granitic pegmatite", "LITHIUM_PEGMATITE"},
        {"Greenbushes", "Western Australia", -33.8600, 116.0600, "Archean fractionated pegmatite", "LITHIUM_PEGMATITE"},
        {"Manono Mine", "DRC", -7.3000, 27.4000, "Giant LCT pegmatite spodumene sheet", "LITHIUM_PEGMATITE"},
        {"Bikita Mine", "Zimbabwe", -20.1000, 31.4000, "Archean pegmatite (petalite-spodumene)", "LITHIUM_PEGMATITE"},
        {"Wodgina Mine", "Western Australia", -21.1800, 118.6600, "Archean LCT pegmatite (spodumene)", "LITHIUM_PEGMATITE"},
        {"Bernic Lake (Tanco)", "Manitoba, Canada", 50.4300, -95.4500, "LCT pegmatite (pollucite-spodumene)", "LITHIUM_PEGMATITE"},
        {"Pilgangoora", "Western Australia", -21.0200, 118.9100, "Archean greenstone-hosted LCT pegmatite", "LITHIUM_PEGMATITE"},

        // REE CARBONATITE
        {"Mountain Pass Mine", "California, USA", 35.4800, -115.5300, "Proterozoic bastnäsite-bearing carbonatite", "REE_CARBONATITE"},
        {"Bayan Obo", "Inner Mongolia, China", 41.8000, 109.9700, "Giant dolomite-hosted REE-Fe-Nb deposit", "REE_CARBONATITE"},
        {"Mount Weld", "Western Australia", -28.8800, 122.4700, "Carbonatite weathered residuum", "REE_CARBONATITE"},
        {"Lula (Araxa)", "Minas Gerais, Brazil", -19.6300, -46.9400, "Weathered carbonatite (pyrochlore Nb-REE)", "REE_CARBONATITE"},
        {"Maoniuping", "Sichuan, China", 28.3200, 101.9900, "Cenozoic alkaline carbonatite bastnäsite", "REE_CARBONATITE"},
        {"Kvanefjeld", "Greenland", 60.9700, -46.0300, "Ilímaussaq alkaline intrusive REE-U-Zn", "REE_CARBONATITE"},

        // ZINC-LEAD
        {"Viburnum Trend", "Missouri, USA", 37.7200, -91.1300, "Paleozoic dolomite-hosted MVT Pb-Zn", "ZINC_LEAD"},
        {"Red Dog Mine", "Alaska, USA", 68.0700, -162.8800, "Mississippian black shale SEDEX Zn-Pb-Ba", "ZINC_LEAD"},
        {"Mount Isa Mine", "Queensland, Aus", -20.7300, 139.4800, "Proterozoic shale-hosted SEDEX Zn-Pb-Ag", "ZINC_LEAD"},
        {"Sullivan Mine", "British Columbia, Can", 49.7000, -116.0000, "Proterozoic clastic-hosted SEDEX Zn-Pb-Ag", "ZINC_LEAD"},
        {"McArthur River (Here)", "Northern Territory, Aus", -16.4300, 136.1000, "Proterozoic shale-hosted massive SEDEX", "ZINC_LEAD"},
        {"Pine Point", "NWT, Canada", 60.8300, -114.4200, "Devonian carbonate barrier reef MVT Zn-Pb", "ZINC_LEAD"},
        {"Navan (Tara Mine)", "Ireland", 53.6800, -6.6900, "Carboniferous carbonate-hosted Zn-Pb", "ZINC_LEAD"},

        // DIAMOND KIMBERLITE
        {"Kelsey Lake", "Colorado/Wyoming, USA", 40.9900, -105.0000, "Devonian kimberlite pipe cluster", "DIAMOND_KIMBERLITE"},
        {"Kimberley Mine (Big Hole)", "South Africa", -28.7400, 24.7600, "Cretaceous type-kimberlite pipe", "DIAMOND_KIMBERLITE"},
        {"Argyle Diamond Mine", "Western Australia", -16.7100, 128.4500, "Proterozoic olivine lamproite pipe", "DIAMOND_KIMBERLITE"},
        {"Diavik Diamond Mine", "NWT, Canada", 64.5000, -110.2700, "Archean craton kimberlite pipe", "DIAMOND_KIMBERLITE"},
        {"Udachny Pipe", "Yakutia, Russia", 66.4300, 112.3100, "Paleozoic diamondiferous kimberlite", "DIAMOND_KIMBERLITE"},
        {"Jwaneng Diamond Mine", "Botswana", -24.5200, 22.8100, "Permian volcanic kimberlite pipe", "DIAMOND_KIMBERLITE"},
        {"Orapa Diamond Mine", "Botswana", -21.3100, 25.3600, "Cretaceous crater-facies kimberlite", "DIAMOND_KIMBERLITE"},

        // NICKEL-PGE
        {"Stillwater Complex", "Montana, USA", 45.3800, -109.9800, "Archean layered mafic J-M Reef PGE", "NICKEL_PGE"},
        {"Sudbury Basin", "Ontario, Canada", 46.6000, -81.2000, "Astrobleme melt-sheet Ni-Cu-PGE sulfide", "NICKEL_PGE"},
        {"Norilsk-Talnakh", "Siberia, Russia", 69.3500, 88.2000, "Triassic flood-basalt conduit Ni-Cu-PGE", "NICKEL_PGE"},
        {"Bushveld (Merensky Reef)", "South Africa", -25.7000, 27.5000, "Giant Paleoproterozoic layered mafic intrusion", "NICKEL_PGE"},
        {"Jinchuan Mine", "Gansu, China", 38.4800, 102.1700, "Proterozoic ultramafic conduit Ni-Cu", "NICKEL_PGE"},
        {"Raglan Mine", "Quebec, Canada", 61.6800, -73.6800, "Proterozoic komatiitic Ni-Cu-PGE sulfide", "NICKEL_PGE"},
        {"Duluth Complex", "Minnesota, USA", 47.8000, -91.8000, "Keweenawan Midcontinent Rift layered mafic", "NICKEL_PGE"},

        // GOSSAN
        {"Rio Tinto", "Spain", 37.6900, -6.5900, "Giant oxidized cap on massive pyrite VMS", "GOSSAN"},
        {"Broken Hill", "New South Wales, Aus", -31.9600, 141.4500, "Highly weathered quartz-gahnite-goethite cap", "GOSSAN"},
        {"Tsumeb Gossan", "Namibia", -19.2400, 17.7200, "Oxidized pipe zone (Cu-Pb-Zn-As carbonates)", "GOSSAN"},
        {"Mount Isa Gossan", "Queensland, Aus", -20.7300, 139.4800, "Weathered hematite-goethite Pb-Zn cap", "GOSSAN"},
        {"Ducktown District", "Tennessee, USA", 35.0300, -84.3800, "Paleozoic massive pyrrhotite gossan cap", "GOSSAN"}
    };
}

#endif // GEOIQ_KNOWNMINES_H
