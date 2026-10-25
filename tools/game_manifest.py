#!/usr/bin/env python3
"""
ALCHEMY CHAOS - Authoritative Game Database Manifest & Graph Verifier.
Generates data-driven Godot 4 Custom Resource (.tres) files and JSON manifest,
and mathematically proves 100% reachability (zero deadlocks, zero unreachable items).
"""

import json
import os
from collections import deque, Counter

CATEGORIES = [
    "Elements",
    "Minerals",
    "Plants",
    "Metals",
    "Synthetic",
    "Rare",
]

REACTION_TYPES = [
    "BUBBLING",
    "SMOKE",
    "SPARKS",
    "EXPLOSION",
    "CRYSTALLIZATION",
    "FREEZING",
    "MELTING",
    "ELECTRICAL",
    "LIQUID_MIXING",
    "COLOR_TRANSITION",
]

# ==============================================================================
# 50 UNIQUE INGREDIENTS
# ==============================================================================
INGREDIENTS = [
    # --- ELEMENTS (8) ---
    {
        "id": "fire",
        "num": 1,
        "name": "Fire",
        "category": "Elements",
        "starter": True,
        "rarity": "Common",
        "primary": "#FF5E1A",
        "secondary": "#FFD15C",
        "properties": ["Thermal", "Volatile", "Luminous"],
        "rp_value": 0,
        "shape": "flame_trio",
        "description": "Primal combustion dancing with restless caloric vigor, capable of calcining matter and awakening dormant ores.",
    },
    {
        "id": "water",
        "num": 2,
        "name": "Water",
        "category": "Elements",
        "starter": True,
        "rarity": "Common",
        "primary": "#1E88E5",
        "secondary": "#80E5FF",
        "properties": ["Fluid", "Solvent", "Cooling"],
        "rp_value": 0,
        "shape": "droplet_swirl",
        "description": "Distilled spring aqua prized by alchemists as the universal cradle of dissolution, hydration, and life.",
    },
    {
        "id": "earth",
        "num": 3,
        "name": "Earth",
        "category": "Elements",
        "starter": True,
        "rarity": "Common",
        "primary": "#7A5230",
        "secondary": "#C49A6C",
        "properties": ["Solid", "Grounded", "Dense"],
        "rp_value": 0,
        "shape": "earth_clod",
        "description": "Rich mineral-bearing terra heavy with ancient silicates, dormant spores, and metallic veins.",
    },
    {
        "id": "air",
        "num": 4,
        "name": "Air",
        "category": "Elements",
        "starter": True,
        "rarity": "Common",
        "primary": "#98D8F8",
        "secondary": "#F0FAFF",
        "properties": ["Gaseous", "Light", "Oxidizing"],
        "rp_value": 0,
        "shape": "wind_vortex",
        "description": "Swirling atmospheric breath that fans embers into roaring furnaces and lifts vapors toward the heavens.",
    },
    {
        "id": "steam",
        "num": 5,
        "name": "Steam",
        "category": "Elements",
        "starter": False,
        "rarity": "Common",
        "primary": "#DCEAF5",
        "secondary": "#90B4CE",
        "properties": ["Vaporous", "Pressurized", "Thermal"],
        "rp_value": 15,
        "shape": "steam_plume",
        "description": "Superheated scalding vapor born from the union of flame and water, driving condensers and extraction coils.",
    },
    {
        "id": "heat",
        "num": 6,
        "name": "Heat",
        "category": "Elements",
        "starter": False,
        "rarity": "Common",
        "primary": "#FF2A00",
        "secondary": "#FFAE42",
        "properties": ["Caloric", "Intense", "Radiant"],
        "rp_value": 15,
        "shape": "heat_waves",
        "description": "Concentrated thermal energy radiating invisible waves strong enough to liquefy metals and vitrify sand.",
    },
    {
        "id": "ice",
        "num": 7,
        "name": "Ice",
        "category": "Elements",
        "starter": False,
        "rarity": "Common",
        "primary": "#64D2FF",
        "secondary": "#E3FAFF",
        "properties": ["Cryogenic", "Brittle", "Crystalline"],
        "rp_value": 15,
        "shape": "ice_spire",
        "description": "Glacial hexagonal water crystal locked in rigid stillness, arresting volatile reactions on contact.",
    },
    {
        "id": "electricity",
        "num": 8,
        "name": "Electricity",
        "category": "Elements",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#3BF0FF",
        "secondary": "#FFF566",
        "properties": ["Galvanic", "High-Voltage", "Ionizing"],
        "rp_value": 25,
        "shape": "lightning_orb",
        "description": "Crackling storm-born fulgurite charge that splits molecules and awakens galvanic currents in solutions.",
    },

    # --- MINERALS (9) ---
    {
        "id": "mud",
        "num": 9,
        "name": "Mud",
        "category": "Minerals",
        "starter": False,
        "rarity": "Common",
        "primary": "#5E3F24",
        "secondary": "#966B43",
        "properties": ["Malleable", "Damp", "Fertile"],
        "rp_value": 15,
        "shape": "mud_mound",
        "description": "Slick alluvial mire teeming with dissolved minerals and primed for both ceramic shaping and botanical growth.",
    },
    {
        "id": "stone",
        "num": 10,
        "name": "Stone",
        "category": "Minerals",
        "starter": False,
        "rarity": "Common",
        "primary": "#6E7682",
        "secondary": "#ADB6C4",
        "properties": ["Hard", "Inert", "Mineral"],
        "rp_value": 15,
        "shape": "stone_monolith",
        "description": "Dense granite-basalt aggregate forged under crust pressure, serving as foundation rock and raw ore host.",
    },
    {
        "id": "lava",
        "num": 11,
        "name": "Lava",
        "category": "Minerals",
        "starter": False,
        "rarity": "Common",
        "primary": "#E63900",
        "secondary": "#FFBF00",
        "properties": ["Magmatic", "Incandescent", "Viscous"],
        "rp_value": 18,
        "shape": "lava_crater",
        "description": "Molten subterranean rock veined with blinding orange fissures that cools into volcanic stone and glass.",
    },
    {
        "id": "sand",
        "num": 12,
        "name": "Sand",
        "category": "Minerals",
        "starter": False,
        "rarity": "Common",
        "primary": "#E2BE78",
        "secondary": "#FFF0C2",
        "properties": ["Granular", "Silicate", "Dry"],
        "rp_value": 18,
        "shape": "sand_dune",
        "description": "Fine golden silica grains eroded from ancient stone, essential for blowing clear laboratory glassware.",
    },
    {
        "id": "clay",
        "num": 13,
        "name": "Clay",
        "category": "Minerals",
        "starter": False,
        "rarity": "Common",
        "primary": "#B86443",
        "secondary": "#E39B7D",
        "properties": ["Plastic", "Refractory", "Absorbent"],
        "rp_value": 18,
        "shape": "clay_tablet",
        "description": "Smooth terracotta kaolin paste that holds intricate impressions and fires into heatproof vessels.",
    },
    {
        "id": "sulfur",
        "num": 14,
        "name": "Brimstone Sulfur",
        "category": "Minerals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#E6D219",
        "secondary": "#FFF98C",
        "properties": ["Combustible", "Pungent", "Tria-Prima"],
        "rp_value": 24,
        "shape": "sulfur_cluster",
        "description": "Bright canary-yellow volcanic crystal representing the fiery Soul in Paracelsian Tria Prima theory.",
    },
    {
        "id": "salt",
        "num": 15,
        "name": "Alchemical Salt",
        "category": "Minerals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#EDF4FA",
        "secondary": "#B4CBE0",
        "properties": ["Ionic", "Crystalline", "Tria-Prima"],
        "rp_value": 24,
        "shape": "salt_cubes",
        "description": "Cubic halite crystals symbolizing the fixed physical Body in the alchemical triad of transmutation.",
    },
    {
        "id": "quartz",
        "num": 16,
        "name": "Clear Quartz",
        "category": "Minerals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#D8EEFF",
        "secondary": "#9BE2FF",
        "properties": ["Piezoelectric", "Prismatic", "Resonant"],
        "rp_value": 26,
        "shape": "quartz_prism",
        "description": "Six-sided hexagonal rock crystal that focuses luminous beams and vibrates under electrical charge.",
    },
    {
        "id": "obsidian",
        "num": 17,
        "name": "Obsidian",
        "category": "Minerals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#261E36",
        "secondary": "#7A5AF5",
        "properties": ["Vitreous", "Razor-Edged", "Volcanic"],
        "rp_value": 28,
        "shape": "obsidian_blade",
        "description": "Jet-black volcanic glass quenched in sudden frost, gleaming with violet conchoidal fractures.",
    },

    # --- PLANTS (8) ---
    {
        "id": "moss",
        "num": 18,
        "name": "Verdant Moss",
        "category": "Plants",
        "starter": False,
        "rarity": "Common",
        "primary": "#4A9E3C",
        "secondary": "#9CE87A",
        "properties": ["Organic", "Humid", "Living"],
        "rp_value": 18,
        "shape": "moss_stone",
        "description": "Velvety emerald bryophyte cushion that awakens the first spark of botanical life from damp stone.",
    },
    {
        "id": "herb",
        "num": 19,
        "name": "Silverleaf Herb",
        "category": "Plants",
        "starter": False,
        "rarity": "Common",
        "primary": "#3EB874",
        "secondary": "#B9F6CA",
        "properties": ["Botanical", "Aromatic", "Restorative"],
        "rp_value": 22,
        "shape": "herb_sprig",
        "description": "Serrated apothecary foliage shimmering with silvery essential oils used as the base of herbal tinctures.",
    },
    {
        "id": "sunpetal",
        "num": 20,
        "name": "Sunpetal Blossom",
        "category": "Plants",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#FF9E1B",
        "secondary": "#FFF078",
        "properties": ["Solar", "Luminous", "Vital"],
        "rp_value": 28,
        "shape": "sun_flower",
        "description": "Golden heliotropic bloom whose petals retain concentrated daylight warmth even at midnight.",
    },
    {
        "id": "frostbloom",
        "num": 21,
        "name": "Frostbloom Lily",
        "category": "Plants",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#6FE4FF",
        "secondary": "#E8FBFF",
        "properties": ["Cryogenic", "Preserving", "Delicate"],
        "rp_value": 28,
        "shape": "frost_lily",
        "description": "Translucent ice-blue alpine flower that exhales freezing mist and never withers in winter.",
    },
    {
        "id": "glowshroom",
        "num": 22,
        "name": "Lumen Mycelium",
        "category": "Plants",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#1CE8B5",
        "secondary": "#85FFEA",
        "properties": ["Bioluminescent", "Fungal", "Catalytic"],
        "rp_value": 28,
        "shape": "glow_mushrooms",
        "description": "Clustered subterranean fungi whose gills pulse with eerie cyan bioluminescence and reactive spores.",
    },
    {
        "id": "nightshade",
        "num": 23,
        "name": "Shadow Nightshade",
        "category": "Plants",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#6C22A6",
        "secondary": "#D17BFF",
        "properties": ["Toxic", "Alkaloid", "Umbral"],
        "rp_value": 32,
        "shape": "nightshade_berry",
        "description": "Glossy dark-purple belladonna berries steeped in potent alkaloids and nocturnal essence.",
    },
    {
        "id": "mandrake",
        "num": 24,
        "name": "Mandragora Root",
        "category": "Plants",
        "starter": False,
        "rarity": "Rare",
        "primary": "#8D5B34",
        "secondary": "#7CE854",
        "properties": ["Vital-Root", "Arcane", "Potent"],
        "rp_value": 38,
        "shape": "mandrake_root",
        "description": "Gnarled rootstock crowned with vibrant leaves, prized in high alchemy for binding life-force to elixirs.",
    },
    {
        "id": "amber_resin",
        "num": 25,
        "name": "Golden Amber Sap",
        "category": "Plants",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#E88B14",
        "secondary": "#FFE082",
        "properties": ["Resinous", "Binding", "Electrostatic"],
        "rp_value": 28,
        "shape": "amber_teardrop",
        "description": "Fossilized golden botanical resin famous since antiquity for trapping light and static charge.",
    },

    # --- METALS (9) ---
    {
        "id": "metal",
        "num": 26,
        "name": "Crude Metal",
        "category": "Metals",
        "starter": False,
        "rarity": "Common",
        "primary": "#8492A6",
        "secondary": "#DAE3F0",
        "properties": ["Conductive", "Metallic", "Dense"],
        "rp_value": 20,
        "shape": "ore_nugget",
        "description": "Smelted metallic bloom liberated from stone by fierce flame, ready to be refined into noble and base metals.",
    },
    {
        "id": "molten_metal",
        "num": 27,
        "name": "Molten Metal",
        "category": "Metals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#FF6D00",
        "secondary": "#FFF394",
        "properties": ["Incandescent", "Fluid-Metal", "Alloying"],
        "rp_value": 26,
        "shape": "crucible_pour",
        "description": "Blinding orange-white liquid metal swirling inside a refractory crucible, eager to fuse into new alloys.",
    },
    {
        "id": "iron",
        "num": 28,
        "name": "Forged Iron",
        "category": "Metals",
        "starter": False,
        "rarity": "Common",
        "primary": "#525D6B",
        "secondary": "#A9B5C4",
        "properties": ["Ferrous", "Magnetic", "Martial"],
        "rp_value": 24,
        "shape": "iron_anvil_ingot",
        "description": "Sturdy dark-grey martial metal ruled by Mars, exhibiting strong magnetism and structural resilience.",
    },
    {
        "id": "copper",
        "num": 29,
        "name": "Red Copper",
        "category": "Metals",
        "starter": False,
        "rarity": "Common",
        "primary": "#D46A38",
        "secondary": "#FFB78A",
        "properties": ["Cupric", "Highly-Conductive", "Venusian"],
        "rp_value": 24,
        "shape": "copper_coil",
        "description": "Lustrous salmon-red metal of Venus fashioned into alembic stills, lightning rods, and galvanic coils.",
    },
    {
        "id": "silver",
        "num": 30,
        "name": "Lunar Silver",
        "category": "Metals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#C7D8EB",
        "secondary": "#FFFFFF",
        "properties": ["Noble", "Reflective", "Lunar"],
        "rp_value": 32,
        "shape": "silver_crescent",
        "description": "Mirror-pure noble metal attuned to Luna, resisting corrosion and channeling subtle astral reflections.",
    },
    {
        "id": "lead",
        "num": 31,
        "name": "Saturnine Lead",
        "category": "Metals",
        "starter": False,
        "rarity": "Common",
        "primary": "#474B57",
        "secondary": "#7F8699",
        "properties": ["Heavy", "Shielding", "Saturnine"],
        "rp_value": 24,
        "shape": "lead_plumb",
        "description": "Ponderous slate-dark metal of Saturn, traditionally chosen as the humble base matter for chrysopoeia.",
    },
    {
        "id": "quicksilver",
        "num": 32,
        "name": "Quicksilver",
        "category": "Metals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#B5C9DE",
        "secondary": "#F2F8FF",
        "properties": ["Fluid-Metal", "Volatile", "Tria-Prima"],
        "rp_value": 34,
        "shape": "mercury_flask",
        "description": "Living liquid mercury representing the volatile Spirit in the Tria Prima, dissolving metals on touch.",
    },
    {
        "id": "bronze",
        "num": 33,
        "name": "Alchemical Bronze",
        "category": "Metals",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#B57C36",
        "secondary": "#F2CA85",
        "properties": ["Alloy", "Resonant", "Impervious"],
        "rp_value": 30,
        "shape": "bronze_gear",
        "description": "Sonorous golden-brown alloy cast into precision astrolabes, pressure valves, and clockwork gears.",
    },
    {
        "id": "gold",
        "num": 34,
        "name": "Solar Gold",
        "category": "Metals",
        "starter": False,
        "rarity": "Rare",
        "primary": "#FFB800",
        "secondary": "#FFF6A6",
        "properties": ["Auric", "Incorruptible", "Solar"],
        "rp_value": 45,
        "shape": "gold_sun_ingot",
        "description": "Tarnishless royal metal of Sol, sought for millennia as perfection in metallic nature.",
    },

    # --- SYNTHETIC COMPOUNDS (9) ---
    {
        "id": "thermal_shock",
        "num": 35,
        "name": "Thermal Shock",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#FF477E",
        "secondary": "#5CE1FF",
        "properties": ["Kinetic", "Fracturing", "Bipolar"],
        "rp_value": 26,
        "shape": "thermal_split_sphere",
        "description": "Violent stress-wave unleashed when extreme frost collides with roaring flame, shattering rigid lattices.",
    },
    {
        "id": "charged_water",
        "num": 36,
        "name": "Charged Water",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#00B8FF",
        "secondary": "#9EFFFF",
        "properties": ["Galvanic", "Electrolyte", "Ionized"],
        "rp_value": 28,
        "shape": "leyden_jar",
        "description": "Luminous electrolyzed aqua surging with dissolved ions and miniature submerged lightning arcs.",
    },
    {
        "id": "glass",
        "num": 37,
        "name": "Alchemical Glass",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Common",
        "primary": "#A8E6FF",
        "secondary": "#FFFFFF",
        "properties": ["Transparent", "Impermeable", "Refractive"],
        "rp_value": 24,
        "shape": "alembic_retort",
        "description": "Crystal-clear borosilicate glass blown into retorts and flasks impervious to boiling acids.",
    },
    {
        "id": "ceramic",
        "num": 38,
        "name": "Refractory Porcelain",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Common",
        "primary": "#EDE6DC",
        "secondary": "#C4B4A2",
        "properties": ["Heatproof", "Insulating", "Vitrified"],
        "rp_value": 24,
        "shape": "porcelain_mortar",
        "description": "High-fired white ceramic crucible ware engineered to withstand white-hot plasma and corrosive salts.",
    },
    {
        "id": "ash",
        "num": 39,
        "name": "Calcined Ash",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Common",
        "primary": "#8D929C",
        "secondary": "#D6DAE2",
        "properties": ["Alkaline", "Potash", "Purified"],
        "rp_value": 22,
        "shape": "ash_urn",
        "description": "Fine pale potash powder left after organic matter undergoes complete alchemical calcination.",
    },
    {
        "id": "acid",
        "num": 40,
        "name": "Vitriol Acid",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#5BE82A",
        "secondary": "#D2FF8A",
        "properties": ["Corrosive", "Caustic", "Dissolving"],
        "rp_value": 34,
        "shape": "acid_flask",
        "description": "Fuming emerald Oil of Vitriol distilled from sulfur vapors, biting fiercely into base metals and stone.",
    },
    {
        "id": "gunpowder",
        "num": 41,
        "name": "Black Powder",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#38353B",
        "secondary": "#FF7B29",
        "properties": ["Explosive", "Deflagrating", "Granular"],
        "rp_value": 32,
        "shape": "powder_horn",
        "description": "Volatile ternary mixture of brimstone, salt, and calcined ash that erupts in thunderous scarlet fire.",
    },
    {
        "id": "amalgam",
        "num": 42,
        "name": "Mercuric Amalgam",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Uncommon",
        "primary": "#D4C6F0",
        "secondary": "#FFE399",
        "properties": ["Amalgamated", "Catalytic", "Plastic-Metal"],
        "rp_value": 36,
        "shape": "amalgam_swirl",
        "description": "Silvery-gold metallic paste formed when quicksilver embraces solid metal in cold dissolution.",
    },
    {
        "id": "aqua_regia",
        "num": 43,
        "name": "Aqua Regia",
        "category": "Synthetic",
        "starter": False,
        "rarity": "Rare",
        "primary": "#FF6A1A",
        "secondary": "#FFE047",
        "properties": ["Royal-Solvent", "Fuming", "Transmutative"],
        "rp_value": 42,
        "shape": "royal_vial",
        "description": "Smoking amber-crimson Royal Water - the fabled synthetic acid potent enough to dissolve pure Solar Gold.",
    },

    # --- RARE SUBSTANCES (7) ---
    {
        "id": "aether",
        "num": 44,
        "name": "Luminous Aether",
        "category": "Rare",
        "starter": False,
        "rarity": "Rare",
        "primary": "#A66BFF",
        "secondary": "#78F6FF",
        "properties": ["Quintessential", "Weightless", "Astral"],
        "rp_value": 55,
        "shape": "aether_nebula",
        "description": "Iridescent fifth element shimmering between light and vapor, bridging terrestrial matter with celestial law.",
    },
    {
        "id": "plasma",
        "num": 45,
        "name": "Contained Plasma",
        "category": "Rare",
        "starter": False,
        "rarity": "Rare",
        "primary": "#DF3BFF",
        "secondary": "#7AF8FF",
        "properties": ["Superheated", "Ionized-Core", "Magnetic"],
        "rp_value": 55,
        "shape": "plasma_sphere",
        "description": "Magnetically suspended miniature star-core pulsing with violet-white arcs of fourth-state matter.",
    },
    {
        "id": "cryo_crystal",
        "num": 46,
        "name": "Absolute Zero Shard",
        "category": "Rare",
        "starter": False,
        "rarity": "Rare",
        "primary": "#1CA3FF",
        "secondary": "#EBFFFF",
        "properties": ["Subzero", "Entropic", "Perfect-Lattice"],
        "rp_value": 58,
        "shape": "cryo_obelisk",
        "description": "Hyper-ordered sapphire lattice chilled near total stillness, freezing ambient vapor into diamond dust.",
    },
    {
        "id": "phoenix_ember",
        "num": 47,
        "name": "Phoenix Cinder",
        "category": "Rare",
        "starter": False,
        "rarity": "Rare",
        "primary": "#FF2247",
        "secondary": "#FFE45E",
        "properties": ["Immortal-Flame", "Self-Renewing", "Rubedo"],
        "rp_value": 62,
        "shape": "phoenix_feather_flame",
        "description": "Plume-shaped ruby ember that rekindles from its own ashes, radiating inexhaustible solar vitality.",
    },
    {
        "id": "void_essence",
        "num": 48,
        "name": "Abyssal Nigredo",
        "category": "Rare",
        "starter": False,
        "rarity": "Rare",
        "primary": "#240B47",
        "secondary": "#B24BFF",
        "properties": ["Nigredo", "Null-Light", "Primordial"],
        "rp_value": 65,
        "shape": "void_singularity",
        "description": "Velvet-black distillate of the Great Putrefaction that drinks stray light and strips matter to pure potential.",
    },
    {
        "id": "elixir_vitae",
        "num": 49,
        "name": "Elixir Vitae",
        "category": "Rare",
        "starter": False,
        "rarity": "Rare",
        "primary": "#FF2E7E",
        "secondary": "#7DFFBE",
        "properties": ["Panacea", "Albedo-Rubedo", "Immortal"],
        "rp_value": 75,
        "shape": "elixir_chalice",
        "description": "Opalescent rose-and-emerald draught of universal renewal that mends decay and harmonizes opposing elements.",
    },
    {
        "id": "philosophers_stone",
        "num": 50,
        "name": "Philosopher's Stone",
        "category": "Rare",
        "starter": False,
        "rarity": "Mythic",
        "primary": "#FF1744",
        "secondary": "#FFD700",
        "properties": ["Magnum-Opus", "Omnipotent", "Transmutative"],
        "rp_value": 150,
        "shape": "philosopher_stone",
        "description": "The Magnum Opus incarnate: a self-luminous crimson-gold hyper-crystal uniting Nigredo, Albedo, Citrinitas, and Rubedo.",
    },
]

