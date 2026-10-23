--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ReplicatedStorage.Shared.QuestDefinitions
    Original narrative ("Project LAZARUS & Frequency 142.8 MHz"),
    6 Main Story Quests, 6 Side Quests, and 18 Collectible Lore Documents / Recordings.
]]

local QuestDefinitions = {}

export type QuestObjectiveDef = {
	Id: string,
	Type: "Interact" | "VisitRegion" | "CollectItem" | "KillSpecies" | "CraftItem" | "BuildStructure" | "ReadLore",
	Target: string,
	RequiredCount: number,
	Description: string,
	Waypoint: Vector3?,
}

export type QuestDef = {
	Id: string,
	Title: string,
	Kind: "Main" | "Side",
	Act: number,
	Summary: string,
	NextQuestId: string?,
	Objectives: { QuestObjectiveDef },
	Rewards: {
		Items: { { ItemId: string, Count: number } }?,
		UnlockBlueprints: { string }?,
	},
}

QuestDefinitions.QUESTS = {
	-- ============================================================
	-- MAIN STORY QUESTLINE (ACTS I - VI)
	-- ============================================================
	Main_01_EchoAwakening = {
		Id = "Main_01_EchoAwakening",
		Title = "Act I: Awakening in Echo-07",
		Kind = "Main",
		Act = 1,
		Summary = "You awoke alone inside Underground Emergency Shelter Echo-07 as static hissed from an analog field radio. Inspect Dr. Vance's terminal log and ascend the blast shaft into The Dead Suburbs.",
		NextQuestId = "Main_02_SuburbiaRelay",
		Objectives = {
			{
				Id = "ReadShelterLog",
				Type = "ReadLore",
				Target = "Lore_01_EchoTerminal",
				RequiredCount = 1,
				Description = "Inspect the Echo-07 Emergency Terminal Log",
				Waypoint = Vector3.new(6, -19, 626),
			},
			{
				Id = "ReachDeadSuburbs",
				Type = "VisitRegion",
				Target = "DeadSuburbs",
				RequiredCount = 1,
				Description = "Climb the blast hatch into The Dead Suburbs",
				Waypoint = Vector3.new(0, 3.5, 585),
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "SterileBandage", Count = 2 },
				{ ItemId = "BatteryCell", Count = 1 },
			},
		},
	},
	Main_02_SuburbiaRelay = {
		Id = "Main_02_SuburbiaRelay",
		Title = "Act II: Dead Air on Maple Creek",
		Kind = "Main",
		Act = 2,
		Summary = "The suburbs are silent save for distant metallic groans. Restore power to the neighborhood Civil Defense Relay Mast to triangulate the 142.8 MHz broadcast.",
		NextQuestId = "Main_03_MercyTriage",
		Objectives = {
			{
				Id = "ReadSuburbNote",
				Type = "ReadLore",
				Target = "Lore_03_EvacNotice",
				RequiredCount = 1,
				Description = "Search the abandoned houses for Evacuation Order #14",
				Waypoint = Vector3.new(-88, 4, 510),
			},
			{
				Id = "RestoreSuburbRelay",
				Type = "Interact",
				Target = "Relay_Suburbs",
				RequiredCount = 1,
				Description = "Reactivate the Suburbs Civil Defense Transmitter",
				Waypoint = Vector3.new(135, 5, 420),
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "ServicePistol", Count = 1 },
				{ ItemId = "Ammo9mm", Count = 24 },
			},
		},
	},
	Main_03_MercyTriage = {
		Id = "Main_03_MercyTriage",
		Title = "Act III: Triage at Mercy General",
		Kind = "Main",
		Act = 3,
		Summary = "Triangulation points north into The Forgotten City. Dr. Vance's research team established a quarantine ward inside Mercy General Hospital before communications went dark.",
		NextQuestId = "Main_04_BlackwoodSignal",
		Objectives = {
			{
				Id = "EnterCity",
				Type = "VisitRegion",
				Target = "ForgottenCity",
				RequiredCount = 1,
				Description = "Follow the ruined highway north into The Forgotten City",
				Waypoint = Vector3.new(0, 4, -220),
			},
			{
				Id = "RecoverHospitalLog",
				Type = "ReadLore",
				Target = "Lore_09_VanceTriageTape",
				RequiredCount = 1,
				Description = "Locate Dr. Vance's Quarantine Recorder inside Mercy General Hospital",
				Waypoint = Vector3.new(-145, 5, -360),
			},
			{
				Id = "CollectKeycard",
				Type = "CollectItem",
				Target = "Site09Keycard",
				RequiredCount = 1,
				Description = "Secure the Site-09 Clearance Keycard from Mercy General",
				Waypoint = Vector3.new(-152, 5, -372),
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "SurgicalTraumaKit", Count = 1 },
				{ ItemId = "BroadAntibiotics", Count = 2 },
			},
		},
	},
	Main_04_BlackwoodSignal = {
		Id = "Main_04_BlackwoodSignal",
		Title = "Act IV: Shadows of Blackwood & The Smelter",
		Kind = "Main",
		Act = 4,
		Summary = "Vance's recording reveals two secondary phase-lock repeaters feeding the signal: one hidden deep in Blackwood Forest, the other inside the Industrial Bio-Chem Sub-Lab.",
		NextQuestId = "Main_05_LazarusBunker",
		Objectives = {
			{
				Id = "ReadForestLog",
				Type = "ReadLore",
				Target = "Lore_06_RangerRecording",
				RequiredCount = 1,
				Description = "Investigate the Blackwood Fire-Watch Cabin recording",
				Waypoint = Vector3.new(-740, 6, 190),
			},
			{
				Id = "ReadIndustrialLog",
				Type = "ReadLore",
				Target = "Lore_12_SubLabManifest",
				RequiredCount = 1,
				Description = "Search the Contaminated Industrial Sub-Lab for the Acoustic Catalyst Manifest",
				Waypoint = Vector3.new(790, 4, 165),
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "GasMaskRespirator", Count = 1 },
				{ ItemId = "PotassiumIodide", Count = 2 },
			},
			UnlockBlueprints = { "CraftAmmo556", "CraftTacticalVest" },
		},
	},
	Main_05_LazarusBunker = {
		Id = "Main_05_LazarusBunker",
		Title = "Act V: Project LAZARUS (Site-09)",
		Kind = "Main",
		Act = 5,
		Summary = "Armed with CBRN protection and Clearance Keycard, breach the Military Exclusion Zone's Underground Signal Laboratory to learn what Project LAZARUS unleashed.",
		NextQuestId = "Main_06_OriginOfTheSignal",
		Objectives = {
			{
				Id = "EnterMilitaryZone",
				Type = "VisitRegion",
				Target = "MilitaryExclusionZone",
				RequiredCount = 1,
				Description = "Infiltrate The Military Exclusion Zone in the northwest",
				Waypoint = Vector3.new(-760, 4, -680),
			},
			{
				Id = "ReadLazarusDossier",
				Type = "ReadLore",
				Target = "Lore_15_LazarusDirective",
				RequiredCount = 1,
				Description = "Access the Project LAZARUS Underground Lab Dossier",
				Waypoint = Vector3.new(-810, -14, -745),
			},
			{
				Id = "CraftOrAcquireDecoder",
				Type = "CollectItem",
				Target = "SignalDecoder",
				RequiredCount = 1,
				Description = "Craft or recover a LAZARUS Signal Decoder",
				Waypoint = Vector3.new(-810, -14, -745),
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "AssaultRifle", Count = 1 },
				{ ItemId = "Ammo556", Count = 60 },
			},
			UnlockBlueprints = { "CraftRadPurge" },
		},
	},
	Main_06_OriginOfTheSignal = {
		Id = "Main_06_OriginOfTheSignal",
		Title = "Act VI: Origin of the Last Signal",
		Kind = "Main",
		Act = 6,
		Summary = "Tower Zero inside The Red Wasteland is still broadcasting the automated neural carrier wave that drives the mutated ecosystem. Cross the irradiated crater basin and sever the Last Signal.",
		NextQuestId = nil,
		Objectives = {
			{
				Id = "EnterRedWasteland",
				Type = "VisitRegion",
				Target = "RedWasteland",
				RequiredCount = 1,
				Description = "Cross into The Red Wasteland in the northeast",
				Waypoint = Vector3.new(760, 4, -680),
			},
			{
				Id = "ReadTowerZeroLog",
				Type = "ReadLore",
				Target = "Lore_18_FinalTransmission",
				RequiredCount = 1,
				Description = "Read Dr. Vance's Final Log at Tower Zero",
				Waypoint = Vector3.new(835, 8, -760),
			},
			{
				Id = "ShutdownTowerZero",
				Type = "Interact",
				Target = "Console_TowerZero",
				RequiredCount = 1,
				Description = "Override Tower Zero's Carrier Array using the Signal Decoder",
				Waypoint = Vector3.new(840, 8, -770),
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "MilitaryRucksack", Count = 1 },
				{ ItemId = "RadPurgeInjector", Count = 2 },
				{ ItemId = "SurgicalTraumaKit", Count = 2 },
			},
		},
	},

	-- ============================================================
	-- SIDE QUESTS (6 Explorative & Survival Assignments)
	-- ============================================================
	Side_01_MedicalScavenge = {
		Id = "Side_01_MedicalScavenge",
		Title = "Field Triage Stockpile",
		Kind = "Side",
		Act = 1,
		Summary = "Infection and hemorrhage claim unready survivors faster than mutants. Gather sterile bandages and antiseptic for emergency treatment.",
		NextQuestId = nil,
		Objectives = {
			{
				Id = "GatherBandages",
				Type = "CollectItem",
				Target = "SterileBandage",
				RequiredCount = 3,
				Description = "Acquire 3 Sterile Gauze Bandages",
			},
			{
				Id = "GatherAntiseptic",
				Type = "CollectItem",
				Target = "AntisepticBottle",
				RequiredCount = 1,
				Description = "Find 1 Chlorhexidine Antiseptic",
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "HemostaticDressing", Count = 2 },
				{ ItemId = "OrthopedicSplint", Count = 1 },
			},
		},
	},
	Side_02_FortifyShelter = {
		Id = "Side_02_FortifyShelter",
		Title = "Before Nightfall",
		Kind = "Side",
		Act = 1,
		Summary = "Temperatures plunge after dusk and nocturnal hunters emerge. Establish a campfire and fortify a shelter structure.",
		NextQuestId = nil,
		Objectives = {
			{
				Id = "BuildFire",
				Type = "BuildStructure",
				Target = "CampfireStation",
				RequiredCount = 1,
				Description = "Construct a Stone-Ringed Campfire",
			},
			{
				Id = "BuildBarricade",
				Type = "BuildStructure",
				Target = "WindowBarricade",
				RequiredCount = 1,
				Description = "Fortify a window or perimeter with a Plank Window Barricade",
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "CookedStew", Count = 2 },
				{ ItemId = "WoodPlank", Count = 4 },
			},
		},
	},
	Side_03_SubstationGenerator = {
		Id = "Side_03_SubstationGenerator",
		Title = "Sparks in the Dark",
		Kind = "Side",
		Act = 2,
		Summary = "Restore emergency auxiliary lighting at the Industrial Substation generator.",
		NextQuestId = nil,
		Objectives = {
			{
				Id = "RepairIndGenerator",
				Type = "Interact",
				Target = "Generator_Industrial",
				RequiredCount = 1,
				Description = "Restart the Auxiliary Diesel Generator in The Industrial Zone",
				Waypoint = Vector3.new(690, 4, 230),
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "RepairKit", Count = 2 },
				{ ItemId = "BatteryCell", Count = 3 },
			},
		},
	},
	Side_04_SilenceTheHollows = {
		Id = "Side_04_SilenceTheHollows",
		Title = "Acoustic Purge",
		Kind = "Side",
		Act = 2,
		Summary = "Hollows prowl the ruined residential blocks, drawn to every creaking door. Thin their pack so survivors can scavenge.",
		NextQuestId = nil,
		Objectives = {
			{
				Id = "EliminateHollows",
				Type = "KillSpecies",
				Target = "Hollow",
				RequiredCount = 3,
				Description = "Eliminate 3 Hollows",
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "Ammo9mm", Count = 30 },
				{ ItemId = "CannedPeaches", Count = 2 },
			},
		},
	},
	Side_05_BlackwoodBunker = {
		Id = "Side_05_BlackwoodBunker",
		Title = "Voices Under the Pines",
		Kind = "Side",
		Act = 3,
		Summary = "Strange metallic knocking echoes from a drainage tunnel beneath Blackwood Forest, accompanied by a Mimic's counterfeit SOS.",
		NextQuestId = nil,
		Objectives = {
			{
				Id = "FindCaveLog",
				Type = "ReadLore",
				Target = "Lore_07_TunnelScratchings",
				RequiredCount = 1,
				Description = "Investigate the Blackwood Underground Tunnel log",
				Waypoint = Vector3.new(-860, -8, 60),
			},
			{
				Id = "KillMimic",
				Type = "KillSpecies",
				Target = "Mimic",
				RequiredCount = 1,
				Description = "Hunt down The Mimic lurking in the forest",
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "TacticalCrossbow", Count = 1 },
				{ ItemId = "CrossbowBolt", Count = 12 },
			},
		},
	},
	Side_06_ApexNocturne = {
		Id = "Side_06_ApexNocturne",
		Title = "White Shadow at Midnight",
		Kind = "Side",
		Act = 5,
		Summary = "Only after nightfall does The Pale One stalk the Exclusion perimeter. Survive an encounter and bring down the apex horror.",
		NextQuestId = nil,
		Objectives = {
			{
				Id = "SlayPaleOne",
				Type = "KillSpecies",
				Target = "PaleOne",
				RequiredCount = 1,
				Description = "Defeat The Pale One at night",
			},
		},
		Rewards = {
			Items = {
				{ ItemId = "HuntingRifle", Count = 1 },
				{ ItemId = "Ammo308", Count = 20 },
			},
		},
	},
}

