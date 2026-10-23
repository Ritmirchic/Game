--!strict
--[[
	EnemyDefinitions — the ten mutant species.

	speed        walk speed (studs/s)
	health       hit points
	damage       per attack
	attackRange  reach in studs
	attackRate   seconds between attacks
	vision       sight distance
	hearing      hearing radius multiplier
	fov          vision cone (degrees)
	aggression   0..1 — how likely the creature commits to a chase
	tiers        spawn tiers this creature can appear at
	habitat      regions where it spawns naturally
	behaviour    special AI hooks implemented in EnemyService
	scale        model scale
	color        body colour
	nocturnal    only awakens at night
	weakness     weapon tags that deal bonus damage
]]

local Enemies = {}

local function enemy(t)
	t.fov = t.fov or 130
	t.hearing = t.hearing or 1.0
	t.aggression = t.aggression or 0.6
	t.attackRate = t.attackRate or 1.4
	t.attackRange = t.attackRange or 7
	t.scale = t.scale or 1
	t.weakness = t.weakness or {}
	t.behaviour = t.behaviour or {}
	t.habitat = t.habitat or { "Forest" }
	t.tiers = t.tiers or { 1, 2 }
	Enemies[t.id] = t
	return t
end

enemy {
	id = "hollow", name = "The Hollow", health = 70, speed = 12, damage = 12,
	attackRange = 7, attackRate = 1.25, vision = 85, hearing = 2.2, fov = 160,
	aggression = 0.85, color = Color3.fromRGB(126, 122, 112), scale = 1.0,
	tiers = { 1, 2 }, habitat = { "Suburbs", "Forest", "City" },
	desc = "Gaunt, hairless, and blind. It hunts by sound alone and screams when it finds you.",
	behaviour = { soundHunter = true, blind = true, scream = 0.25 },
	drops = { "rags", "mutant_gland" },
	lootChance = 0.55,
}

enemy {
	id = "crawler", name = "The Crawler", health = 45, speed = 22, damage = 9,
	attackRange = 6, attackRate = 0.8, vision = 70, hearing = 1.4, fov = 120,
	aggression = 0.9, color = Color3.fromRGB(96, 82, 74), scale = 0.72,
	tiers = { 1, 2, 3 }, habitat = { "Forest", "Industrial", "City" },
	desc = "Low to the ground, fast through gaps you cannot follow. Attacks legs first.",
	behaviour = { limbTarget = "Legs", crawl = true, packHunter = true },
	drops = { "raw_meat", "mutant_gland" },
	lootChance = 0.6,
}

enemy {
	id = "watcher", name = "The Watcher", health = 55, speed = 9, damage = 6,
	attackRange = 8, attackRate = 2.0, vision = 190, hearing = 1.1, fov = 90,
	aggression = 0.2, color = Color3.fromRGB(108, 116, 104), scale = 1.25,
	tiers = { 2, 3 }, habitat = { "Forest", "City", "Military" },
	desc = "It stands at the treeline and stares. Staring back builds stress until you break.",
	behaviour = { observer = true, stressAura = 2.6, fleeOnApproach = 40, teleport = true },
	drops = { "mutant_gland" },
	lootChance = 0.35,
}

enemy {
	id = "screamer", name = "The Screamer", health = 60, speed = 14, damage = 5,
	attackRange = 6, attackRate = 2.2, vision = 95, hearing = 1.6, fov = 140,
	aggression = 0.7, color = Color3.fromRGB(140, 96, 92), scale = 0.95,
	tiers = { 2, 3 }, habitat = { "Forest", "Industrial", "Wasteland" },
	desc = "Thin throat, enormous lungs. Its cry pulls every mutant within a kilometre.",
	behaviour = { caller = true, callRadius = 220, callCooldown = 22 },
	drops = { "raw_meat", "mutant_gland" },
	lootChance = 0.5,
}