# ==============================================================================
# 120 UNIQUE VALID RECIPES (2-ingredient + 3-ingredient multi-stage + hidden + rare)
# Every unordered input tuple is unique!
# ==============================================================================
RAW_RECIPES = [
    # --- Core Starter & Prompt-Mandated Examples ---
    (["fire", "water"], "steam", "SMOKE", False, False, 15, "Boiling aqua erupts into pressurized white vapor."),
    (["water", "earth"], "mud", "LIQUID_MIXING", False, False, 15, "Dry soil drinks spring water to form rich malleable mire."),
    (["heat", "metal"], "molten_metal", "MELTING", False, False, 22, "Fierce caloric waves liquefy solid metal into a glowing stream."),
    (["ice", "fire"], "thermal_shock", "EXPLOSION", False, False, 24, "Sudden extreme temperature contrast unleashes a shattering shockwave."),
    (["electricity", "water"], "charged_water", "ELECTRICAL", False, False, 24, "Galvanic arcs ionize pure aqua into crackling electrolyte."),

    # --- Elemental Foundations ---
    (["fire", "fire"], "heat", "MELTING", False, False, 14, "Twin flames intensify into pure radiant thermal energy."),
    (["fire", "air"], "heat", "SPARKS", False, False, 14, "Oxygen feeds combustion into a roaring blast of heat."),
    (["water", "air"], "ice", "FREEZING", False, False, 15, "Biting high-altitude winds chill water droplets into glacial ice."),
    (["steam", "air"], "electricity", "ELECTRICAL", False, False, 22, "Colliding storm-vapors build static charge until lightning leaps forth."),
    (["earth", "earth"], "stone", "CRYSTALLIZATION", False, False, 15, "Compacted strata fuse under weight into solid bedrock."),
    (["earth", "fire"], "lava", "MELTING", False, False, 16, "Subterranean flame melts terra into incandescent magma."),
    (["lava", "water"], "stone", "SMOKE", False, False, 16, "Quenched magma hisses violently and hardens into basalt."),
    (["mud", "fire"], "stone", "SMOKE", False, False, 16, "Baked mire hardens into dry sun-cracked rock."),
    (["stone", "heat"], "lava", "MELTING", False, False, 18, "Sustained furnace heat returns solid rock to molten magma."),
    (["steam", "ice"], "water", "LIQUID_MIXING", False, False, 12, "Hot vapor condenses against glacial ice into pure liquid water."),

    # --- Mineralogy & Geochemistry ---
    (["stone", "air"], "sand", "SPARKS", False, False, 16, "Relentless wind scours bedrock into golden silica grains."),
    (["thermal_shock", "stone"], "sand", "EXPLOSION", False, False, 20, "Thermal fracture shatters solid boulders into fine sand."),
    (["mud", "sand"], "clay", "LIQUID_MIXING", False, False, 18, "Silica grit and wet mire knead together into smooth potter's clay."),
    (["mud", "earth"], "clay", "LIQUID_MIXING", False, False, 18, "Refined alluvial earth settles into dense kaolin clay."),
    (["lava", "air"], "sulfur", "SMOKE", False, False, 20, "Volcanic fumaroles deposit bright yellow brimstone crystals."),
    (["fire", "stone"], "metal", "MELTING", False, False, 20, "Smelting stone in direct flame sweats beads of crude metal from ore."),
    (["lava", "earth"], "metal", "MELTING", False, False, 20, "Deep magmatic veins concentrate metallic ore blooms."),
    (["steam", "stone"], "salt", "CRYSTALLIZATION", False, False, 20, "Mineral-laden vapor evaporates to leave cubic halite crystals."),
    (["water", "sand"], "salt", "CRYSTALLIZATION", False, False, 20, "Briny coastal sand yields sparkling white salt upon leaching."),
    (["sand", "ice"], "quartz", "CRYSTALLIZATION", False, False, 22, "Slow ordered cooling coaxing silica into hexagonal rock crystal."),
    (["stone", "electricity"], "quartz", "ELECTRICAL", False, False, 24, "Piezoelectric discharge aligns mineral silica into clear quartz."),
    (["lava", "ice"], "obsidian", "FREEZING", False, False, 24, "Instant cryogenic quenching freezes magma into razor-sharp black glass."),
    (["lava", "thermal_shock"], "obsidian", "EXPLOSION", False, False, 26, "Shock-cooled volcanic flow vitrifies into mirror-dark obsidian."),
    (["sulfur", "stone"], "lava", "MELTING", False, False, 18, "Burning brimstone flux lowers rock's melting point into magma."),
    (["quartz", "thermal_shock"], "sand", "EXPLOSION", False, False, 18, "Shattered crystal prisms crumble into sparkling silica sand."),

    # --- Arcane Botany (Plants) ---
    (["mud", "air"], "moss", "COLOR_TRANSITION", False, False, 18, "Airborne spores take root across damp mire as emerald moss."),
    (["stone", "water"], "moss", "BUBBLING", False, False, 18, "River-washed stone sprouts a velvety coat of living moss."),
    (["moss", "earth"], "herb", "COLOR_TRANSITION", False, False, 20, "Rooted moss matures in rich soil into fragrant silverleaf herbs."),
    (["moss", "water"], "herb", "LIQUID_MIXING", False, False, 20, "Hydrated bryophytes unfurl into medicinal apothecary leaves."),
    (["herb", "heat"], "sunpetal", "COLOR_TRANSITION", False, False, 24, "Warm caloric radiance coaxes silverleaf buds into golden sunpetals."),
    (["moss", "sunpetal"], "herb", "COLOR_TRANSITION", False, False, 18, "Solar pollen invigorates humble moss into lush herbal shoots."),
    (["herb", "ice"], "frostbloom", "FREEZING", False, False, 24, "Glacial rime crystallizes living foliage into an immortal frostbloom."),
    (["moss", "ice"], "frostbloom", "FREEZING", False, False, 24, "Frozen tundra moss blossoms into delicate blue ice-lilies."),
    (["moss", "mud"], "glowshroom", "BUBBLING", False, False, 24, "Deep boggy humus nurtures bioluminescent cavern mycelium."),
    (["moss", "electricity"], "glowshroom", "ELECTRICAL", False, False, 26, "Bio-galvanic current awakens glowing cyan fungal caps."),
    (["herb", "sulfur"], "nightshade", "SMOKE", False, False, 26, "Sulfurous fumes twist gentle herbs into venomous purple nightshade."),
    (["glowshroom", "ash"], "nightshade", "COLOR_TRANSITION", True, False, 30, "Alkaline ash and cavern spores mutate into potent shadow berries."),
    (["herb", "clay"], "mandrake", "COLOR_TRANSITION", False, False, 30, "Molded terracotta and living roots intertwine into sentient mandragora."),
    (["nightshade", "mud"], "mandrake", "BUBBLING", False, False, 30, "Alkaloid berries buried in fertile mire sprout a knotted mandrake root."),
    (["sunpetal", "clay"], "amber_resin", "CRYSTALLIZATION", False, False, 26, "Solar nectar sealed in clay hardens into translucent golden amber."),
    (["herb", "steam"], "amber_resin", "BUBBLING", False, False, 24, "Steam-distilled botanical sap condenses into fragrant amber teardrops."),
    (["moss", "fire"], "ash", "SMOKE", False, False, 18, "Dry moss calcines in seconds into pale alkaline potash."),
    (["herb", "fire"], "ash", "SMOKE", False, False, 18, "Apothecary leaves burn down to purified white mineral ash."),

    # --- Metallurgy & Chrysopoeia ---
    (["metal", "earth"], "iron", "SPARKS", False, False, 22, "Earthy carbon tempers crude bloom into magnetic forged iron."),
    (["molten_metal", "water"], "iron", "SMOKE", False, False, 22, "Quenching white-hot metal in water tempers resilient dark iron."),
    (["metal", "air"], "copper", "SPARKS", False, False, 22, "Oxidized smelting yields warm salmon-red Venusian copper."),
    (["metal", "electricity"], "copper", "ELECTRICAL", False, False, 24, "Current-drawn refining isolates pure conductive copper."),
    (["metal", "ice"], "silver", "FREEZING", False, False, 28, "Cold lunar cupellation refines metal into mirror-bright silver."),
    (["iron", "quartz"], "silver", "CRYSTALLIZATION", True, False, 30, "Prismatic resonance purifies base iron toward noble lunar sheen."),
    (["metal", "mud"], "lead", "LIQUID_MIXING", False, False, 22, "Heavy earthy dross settles into dense Saturnine lead."),
    (["iron", "stone"], "lead", "MELTING", False, False, 22, "Slag-heavy smelting yields ponderous dull-grey lead."),
    (["metal", "water"], "quicksilver", "LIQUID_MIXING", False, False, 28, "Fluid metallic principle awakens as shimmering liquid mercury."),
    (["silver", "molten_metal"], "quicksilver", "MELTING", False, False, 30, "Noble silver dissolved in liquid heat flows as living quicksilver."),
    (["lead", "charged_water"], "quicksilver", "ELECTRICAL", True, False, 32, "Galvanic electrolysis liberates fluid mercury from heavy lead."),
    (["copper", "molten_metal"], "bronze", "MELTING", False, False, 26, "Red copper alloyed in molten metal casts sonorous golden bronze."),
    (["copper", "iron"], "bronze", "SPARKS", False, False, 26, "Forged bimetallic fusion yields corrosion-proof alchemical bronze."),
    (["quicksilver", "sulfur"], "gold", "COLOR_TRANSITION", True, True, 45, "Classical Paracelsian union of Mercury and Brimstone births Solar Gold!"),
    (["silver", "sunpetal"], "gold", "COLOR_TRANSITION", True, True, 45, "Lunar silver steeped in concentrated solar essence ripens into pure Gold!"),
    (["iron", "heat"], "molten_metal", "MELTING", False, False, 20, "Red-hot forged iron surrenders its shape back into liquid metal."),
    (["copper", "heat"], "molten_metal", "MELTING", False, False, 20, "Heated copper coils melt into a glowing orange pool."),
    (["lead", "heat"], "molten_metal", "MELTING", False, False, 18, "Low-melting Saturnine lead liquefies swiftly over the burner."),

    # --- Synthetic Laboratory Chemistry ---
    (["sand", "heat"], "glass", "MELTING", False, False, 22, "Silica grains fuse into crystal-clear borosilicate laboratory glass."),
    (["sand", "fire"], "glass", "MELTING", False, False, 22, "Roaring flame vitrifies golden sand into transparent glass."),
    (["quartz", "molten_metal"], "glass", "MELTING", False, False, 24, "Crushed quartz fluxed on molten tin forms flawless optical glass."),
    (["clay", "heat"], "ceramic", "MELTING", False, False, 22, "Kiln-fired kaolin vitrifies into heatproof white porcelain."),
    (["clay", "fire"], "ceramic", "SMOKE", False, False, 22, "Terracotta clay hardens in open flame into refractory ceramic."),
    (["sulfur", "charged_water"], "acid", "BUBBLING", False, False, 30, "Electrolyzed brimstone fumes condense into caustic green Oil of Vitriol."),
    (["sulfur", "steam"], "acid", "BUBBLING", False, False, 28, "Hot sulfurous vapor condenses in glass coils as biting Vitriol Acid."),
    (["sulfur", "ash"], "gunpowder", "SPARKS", False, False, 28, "Brimstone ground with alkaline potash forms volatile black powder."),
    (["sulfur", "salt"], "gunpowder", "SPARKS", False, False, 28, "Crystalline saltpeter and sulfur blend into deflagrating powder."),
    (["gunpowder", "fire"], "thermal_shock", "EXPLOSION", False, False, 26, "Ignited black powder detonates in a concussive blast wave!"),
    (["quicksilver", "copper"], "amalgam", "LIQUID_MIXING", False, False, 30, "Liquid mercury drinks red copper into a pliable metallic amalgam."),
    (["quicksilver", "silver"], "amalgam", "LIQUID_MIXING", False, False, 32, "Silver dissolves effortlessly in mercury to form mirror amalgam."),
    (["quicksilver", "iron"], "amalgam", "ELECTRICAL", False, False, 30, "Forced galvanic wetting binds reluctant iron with quicksilver."),
    (["acid", "salt"], "aqua_regia", "BUBBLING", False, True, 40, "Vitriol and halite distill into fuming orange-gold Royal Water!"),
    (["acid", "charged_water"], "aqua_regia", "ELECTRICAL", True, True, 40, "Hyper-ionized acid turns amber-red with the power to dissolve gold."),
    (["acid", "metal"], "salt", "BUBBLING", False, False, 22, "Caustic vitriol dissolves crude metal, precipitating mineral salt."),
    (["acid", "ash"], "salt", "BUBBLING", False, False, 20, "Acid and alkaline ash neutralize in fizzing effervescence to yield salt."),
    (["amber_resin", "air"], "electricity", "ELECTRICAL", True, False, 26, "Rubbed golden Elektron resin crackles with ancient static sparks!"),
    (["charged_water", "ice"], "cryo_crystal", "FREEZING", True, True, 50, "High-voltage supercooling locks water into an Absolute Zero lattice!"),

    # --- Rare & Mythic Transmutations (2-Ingredient Paths) ---
    (["electricity", "quartz"], "aether", "CRYSTALLIZATION", False, True, 50, "High-frequency crystal resonance sublimates energy into Luminous Aether!"),
    (["charged_water", "glass"], "aether", "COLOR_TRANSITION", True, True, 50, "Sealed Leyden glow distills weightless quintessence inside pure glass."),
    (["electricity", "heat"], "plasma", "ELECTRICAL", False, True, 50, "Superheated lightning strips electrons away into blazing violet Plasma!"),
    (["electricity", "molten_metal"], "plasma", "ELECTRICAL", False, True, 52, "Arc-furnace discharge ignites a self-contained magnetic plasma orb."),
    (["fire", "charged_water"], "plasma", "EXPLOSION", True, True, 52, "Combustion inside ionized aqua flashes into fourth-state plasma."),
    (["frostbloom", "obsidian"], "cryo_crystal", "FREEZING", False, True, 52, "Alpine frost-essence and volcanic glass fuse into an Absolute Zero Shard!"),
    (["ice", "aether"], "cryo_crystal", "CRYSTALLIZATION", False, True, 54, "Quintessential cold perfects glacial ice into an unmelting sapphire obelisk."),
    (["sunpetal", "plasma"], "phoenix_ember", "EXPLOSION", False, True, 58, "Living solar petals ignited by star-plasma awaken an immortal Phoenix Cinder!"),
    (["gold", "gunpowder"], "phoenix_ember", "EXPLOSION", True, True, 58, "Auric detonation leaves behind a self-renewing crimson-gold ember."),
    (["sunpetal", "lava"], "phoenix_ember", "MELTING", False, True, 55, "Botanical sunlight survives magmatic fire by rebirthing as a Phoenix Cinder."),
    (["obsidian", "nightshade"], "void_essence", "COLOR_TRANSITION", True, True, 60, "Volcanic black glass and midnight poison distill into Abyssal Nigredo!"),
    (["lead", "aether"], "void_essence", "SMOKE", False, True, 60, "Saturnine heaviness dissolved in quintessence yields pure Nigredo darkness."),
    (["mandrake", "gold"], "elixir_vitae", "COLOR_TRANSITION", False, True, 70, "Auric incorruptibility bound to vital mandragora blooms into Elixir Vitae!"),
    (["aether", "mandrake"], "elixir_vitae", "BUBBLING", True, True, 70, "Quintessence infused with root-of-life distills the panacea of immortality."),
    (["void_essence", "elixir_vitae"], "philosophers_stone", "COLOR_TRANSITION", True, True, 150, "Nigredo and Rubedo unite! Darkness and Immortal Life crystallize into the Philosopher's Stone!"),
    (["aqua_regia", "phoenix_ember"], "philosophers_stone", "CRYSTALLIZATION", True, True, 140, "Royal Water and Immortal Flame Fuse into the supreme crimson Philosopher's Stone!"),
    (["philosophers_stone", "lead"], "gold", "COLOR_TRANSITION", False, True, 60, "True Chrysopoeia! A grain of the Stone transmutes base Saturnine Lead into pure Solar Gold!"),
    (["philosophers_stone", "iron"], "silver", "COLOR_TRANSITION", False, True, 50, "Transmutative projection elevates martial Iron into gleaming Lunar Silver!"),
    (["aqua_regia", "gold"], "aether", "BUBBLING", True, True, 55, "Dissolving incorruptible Gold in Royal Water liberates trapped Luminous Aether!"),

    # --- Multi-Stage 3-Ingredient Tri-Crucible Recipes (16 Recipes!) ---
    (["fire", "water", "air"], "electricity", "ELECTRICAL", False, False, 30, "Tri-elemental tempest inside the manifold generates pure storm lightning!"),
    (["earth", "water", "sand"], "clay", "LIQUID_MIXING", False, False, 26, "Balanced ternary sediment blend yields studio-grade refractory clay."),
    (["sulfur", "ash", "salt"], "gunpowder", "EXPLOSION", False, False, 38, "True historical ternary formula! Brimstone, potash, and saltpeter unite into potent Black Powder."),
    (["copper", "iron", "heat"], "bronze", "MELTING", False, False, 35, "Direct furnace smelting casts high-resonance Alchemical Bronze in one stage."),
    (["sand", "ash", "heat"], "glass", "MELTING", False, False, 34, "Potash flux lowers silica's fusion point to blow crystal-clear glass."),
    (["quicksilver", "sulfur", "salt"], "gold", "COLOR_TRANSITION", True, True, 65, "The Sacred Tria Prima! Spirit, Soul, and Body harmonize directly into Solar Gold!"),
    (["herb", "sunpetal", "charged_water"], "elixir_vitae", "BUBBLING", False, True, 80, "Botanical triad energized by galvanic aqua brews radiant Elixir Vitae!"),
    (["frostbloom", "ice", "quartz"], "cryo_crystal", "FREEZING", False, True, 65, "Triple cryogenic resonance grows a flawless Absolute Zero Shard!"),
    (["obsidian", "ash", "thermal_shock"], "void_essence", "EXPLOSION", True, True, 70, "Shattered black glass and calcined ash collapse into Abyssal Nigredo!"),
    (["sunpetal", "fire", "gold"], "phoenix_ember", "SPARKS", False, True, 70, "Solar gold, blossom, and flame ignite an eternal Phoenix Cinder!"),
    (["acid", "salt", "heat"], "aqua_regia", "BUBBLING", False, True, 52, "Heated distillation of vitriol and halite yields potent fuming Aqua Regia."),
    (["electricity", "steam", "quartz"], "plasma", "ELECTRICAL", False, True, 65, "Piezoelectric arc through superheated vapor ignites Contained Plasma!"),
    (["glowshroom", "amber_resin", "quartz"], "aether", "CRYSTALLIZATION", True, True, 65, "Bioluminescent light trapped in amber and focused through quartz distills Aether!"),
    (["quicksilver", "silver", "gold"], "amalgam", "LIQUID_MIXING", False, False, 48, "Royal electrum-mercury paste shimmers with catalytic conductivity."),
    (["mandrake", "nightshade", "amber_resin"], "elixir_vitae", "COLOR_TRANSITION", True, True, 85, "Great Spagyric Remedy: poison transmuted by vital root and amber into Panacea!"),
    (["phoenix_ember", "cryo_crystal", "amalgam"], "philosophers_stone", "CRYSTALLIZATION", True, True, 160, "Coincidentia Oppositorum! Immortal Fire and Absolute Frost bound in Mercury forge the Philosopher's Stone!"),
]