-- 18 Environmental Storytelling Documents & Survivor Recordings (Section 29)
QuestDefinitions.LORE_ENTRIES = {
	Lore_01_EchoTerminal = {
		Id = "Lore_01_EchoTerminal",
		Title = "Echo-07 Terminal Log #044",
		Region = "EmergencyShelter",
		Text = "OCT 14 — Automated cryo-sedation cycle ended 194 days late. Surface sensors read airborne particulate contamination and an unscheduled 142.8 MHz carrier wave originating northeast. Do NOT answer automated evacuation prompts.",
	},
	Lore_02_BunkerWallNote = {
		Id = "Lore_02_BunkerWallNote",
		Title = "Scrawled Hatch Warning",
		Region = "EmergencyShelter",
		Text = "They hear flashlights clicking if you're right beside a window, and they SEE the beam through fog. Crouch when moving past cars—rusted alarms still have battery charge.",
	},
	Lore_03_EvacNotice = {
		Id = "Lore_03_EvacNotice",
		Title = "Civil Defense Evacuation Order #14",
		Region = "DeadSuburbs",
		Text = "ALL MAPLE CREEK RESIDENTS: Proceed immediately to Mercy General Triage. Boil all tap water with activated charcoal. Report individuals exhibiting auditory fixation or vocal mimicry.",
	},
	Lore_04_SuburbDiary = {
		Id = "Lore_04_SuburbDiary",
		Title = "Water-Stained Kitchen Journal",
		Region = "DeadSuburbs",
		Text = "Day 9 without grid power. Something stood between the birch trees behind the garage for four hours without blinking. My chest felt like ice just looking at its antlers.",
	},
	Lore_05_ShopkeeperLedger = {
		Id = "Lore_05_ShopkeeperLedger",
		Title = "Mart-24 Backroom Note",
		Region = "DeadSuburbs",
		Text = "Boarded the front glass with oak planks, didn't matter when the siren went off down the block. Keep a splint in your belt—jumping off a porch roof with a pack will snap an ankle.",
	},
	Lore_06_RangerRecording = {
		Id = "Lore_06_RangerRecording",
		Title = "Ranger Station Reel #09",
		Region = "BlackwoodForest",
		Text = "[TAPE HISS] 'This is Warden Miller, Blackwood Tower. Military trucks hauled three phase-antennas into the pines before the sky turned copper. Now the radios repeat my wife's voice from last winter—word for word.'",
	},
	Lore_07_TunnelScratchings = {
		Id = "Lore_07_TunnelScratchings",
		Title = "Drainage Tunnel Recorder",
		Region = "BlackwoodForest",
		Text = "[HEAVY BREATHING] 'Don't sprint over loose earth in Blackwood! Something moves underneath the pine needles—it hears boots pounding and comes straight up through the mud.'",
	},
	Lore_08_CamperLetter = {
		Id = "Lore_08_CamperLetter",
		Title = "Unsent Hunter's Letter",
		Region = "BlackwoodForest",
		Text = "Crossbow bolts don't bring the Hollows running like my .357 did. Pine-needle tea keeps the shakes away when the fog rolls in.",
	},
	Lore_09_VanceTriageTape = {
		Id = "Lore_09_VanceTriageTape",
		Title = "Dr. Elena Vance — Quarantine Ward Log",
		Region = "ForgottenCity",
		Text = "'Patients exposed to the 142.8 MHz harmonic aren't dying of radiation alone—their nervous systems are rewiring around acoustic reception. My clearance badge for Site-09 is in the lockbox. Shut LAZARUS down.'",
	},
	Lore_10_PrecinctDispatch = {
		Id = "Lore_10_PrecinctDispatch",
		Title = "Precinct 9 Desk Blotter",
		Region = "ForgottenCity",
		Text = "02:14 — Patrol Unit 4 fired at a Screamer near MetroGrocer. Within ninety seconds every alley emptied onto 4th Street. Aim for the throat sac before it inhales.",
	},
	Lore_11_RooftopSniperNote = {
		Id = "Lore_11_RooftopSniperNote",
		Title = "Apartment Rooftop Spotter Log",
		Region = "ForgottenCity",
		Text = "During thunderstorms you can sprint across the boulevard—the thunder drowns your boots. Wait for the flash.",
	},
	Lore_12_SubLabManifest = {
		Id = "Lore_12_SubLabManifest",
		Title = "Chem-Plant Sub-Lab Manifest",
		Region = "IndustrialZone",
		Text = "SHIPMENT: 40 drums Bio-Acoustic Isotope Catalyst routed from Smelter B to Site-09 & Tower Zero. Mandatory CBRN Respirators beyond Airlock 2.",
	},
	Lore_13_ForemanMemo = {
		Id = "Lore_13_ForemanMemo",
		Title = "Cooling Tower Maintenance Memo",
		Region = "IndustrialZone",
		Text = "Workers near Tank 3 developed calcified dermal plates within 72 hours of the pipe rupture. Standard 9mm rounds ricochet right off their shoulders.",
	},
	Lore_14_CheckpointOrders = {
		Id = "Lore_14_CheckpointOrders",
		Title = "AEGIS Perimeter Rules of Engagement",
		Region = "MilitaryExclusionZone",
		Text = "CLASSIFIED — Maintain halogen floodlights from 19:30 to 05:45. Specimens designated 'PALE-01' hunt exclusively in unlit sectors.",
	},
	Lore_15_LazarusDirective = {
		Id = "Lore_15_LazarusDirective",
		Title = "PROJECT LAZARUS — Executive Summary",
		Region = "MilitaryExclusionZone",
		Text = "TOP SECRET // SITE-09: Project LAZARUS sought to link emergency civil transceivers into an atmospheric neuro-stabilization array. Instead, Tower Zero's resonance cascade shattered the blood-brain barrier across six counties.",
	},
	Lore_16_LabAutopsy = {
		Id = "Lore_16_LabAutopsy",
		Title = "Site-09 Specimen Autopsy #12",
		Region = "MilitaryExclusionZone",
		Text = "Subject 'MIMIC': Laryngeal cartilage fused with copper coil fragments ingested after collapse. Capable of storing 6-second radio bursts to bait warm-blooded targets.",
	},
	Lore_17_CraterSurvey = {
		Id = "Lore_17_CraterSurvey",
		Title = "Red-Zone Dosimetry Log",
		Region = "RedWasteland",
		Text = "Ambient exposure exceeds 1.8 mSv/s in the basin—worse during ochre Radiation Storms. Carry Potassium Iodide and Chelation Injectors or you won't reach the array.",
	},
	Lore_18_FinalTransmission = {
		Id = "Lore_18_FinalTransmission",
		Title = "Dr. Vance — Tower Zero Final Entry",
		Region = "RedWasteland",
		Text = "'I reached the array with a cracked leg and one bandage left, without a Decoder to kill the carrier loop. Whoever hears Echo-07's wake-up ping: plug the Decoder into the master bus. Let the dead finally rest.'",
	},
}

return table.freeze(QuestDefinitions)