enemy {
	id = "brute", name = "The Brute", health = 420, speed = 13, damage = 34,
	attackRange = 10, attackRate = 2.1, vision = 90, hearing = 1.0, fov = 120,
	aggression = 0.95, color = Color3.fromRGB(92, 84, 70), scale = 1.75,
	tiers = { 4, 5, 6 }, habitat = { "Industrial", "Military", "Wasteland" },
	desc = "Bone plates over muscle. Bullets work; it just takes a lot of them.",
	behaviour = { armored = 0.35, charge = true, chargeSpeed = 24, knockback = 24 },
	drops = { "scrap_metal", "mutant_gland", "raw_meat" },
	lootChance = 0.8,
}

enemy {
	id = "mimic", name = "The Mimic", health = 80, speed = 15, damage = 20,
	attackRange = 7, attackRate = 1.5, vision = 80, hearing = 1.8, fov = 150,
	aggression = 0.8, color = Color3.fromRGB(112, 108, 96), scale = 1.1,
	tiers = { 3, 4 }, habitat = { "Suburbs", "Forest", "City" },
	desc = "It copies the sound of a crying child, of footsteps, of your own radio. Then it waits.",
	behaviour = { mimic = true, lureCooldown = 18, ambushRange = 30 },
	drops = { "mutant_gland" },
	lootChance = 0.55,
}

enemy {
	id = "burrower", name = "The Burrower", health = 110, speed = 10, damage = 24,
	attackRange = 8, attackRate = 1.8, vision = 60, hearing = 2.0, fov = 100,
	aggression = 0.85, color = Color3.fromRGB(84, 74, 62), scale = 1.2,
	tiers = { 3, 4 }, habitat = { "Wasteland", "Forest" },
	desc = "Moves under the soil and erupts beneath its prey. Watch the ground.",
	behaviour = { burrow = true, burrowTrigger = 34, emerge = true },
	drops = { "mutant_gland", "raw_meat" },
	lootChance = 0.6,
}

enemy {
	id = "stalker", name = "The Stalker", health = 140, speed = 18, damage = 28,
	attackRange = 8, attackRate = 1.6, vision = 140, hearing = 1.2, fov = 110,
	aggression = 0.9, color = Color3.fromRGB(78, 80, 74), scale = 1.15,
	tiers = { 4, 5 }, habitat = { "City", "Military", "Forest" },
	desc = "It only hunts the isolated. Stay close to another survivor and it will hesitate.",
	behaviour = { solitudeHunter = true, fleeBelowHealth = 0.25, retreatTime = 8 },
	drops = { "scrap_metal", "mutant_gland" },
	lootChance = 0.7,
}

enemy {
	id = "contaminated", name = "The Contaminated", health = 160, speed = 11, damage = 26,
	attackRange = 7, attackRate = 1.5, vision = 85, hearing = 1.0, fov = 130,
	aggression = 0.8, color = Color3.fromRGB(132, 148, 70), scale = 1.3,
	tiers = { 5, 6 }, habitat = { "Industrial", "Wasteland", "Military" },
	desc = "Bloated with fallout. Killing it up close raises your own dose.",
	behaviour = { radiationAura = 0.9, deathCloud = true },
	drops = { "chemicals", "mutant_gland" },
	lootChance = 0.65,
}

enemy {
	id = "pale_one", name = "The Pale One", health = 260, speed = 20, damage = 42,
	attackRange = 9, attackRate = 1.7, vision = 200, hearing = 2.4, fov = 160,
	aggression = 1.0, color = Color3.fromRGB(206, 202, 194), scale = 1.35,
	nocturnal = true, tiers = { 5, 6 }, habitat = { "Wasteland", "Forest", "Military" },
	desc = "Only at night. Only when you are alone. Only once.",
	behaviour = { nocturnal = true, stalkerAlpha = true, fearAura = 4.0 },
	drops = { "mutant_gland", "antibiotics", "core_sample" },
	lootChance = 0.9,
}

function Enemies.byRegion(region)
	local list = {}
	for _, def in pairs(Enemies) do
		if type(def) ~= "table" then continue end
		for _, r in ipairs(def.habitat) do
			if r == region then
				table.insert(list, def)
				break
			end
		end
	end
	return list
end

function Enemies.byTier(tier)
	local list = {}
	for _, def in pairs(Enemies) do
		if type(def) ~= "table" then continue end
		for _, t in ipairs(def.tiers) do
			if t == tier then
				table.insert(list, def)
				break
			end
		end
	end
	return list
end

return Enemies