# ==============================================================================
# 7 LABORATORY UPGRADES (3 Tiers Each)
# ==============================================================================
UPGRADES = [
    {
        "id": "burner",
        "name": "Athanor Arcane Burner",
        "icon": "res://assets/upgrades/upg_burner.png",
        "max_level": 3,
        "costs": [45, 110, 220],
        "short_effect": "Faster reactions & bonus Thermal/Metal RP",
        "description": "Upgrades the under-crucible furnace from an amber spirit lamp to a cobalt-violet Athanor jet, accelerating reaction speed and boosting RP from thermal & metallic syntheses.",
        "tiers": [
            "Reaction time 0.85s | +15% RP on Thermal & Metallic reactions | Cobalt flame",
            "Reaction time 0.55s | +30% RP on Thermal & Metallic reactions | Twin brass jets",
            "Reaction time 0.35s | +50% RP on ALL reactions | Incandescent violet Athanor",
        ],
    },
    {
        "id": "mixer",
        "name": "Tri-Crucible Mixing Manifold",
        "icon": "res://assets/upgrades/upg_mixer.png",
        "max_level": 3,
        "costs": [50, 125, 240],
        "short_effect": "Unlocks 3rd Catalyst Slot & Chain-Feed",
        "description": "Installs a precision third brass reagent pedestal and copper return conduit, unlocking 3-ingredient multi-stage syntheses and one-click reaction chaining.",
        "tiers": [
            "Unlocks 3rd Catalyst Pedestal for 3-ingredient multi-stage recipes",
            "Unlocks Chain-Feed (send result directly into Slot 1) +20% 3-slot RP",
            "Master Manifold: +45% RP on multi-stage recipes & dual-output resonance",
        ],
    },
    {
        "id": "stabilizer",
        "name": "Gyroscopic Reaction Stabilizers",
        "icon": "res://assets/upgrades/upg_stabilizer.png",
        "max_level": 3,
        "costs": [40, 100, 200],
        "short_effect": "Salvages RP on fizzles & adds Timed bonus",
        "description": "Mounts spinning quartz-and-brass containment rings around the cauldron, harvesting residual energy from failed experiments and stabilizing volatile timed trials.",
        "tiers": [
            "Failed mixes salvage +3 RP instead of 0 | Spinning inner brass ring",
            "Failed mixes salvage +6 RP & tell if Slot 1 has undiscovered pairs",
            "Failed mixes salvage +10 RP & grant +5s time bonus per Timed Trial brew",
        ],
    },
    {
        "id": "storage",
        "name": "Apothecary Storage Racks",
        "icon": "res://assets/upgrades/upg_storage.png",
        "max_level": 3,
        "costs": [35, 90, 180],
        "short_effect": "Expands Quick-Pin Tray & Recipe Replay History",
        "description": "Expands carved walnut shelves and crystal jars, enlarging your Workbench Quick-Pin Favorites bar and Recent Synthesis Replay log.",
        "tiers": [
            "Unlocks 8 Quick-Pin Favorite slots & 5 Recent Synthesis replay slots",
            "Unlocks 12 Quick-Pin Favorite slots & 8 Recent Synthesis replay slots",
            "Unlocks 16 Quick-Pin Favorite slots & 12 Recent Synthesis replay slots",
        ],
    },
    {
        "id": "automaton",
        "name": "Clockwork Auto-Experimenter",
        "icon": "res://assets/upgrades/upg_automaton.png",
        "max_level": 3,
        "costs": [75, 160, 300],
        "short_effect": "Automatically tests untried combinations",
        "description": "Activates a spring-driven brass automaton arm beside the cauldron that methodically tests untried combinations of your discovered substances while toggled ON.",
        "tiers": [
            "Unlocks Auto-Experiment toggle (tests an untried pair every 5.0s)",
            "Precision escapement: tests an untried pair or trio every 2.8s",
            "Grand Automaton: tests every 1.4s & prioritizes valid new discoveries",
        ],
    },
    {
        "id": "research_lens",
        "name": "Hermetic Spectroscope",
        "icon": "res://assets/upgrades/upg_research_lens.png",
        "max_level": 3,
        "costs": [55, 130, 250],
        "short_effect": "Reveals Journal clues & boosts Discovery RP",
        "description": "Equips the Research Board with a dispersion prism spectroscope that decodes partial formulas in the Encyclopedia and amplifies Research Point insights.",
        "tiers": [
            "Reveals Result Category & Reaction Type of undiscovered Journal recipes | +15% Discovery RP",
            "Reveals 1st Ingredient of non-hidden undiscovered recipes | +25% Discovery RP",
            "Decodes 1st Ingredient even on Hidden Mysteries | +40% Discovery RP",
        ],
    },
    {
        "id": "detector",
        "name": "Rare Resonance Detector",
        "icon": "res://assets/upgrades/upg_detector.png",
        "max_level": 3,
        "costs": [65, 150, 280],
        "short_effect": "Detects undiscovered & Rare reagent pairs",
        "description": "Ignites a crystalline resonance lantern on your desk that pulses when a slotted reagent has undiscovered reactions, and can scan for valid partner hints.",
        "tiers": [
            "Desk Lantern glows when Slot 1 has undiscovered recipes | Scan Hint (15s cooldown)",
            "Distinguishes Rare/Hidden resonance in Violet-Gold | Scan Hint (6s cooldown)",
            "Instant Partner Highlight badge in inventory + Scan slots a valid pair (2s cooldown)",
        ],
    },
]

