--!strict
--[[
	WeaponDefinitions — combat statistics for every weapon in the game.

	kind        "melee" | "firearm" | "throwable"
	damage      base damage per hit
	range       melee reach (studs) or hitscan distance
	fireRate    seconds between attacks
	ammo        ammo item id (firearms)
	magazine    rounds per magazine/reload
	reload      reload time in seconds
	spread      radians of random deviation while hip firing
	aimSpread   spread while aiming
	recoil      camera kick per shot (degrees)
	pellets     projectiles per trigger pull
	durability  durability lost per hit
	noise       studs of sound generated
	falloff     { start, end, minMultiplier }
]]

local Weapons = {}

local function weapon(t)
	t.kind = t.kind or "melee"
	t.durability = t.durability or 1
	t.noise = t.noise or 30
	t.pellets = t.pellets or 1
	t.spread = t.spread or 0.01
	t.aimSpread = t.aimSpread or t.spread * 0.35
	t.recoil = t.recoil or 0
	t.falloff = t.falloff or { start = 120, finish = 400, min = 0.42 }
	Weapons[t.id] = t
	return t
end

weapon {
	id = "knife", kind = "melee", name = "Survival Knife", damage = 24, range = 7.5,
	fireRate = 0.42, noise = 8, durability = 0.9, stamina = 4,
	anim = "Stab", tags = { "blade" },
}

weapon {
	id = "hatchet", kind = "melee", name = "Fire Axe", damage = 46, range = 8.5,
	fireRate = 0.78, noise = 16, durability = 1.6, stamina = 9,
	anim = "Chop", tags = { "heavy", "woodcutting" },
}

weapon {
	id = "crowbar", kind = "melee", name = "Crowbar", damage = 31, range = 9.0,
	fireRate = 0.6, noise = 12, durability = 1.1, stamina = 6,
	anim = "Swing", tags = { "pry" },
}

weapon {
	id = "bat", kind = "melee", name = "Wrapped Bat", damage = 34, range = 10.5,
	fireRate = 0.66, noise = 14, durability = 1.3, stamina = 7,
	anim = "Swing",
}

weapon {
	id = "wrench", kind = "melee", name = "Pipe Wrench", damage = 40, range = 8.0,
	fireRate = 0.9, noise = 18, durability = 1.5, stamina = 10,
	anim = "Chop",
}

weapon {
	id = "pistol", kind = "firearm", name = "PM-9 Pistol", damage = 26, range = 320,
	fireRate = 0.22, ammo = "ammo_9mm", magazine = 12, reload = 1.7, spread = 0.028,
	aimSpread = 0.006, recoil = 1.6, noise = 260, durability = 0.06,
	pellets = 1, falloff = { start = 90, finish = 300, min = 0.55 },
	anim = "Fire", tags = { "gun", "sidearm" },
}

weapon {
	id = "shotgun", kind = "firearm", name = "Sawn-off Shotgun", damage = 15, range = 140,
	fireRate = 0.85, ammo = "ammo_shell", magazine = 2, reload = 2.4, spread = 0.085,
	aimSpread = 0.05, recoil = 6.5, noise = 380, durability = 0.2,
	pellets = 8, falloff = { start = 25, finish = 110, min = 0.20 },
	anim = "Fire", tags = { "gun", "shotgun" },
}

weapon {
	id = "rifle", kind = "firearm", name = "AK-pattern Rifle", damage = 34, range = 480,
	fireRate = 0.115, ammo = "ammo_762", magazine = 30, reload = 2.8, spread = 0.032,
	aimSpread = 0.009, recoil = 2.1, noise = 420, durability = 0.04,
	pellets = 1, falloff = { start = 150, finish = 460, min = 0.6 },
	anim = "Fire", tags = { "gun", "rifle", "auto" },
}

weapon {
	id = "bolt_rifle", kind = "firearm", name = "Hunting Rifle", damage = 82, range = 520,
	fireRate = 1.5, ammo = "ammo_762", magazine = 5, reload = 3.1, spread = 0.02,
	aimSpread = 0.0025, recoil = 4.2, noise = 440, durability = 0.08,
	pellets = 1, falloff = { start = 260, finish = 520, min = 0.75 },
	anim = "Fire", tags = { "gun", "rifle", "scope" },
}

weapon {
	id = "crossbow", kind = "firearm", name = "Improvised Crossbow", damage = 58, range = 260,
	fireRate = 1.15, ammo = "bolt", magazine = 1, reload = 1.9, spread = 0.012,
	aimSpread = 0.004, recoil = 1.0, noise = 22, durability = 0.12,
	pellets = 1, projectile = "bolt", falloff = { start = 120, finish = 260, min = 0.7 },
	anim = "Fire", tags = { "crossbow", "silent" },
}

weapon {
	id = "molotov", kind = "throwable", name = "Molotov", damage = 18, range = 55,
	fireRate = 1.6, noise = 90, durability = 1, radius = 16, duration = 14,
	anim = "Throw", tags = { "fire", "throwable" },
}

return Weapons