# ==============================================================================
# 16 CHALLENGES (All 5 Required Challenge Archetypes!)
# ==============================================================================
CHALLENGES = [
    {
        "id": "ch_first_glass",
        "title": "Apprentice Glassblower",
        "type": "SUBSTANCE",
        "target_ingredient": "glass",
        "time_limit": 0,
        "rp_reward": 40,
        "description": "Every true laboratory requires acid-proof vessels. Synthesize Alchemical Glass from mineral silica and intense thermal energy.",
    },
    {
        "id": "ch_vitriol_brew",
        "title": "Distillation of Vitriol",
        "type": "SUBSTANCE",
        "target_ingredient": "acid",
        "time_limit": 0,
        "rp_reward": 55,
        "description": "Synthesize corrosive green Vitriol Acid by uniting volcanic Brimstone Sulfur with pressurized vapor or ionized water.",
    },
    {
        "id": "ch_bronze_chain",
        "title": "Metallurgist's Fourfold Chain",
        "type": "MULTI_STEP",
        "required_ingredients": ["lava", "metal", "molten_metal", "bronze"],
        "time_limit": 0,
        "rp_reward": 70,
        "description": "Master the complete metallurgical chain: discover Lava, smelt Crude Metal, liquefy Molten Metal, and alloy Alchemical Bronze.",
    },
    {
        "id": "ch_royal_solvent",
        "title": "Royal Dissolution Protocol",
        "type": "MULTI_STEP",
        "required_ingredients": ["sulfur", "salt", "acid", "aqua_regia"],
        "time_limit": 0,
        "rp_reward": 90,
        "description": "Complete the multi-stage synthetic pathway culminating in fuming orange-gold Aqua Regia.",
    },
    {
        "id": "ch_riddle_elektron",
        "title": "Riddle of Ancient Elektron",
        "type": "HIDDEN_PUZZLE",
        "target_recipe_inputs": ["air", "amber_resin"],
        "time_limit": 0,
        "rp_reward": 65,
        "description": "Hidden Formula Puzzle: 'Rub golden tears of ancient sunlit trees against the rushing wind to wake the sleeping storm-spark.'",
    },
    {
        "id": "ch_riddle_nigredo",
        "title": "Riddle of the Black Sun",
        "type": "HIDDEN_PUZZLE",
        "target_recipe_inputs": ["nightshade", "obsidian"],
        "time_limit": 0,
        "rp_reward": 95,
        "description": "Hidden Formula Puzzle: 'When razor-sharp volcanic midnight drinks the venomous belladonna berry, Abyssal Nigredo awakens.'",
    },
    {
        "id": "ch_riddle_chrysopoeia",
        "title": "Paracelsian Secret of Sol",
        "type": "HIDDEN_PUZZLE",
        "target_recipe_inputs": ["quicksilver", "sulfur"],
        "time_limit": 0,
        "rp_reward": 100,
        "description": "Hidden Formula Puzzle: 'Wed the silver fluid Spirit to the yellow volcanic Soul to birth incorruptible Solar Gold.'",
    },
    {
        "id": "ch_riddle_tria_prima",
        "title": "Triad of the Sages",
        "type": "HIDDEN_PUZZLE",
        "target_recipe_inputs": ["quicksilver", "salt", "sulfur"],
        "time_limit": 0,
        "rp_reward": 120,
        "description": "Hidden 3-Slot Puzzle: 'Unite Spirit (Quicksilver), Soul (Sulfur), and Body (Salt) simultaneously in the Tri-Crucible Manifold.'",
    },
    {
        "id": "ch_timed_blitz",
        "title": "Timed Trial: Crucible Blitz",
        "type": "TIMED",
        "timed_goal_count": 5,
        "timed_targets": [],
        "time_limit": 60,
        "rp_reward": 80,
        "description": "Activate this Timed Trial and successfully execute 5 valid alchemical reactions within 60 seconds!",
    },
    {
        "id": "ch_timed_thermo",
        "title": "Timed Trial: Fire & Frost",
        "type": "TIMED",
        "timed_goal_count": 3,
        "timed_targets": ["steam", "thermal_shock", "obsidian"],
        "time_limit": 75,
        "rp_reward": 110,
        "description": "Activate this Timed Trial and synthesize Steam, Thermal Shock, and Obsidian within 75 seconds!",
    },
    {
        "id": "ch_timed_high_alchemy",
        "title": "Timed Trial: Grand Synthesis",
        "type": "TIMED",
        "timed_goal_count": 4,
        "timed_targets": ["glass", "acid", "gold", "aether"],
        "time_limit": 90,
        "rp_reward": 150,
        "description": "Activate this Timed Trial and brew Alchemical Glass, Vitriol Acid, Solar Gold, and Luminous Aether before the 90s hourglass runs dry!",
    },
    {
        "id": "ch_obj_botany",
        "title": "Imperial Herbarium",
        "type": "OBJECTIVE",
        "objective_category": "Plants",
        "objective_count": 8,
        "time_limit": 0,
        "rp_reward": 100,
        "description": "Research Objective: Cultivate and discover all 8 botanical substances in the Plants category.",
    },
    {
        "id": "ch_obj_metals",
        "title": "Seven Planetary Metals & Beyond",
        "type": "OBJECTIVE",
        "objective_category": "Metals",
        "objective_count": 9,
        "time_limit": 0,
        "rp_reward": 110,
        "description": "Research Objective: Smelt, refine, and transmute all 9 substances in the Metals category.",
    },
    {
        "id": "ch_obj_reactions",
        "title": "Phenomenology of Chaos",
        "type": "OBJECTIVE",
        "objective_reactions": 10,
        "time_limit": 0,
        "rp_reward": 125,
        "description": "Research Objective: Witness all 10 distinct physical reaction phenomena (Bubbling, Smoke, Sparks, Explosion, Crystallization, Freezing, Melting, Electrical, Liquid Mixing, Color Transition).",
    },
    {
        "id": "ch_obj_rares",
        "title": "Cabinet of Quintessence",
        "type": "OBJECTIVE",
        "objective_category": "Rare",
        "objective_count": 7,
        "time_limit": 0,
        "rp_reward": 180,
        "description": "Research Objective: Discover all 7 Rare & Mythic substances at the pinnacle of the encyclopedia.",
    },
    {
        "id": "ch_magnum_opus",
        "title": "The Magnum Opus",
        "type": "SUBSTANCE",
        "target_ingredient": "philosophers_stone",
        "time_limit": 0,
        "rp_reward": 250,
        "description": "Synthesize the legendary Philosopher's Stone - the ultimate triumph of Alchemy Chaos.",
    },
]

# ==============================================================================
# 20 ACHIEVEMENTS & 8 MILESTONES
# ==============================================================================
ACHIEVEMENTS = [
    {"id": "ach_first_brew", "title": "First Transmutation", "desc": "Complete your very first successful alchemical synthesis.", "metric": "recipes", "target": 1},
    {"id": "ach_steam_mud", "title": "Foundations of Matter", "desc": "Discover both Steam and Mud.", "metric": "pair", "items": ["steam", "mud"]},
    {"id": "ach_ing_10", "title": "Initiate Distiller", "desc": "Discover 10 unique substances.", "metric": "ingredients", "target": 10},
    {"id": "ach_ing_25", "title": "Journeyman Spagyrist", "desc": "Discover 25 unique substances (50% Encyclopedia).", "metric": "ingredients", "target": 25},
    {"id": "ach_ing_40", "title": "Adept of the Crucible", "desc": "Discover 40 unique substances.", "metric": "ingredients", "target": 40},
    {"id": "ach_ing_50", "title": "Complete Encyclopedia", "desc": "Discover all 50 unique substances in Alchemy Chaos!", "metric": "ingredients", "target": 50},
    {"id": "ach_rec_25", "title": "Experimental Method", "desc": "Discover 25 valid recipes.", "metric": "recipes", "target": 25},
    {"id": "ach_rec_60", "title": "Codex of Combinations", "desc": "Discover 60 valid recipes.", "metric": "recipes", "target": 60},
    {"id": "ach_rec_100", "title": "Century of Chaos", "desc": "Discover 100 valid recipes!", "metric": "recipes", "target": 100},
    {"id": "ach_hidden_5", "title": "Seeker of Esoterica", "desc": "Uncover 5 Hidden Mystery recipes.", "metric": "hidden_recipes", "target": 5},
    {"id": "ach_tri_crucible", "title": "Trinity Synthesis", "desc": "Successfully complete a 3-ingredient Tri-Crucible reaction.", "metric": "tri_recipe", "target": 1},
    {"id": "ach_all_elements", "title": "Lord of the Four Winds", "desc": "Discover all 8 Elemental substances.", "metric": "cat_Elements", "target": 8},
    {"id": "ach_all_minerals", "title": "Master Lapidary", "desc": "Discover all 9 Mineral substances.", "metric": "cat_Minerals", "target": 9},
    {"id": "ach_all_plants", "title": "Arcane Botanist", "desc": "Discover all 8 Plant substances.", "metric": "cat_Plants", "target": 8},
    {"id": "ach_all_metals", "title": "Royal Metallurgist", "desc": "Discover all 9 Metal substances.", "metric": "cat_Metals", "target": 9},
    {"id": "ach_all_synthetic", "title": "Synthetic Pioneer", "desc": "Discover all 9 Synthetic Compounds.", "metric": "cat_Synthetic", "target": 9},
    {"id": "ach_chrysopoeia", "title": "Chrysopoeia", "desc": "Synthesize pure Solar Gold.", "metric": "item", "item": "gold"},
    {"id": "ach_timed_win", "title": "Against the Hourglass", "desc": "Complete any Timed Laboratory Challenge.", "metric": "timed_challenge", "target": 1},
    {"id": "ach_max_upgrade", "title": "Grand Sanctum Apparatus", "desc": "Upgrade any piece of laboratory equipment to Level 3 (MAX).", "metric": "max_single_upgrade", "target": 1},
    {"id": "ach_philosopher", "title": "Magnum Opus", "desc": "Synthesize the legendary Philosopher's Stone.", "metric": "item", "item": "philosophers_stone"},
]

MILESTONES = [
    {"id": "ms_1", "title": "Novice Apprentice", "req_ingredients": 10, "req_recipes": 8, "rp_bonus": 35},
    {"id": "ms_2", "title": "Guild Apothecary", "req_ingredients": 18, "req_recipes": 20, "rp_bonus": 55},
    {"id": "ms_3", "title": "Crucible Adept", "req_ingredients": 26, "req_recipes": 38, "rp_bonus": 80},
    {"id": "ms_4", "title": "Hermetic Scholar", "req_ingredients": 34, "req_recipes": 55, "rp_bonus": 110},
    {"id": "ms_5", "title": "Master Distiller", "req_ingredients": 42, "req_recipes": 75, "rp_bonus": 150},
    {"id": "ms_6", "title": "Sovereign Chrysopoeist", "req_ingredients": 47, "req_recipes": 92, "rp_bonus": 200},
    {"id": "ms_7", "title": "Centurion of Reactions", "req_ingredients": 49, "req_recipes": 100, "rp_bonus": 250},
    {"id": "ms_8", "title": "Archmage of Alchemy Chaos", "req_ingredients": 50, "req_recipes": 115, "rp_bonus": 400},
]


def canonical_key(inputs):
    return "+".join(sorted(inputs))


def build_and_verify():
    assert len(INGREDIENTS) == 50, f"Expected 50 ingredients, got {len(INGREDIENTS)}"
    ing_by_id = {}
    for idx, ing in enumerate(INGREDIENTS, 1):
        assert ing["num"] == idx, f"Numbering mismatch at {ing['id']}"
        assert ing["id"] not in ing_by_id, f"Duplicate ingredient id {ing['id']}"
        assert ing["category"] in CATEGORIES, f"Invalid category {ing['category']}"
        ing_by_id[ing["id"]] = ing

    cat_counts = Counter(i["category"] for i in INGREDIENTS)
    print("Ingredient counts by category:", dict(cat_counts))

    # Build normalized recipes list and check duplicates
    recipes = []
    seen_keys = {}
    for idx, r in enumerate(RAW_RECIPES, 1):
        inputs, result, rtype, is_hidden, is_rare, rp_reward, lore = r
        assert len(inputs) in (2, 3), f"Invalid input length in recipe {idx}"
        for inp in inputs:
            assert inp in ing_by_id, f"Unknown input '{inp}' in recipe {idx}"
        assert result in ing_by_id, f"Unknown result '{result}' in recipe {idx}"
        assert rtype in REACTION_TYPES, f"Unknown reaction type '{rtype}'"
        ckey = canonical_key(inputs)
        if ckey in seen_keys:
            raise ValueError(f"Duplicate recipe key '{ckey}': {seen_keys[ckey]} vs {result}")
        seen_keys[ckey] = result
        recipes.append({
            "id": f"rec_{idx:03d}",
            "num": idx,
            "inputs": sorted(inputs),
            "key": ckey,
            "result": result,
            "reaction_type": rtype,
            "is_hidden": is_hidden,
            "is_rare": is_rare or ing_by_id[result]["category"] == "Rare",
            "rp_reward": rp_reward,
            "lore": lore,
        })

    print(f"Total unique valid recipes: {len(recipes)} (2-slot: {sum(1 for r in recipes if len(r['inputs'])==2)}, 3-slot: {sum(1 for r in recipes if len(r['inputs'])==3)})")
    assert len(recipes) >= 100, f"Must have >= 100 recipes, got {len(recipes)}"

    # Verify prompt-mandated examples verbatim!
    required_examples = [
        (["fire", "water"], "steam"),
        (["water", "earth"], "mud"),
        (["heat", "metal"], "molten_metal"),
        (["ice", "fire"], "thermal_shock"),
        (["electricity", "water"], "charged_water"),
    ]
    for req_in, req_out in required_examples:
        k = canonical_key(req_in)
        assert seen_keys.get(k) == req_out, f"Missing required example {k} -> {req_out}"
    print("All 5 prompt-specified example recipes verified verbatim!")

    # Verify all 10 reaction types are used!
    rtype_counts = Counter(r["reaction_type"] for r in recipes)
    for rt in REACTION_TYPES:
        assert rtype_counts[rt] >= 4, f"Reaction type {rt} under-represented: {rtype_counts[rt]}"
    print("Reaction type distribution:", dict(rtype_counts))

    # BFS Reachability Check #1: Using ONLY 2-ingredient recipes from the 4 starters (zero upgrade deadlock!)
    discovered_2slot = {i["id"] for i in INGREDIENTS if i["starter"]}
    changed = True
    steps = 0
    while changed:
        changed = False
        steps += 1
        for r in recipes:
            if len(r["inputs"]) == 2 and all(x in discovered_2slot for x in r["inputs"]):
                if r["result"] not in discovered_2slot:
                    discovered_2slot.add(r["result"])
                    changed = True

    missing_2slot = set(ing_by_id.keys()) - discovered_2slot
    assert not missing_2slot, f"DEADLOCK! Unreachable via 2-slot recipes: {missing_2slot}"
    print(f"Reachability Proof #1 PASSED: All {len(discovered_2slot)}/50 ingredients reachable from 4 starters in {steps} BFS waves using 2-slot recipes alone!")

    # Reachability Check #2: Verify EVERY recipe is triggerable once all ingredients are reachable
    reachable_recipes = [r for r in recipes if all(x in discovered_2slot for x in r["inputs"])]
    assert len(reachable_recipes) == len(recipes), "Unreachable recipes found!"
    print(f"Reachability Proof #2 PASSED: All {len(reachable_recipes)}/{len(recipes)} recipes are 100% reachable!")

    # Economy Check: Total earnable RP vs Total Upgrade Cost
    total_ing_rp = sum(i["rp_value"] for i in INGREDIENTS)
    total_rec_rp = sum(r["rp_reward"] for r in recipes)
    total_chal_rp = sum(c["rp_reward"] for c in CHALLENGES)
    total_ms_rp = sum(m["rp_bonus"] for m in MILESTONES)
    total_earnable_rp = total_ing_rp + total_rec_rp + total_chal_rp + total_ms_rp
    total_upgrade_cost = sum(sum(u["costs"]) for u in UPGRADES)
    print(f"Economy Proof: Earnable Base RP = {total_earnable_rp} (Ing={total_ing_rp}, Rec={total_rec_rp}, Chal={total_chal_rp}, MS={total_ms_rp}) vs Max All Upgrades Cost = {total_upgrade_cost}")
    assert total_earnable_rp > total_upgrade_cost * 1.5, "Insufficient RP economy!"

    # Verify Challenge references
    for ch in CHALLENGES:
        if "target_ingredient" in ch:
            assert ch["target_ingredient"] in ing_by_id
        if "required_ingredients" in ch:
            for req in ch["required_ingredients"]:
                assert req in ing_by_id
        if "target_recipe_inputs" in ch:
            k = canonical_key(ch["target_recipe_inputs"])
            assert k in seen_keys, f"Challenge {ch['id']} missing recipe key {k}"

    return recipes


def write_outputs(recipes):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(root, "alchemy_chaos", "data")
    ing_tres_dir = os.path.join(data_dir, "ingredients")
    rec_tres_dir = os.path.join(data_dir, "recipes")
    os.makedirs(ing_tres_dir, exist_ok=True)
    os.makedirs(rec_tres_dir, exist_ok=True)

    manifest = {
        "version": "1.0.0",
        "categories": CATEGORIES,
        "reaction_types": REACTION_TYPES,
        "ingredients": INGREDIENTS,
        "recipes": recipes,
        "upgrades": UPGRADES,
        "challenges": CHALLENGES,
        "achievements": ACHIEVEMENTS,
        "milestones": MILESTONES,
    }
    manifest_path = os.path.join(data_dir, "database_manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    def hex_to_gd_color(h: str) -> str:
        hs = h.lstrip("#")
        r = int(hs[0:2], 16) / 255.0
        g = int(hs[2:4], 16) / 255.0
        b = int(hs[4:6], 16) / 255.0
        return f"Color({r:.4f}, {g:.4f}, {b:.4f}, 1.0)"

    # Also write authentic Godot 4 Custom Resource (.tres) files for every Ingredient & Recipe!
    for ing in INGREDIENTS:
        props_str = ", ".join(f'"{p}"' for p in ing["properties"])
        tres = f'''[gd_resource type="Resource" script_class="IngredientData" load_steps=2 format=3]

[ext_resource type="Script" path="res://scripts/resources/ingredient_data.gd" id="1"]

[resource]
script = ExtResource("1")
id = "{ing["id"]}"
num = {ing["num"]}
display_name = "{ing["name"]}"
category = "{ing["category"]}"
starter = {"true" if ing["starter"] else "false"}
rarity = "{ing["rarity"]}"
primary_color = {hex_to_gd_color(ing["primary"])}
secondary_color = {hex_to_gd_color(ing["secondary"])}
properties = PackedStringArray({props_str})
rp_value = {ing["rp_value"]}
icon_path = "res://assets/ingredients/{ing["id"]}.png"
description = {json.dumps(ing["description"])}
'''
        with open(os.path.join(ing_tres_dir, f"{ing['num']:02d}_{ing['id']}.tres"), "w", encoding="utf-8") as f:
            f.write(tres)

    for rec in recipes:
        inp_str = ", ".join(f'"{i}"' for i in rec["inputs"])
        tres = f'''[gd_resource type="Resource" script_class="RecipeData" load_steps=2 format=3]

[ext_resource type="Script" path="res://scripts/resources/recipe_data.gd" id="1"]

[resource]
script = ExtResource("1")
id = "{rec["id"]}"
num = {rec["num"]}
inputs = PackedStringArray({inp_str})
result = "{rec["result"]}"
reaction_type = "{rec["reaction_type"]}"
is_hidden = {"true" if rec["is_hidden"] else "false"}
is_rare = {"true" if rec["is_rare"] else "false"}
rp_reward = {rec["rp_reward"]}
lore = {json.dumps(rec["lore"])}
'''
        with open(os.path.join(rec_tres_dir, f"{rec['id']}.tres"), "w", encoding="utf-8") as f:
            f.write(tres)

    print(f"Wrote manifest + {len(INGREDIENTS)} Ingredient .tres + {len(recipes)} Recipe .tres files!")


if __name__ == "__main__":
    recs = build_and_verify()
    write_outputs(recs)
