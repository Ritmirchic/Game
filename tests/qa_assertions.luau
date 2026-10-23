--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    tests/qa_assertions.luau
    6-Stage QA, Integration, Exploit-Rejection, and 300-Tick Performance Benchmark Suite.
    Executes directly inside standalone Luau VM against the real production codebase.
]]

do
	local totalAssertions = 0
	local stageCounts: { [string]: number } = {}
	local currentStage = "Stage1"

	local function setStage(name: string)
		currentStage = name
		stageCounts[name] = 0
		print(string.format("\n=== [%s] ===", name))
	end

	local function check(cond: any, msg: string)
		if not cond then
			error(string.format("[%s] ASSERTION FAILED: %s", currentStage, msg), 2)
		end
		totalAssertions += 1
		stageCounts[currentStage] = (stageCounts[currentStage] or 0) + 1
	end

	local function createMockPlayer(userId: number, name: string): any
		local p = makeInstance("Player", name)
		p.UserId = userId
		local char = makeInstance("Model", name)
		local hrp = makeInstance("Part", "HumanoidRootPart")
		hrp.Size = gEnv.Vector3.new(2, 2, 1)
		hrp.CFrame = gEnv.CFrame.new(0, -19.5, 620)
		hrp.CanCollide = false
		hrp.Parent = char
		local head = makeInstance("Part", "Head")
		head.Size = gEnv.Vector3.new(1.2, 1.2, 1.2)
		head.CFrame = gEnv.CFrame.new(0, -18.0, 620)
		head.CanCollide = false
		head.Parent = char
		local hum = makeInstance("Humanoid", "Humanoid")
		hum.Health = 100
		hum.MaxHealth = 100
		hum.WalkSpeed = 12.5
		hum.JumpHeight = 6.2
		hum.Parent = char
		char.PrimaryPart = hrp
		char.Parent = WorkspaceInst
		p.Character = char
		table.insert(mockPlayersList, p)
		return p
	end

	local RS = gEnv.game:GetService("ReplicatedStorage")
	local SharedFolder = RS.Shared

	local RemotesFolder = makeInstance("Folder", "Remotes")
	RemotesFolder.Parent = RS

	local SSS = gEnv.game:GetService("ServerScriptService")
	local ServicesFolder = SSS.Services

	local SS = gEnv.game:GetService("ServerStorage")
	for _, fName in ipairs({ "EnemyTemplates", "WeaponTemplates", "BuildingTemplates", "ItemTemplates", "LootTemplates" }) do
		local f = makeInstance("Folder", fName)
		f.Parent = SS
	end

	local WS = gEnv.game:GetService("Workspace")
	for _, fName in ipairs({ "World", "InteractionObjects", "EnemySpawns", "ActiveEnemies" }) do
		local f = makeInstance("Folder", fName)
		f.Parent = WS
	end

	local sharedNames = {
		"Types",
		"Config",
		"UtilityModules",
		"NetworkSchema",
		"ItemDefinitions",
		"WeaponDefinitions",
		"CraftingRecipes",
		"BuildingDefinitions",
		"EnemyDefinitions",
		"QuestDefinitions",
		"AnimationDefinitions",
	}
	local Shared: { [string]: any } = {}
	for _, name in ipairs(sharedNames) do
		Shared[name] = gEnv.require(SharedFolder[name])
	end

	for _, evName in pairs(Shared.NetworkSchema.EVENTS) do
		local re = makeInstance("RemoteEvent", evName)
		re.Parent = RemotesFolder
	end
	for _, fnName in pairs(Shared.NetworkSchema.FUNCTIONS) do
		local rf = makeInstance("RemoteFunction", fnName)
		rf.Parent = RemotesFolder
	end

	-- =========================================================================
	-- STAGE 1: STATIC SCHEMA, DEFINITIONS & ASSET INTEGRITY AUDIT
	-- =========================================================================
	setStage("Stage 1: Shared Definitions, Regions, 10 Mutants, Weapons, Crafting & Quests")

	local regionCount = 0
	for rKey, rData in pairs(Shared.Config.REGIONS) do
		regionCount += 1
		check(type(rData.DisplayName) == "string" and #rData.DisplayName > 3, "Region missing DisplayName: " .. rKey)
		check(rData.Radius > 0, "Region radius invalid: " .. rKey)
	end
	check(regionCount == 7, "Expected EmergencyShelter + 6 surface regions (7 total), got " .. tostring(regionCount))

	local itemCount = 0
	for itemId, def in pairs(Shared.ItemDefinitions.ITEMS) do
		itemCount += 1
		check(def.Width >= 1 and def.Height >= 1, "Invalid grid size on " .. itemId)
		check(def.WeightKg >= 0, "Negative weight on " .. itemId)
	end
	check(itemCount >= 40, "Expected >= 40 survival items, got " .. tostring(itemCount))

	local weaponCount = 0
	for wId, wDef in pairs(Shared.WeaponDefinitions.WEAPONS) do
		weaponCount += 1
		check(wDef.BaseDamage > 0, "Weapon damage <= 0: " .. wId)
		check(wDef.ADSFov > 0, "Weapon ADSFov <= 0: " .. wId)
	end
	check(weaponCount == 11, "Expected 11 distinct weapons, got " .. tostring(weaponCount))

	check(#Shared.CraftingRecipes.ORDERED_LIST == 25, "Expected 25 ordered crafting recipes")
	for _, rec in ipairs(Shared.CraftingRecipes.ORDERED_LIST) do
		check(Shared.ItemDefinitions.Get(rec.OutputItemId) ~= nil, "Invalid recipe output " .. rec.OutputItemId)
		for _, ing in ipairs(rec.Ingredients) do
			check(Shared.ItemDefinitions.Get(ing.ItemId) ~= nil, "Invalid recipe ingredient " .. ing.ItemId)
		end
	end

	local buildCount = 0
	for bId, bDef in pairs(Shared.BuildingDefinitions.STRUCTURES) do
		buildCount += 1
		check(bDef.MaxHP >= 100, "Building MaxHP < 100 on " .. bId)
		check(#bDef.Costs >= 1, "Building has no material costs: " .. bId)
	end
	check(buildCount == 12, "Expected 12 base building structures, got " .. tostring(buildCount))

	local expectedSpecies = {
		"Hollow", "Crawler", "Watcher", "Screamer", "Brute",
		"Mimic", "Burrower", "Stalker", "Contaminated", "PaleOne",
	}
	for _, sp in ipairs(expectedSpecies) do
		local eDef = Shared.EnemyDefinitions.SPECIES[sp]
		check(eDef ~= nil, "Missing mutant species definition: " .. sp)
		check(eDef.MaxHealth >= 50, "Mutant MaxHealth invalid: " .. sp)
	end
	for _, mState in ipairs({ "Idle", "Walk", "Run", "Investigate", "Search", "Attack", "DamageReact", "Retreat", "Death" }) do
		check(Shared.AnimationDefinitions.MUTANT_STATES[mState] ~= nil, "Missing mutant animation state: " .. mState)
	end

	local mainQuests, sideQuests, loreCount = 0, 0, 0
	for _, q in pairs(Shared.QuestDefinitions.QUESTS) do
		if q.Kind == "Main" then
			mainQuests += 1
		else
			sideQuests += 1
		end
	end
	for _ in pairs(Shared.QuestDefinitions.LORE_ENTRIES) do
		loreCount += 1
	end
	check(mainQuests == 6 and sideQuests == 6, "Expected 6 Main + 6 Side quests")
	check(loreCount == 18, "Expected 18 lore entries, got " .. tostring(loreCount))
	print(string.format("  -> Stage 1 Passed (%d assertions)", stageCounts[currentStage]))

	-- =========================================================================
	-- STAGE 2: ALL 13 SERVER SERVICES INITIALIZATION & PLAYER DATA PERSISTENCE
	-- =========================================================================
	setStage("Stage 2: Server Service Registry, Session Locking & Starter Shelter Loadout")

	local serviceNames = {
		"SecurityService",
		"PlayerDataService",
		"SurvivalService",
		"InventoryService",
		"LootService",
		"CraftingService",
		"BuildingService",
		"CombatService",
		"EnemyAIService",
		"WeatherService",
		"WorldEventService",
		"QuestService",
		"PartyService",
	}
	local ServiceRegistry: { [string]: any } = {}
	local sentPackets: { { Player: any, Event: string, Data: any } } = {}

	ServiceRegistry.SendToPlayer = function(player: any, eventName: string, payload: any)
		table.insert(sentPackets, { Player = player, Event = eventName, Data = payload })
	end
	ServiceRegistry.BroadcastAll = function(eventName: string, payload: any)
		for _, plr in ipairs(gEnv.game:GetService("Players"):GetPlayers()) do
			table.insert(sentPackets, { Player = plr, Event = eventName, Data = payload })
		end
	end

	for _, sName in ipairs(serviceNames) do
		ServiceRegistry[sName] = gEnv.require(ServicesFolder[sName])
	end
	for _, sName in ipairs(serviceNames) do
		ServiceRegistry[sName]:Init(ServiceRegistry)
	end
	for _, sName in ipairs(serviceNames) do
		if type(ServiceRegistry[sName].Start) == "function" then
			ServiceRegistry[sName]:Start()
		end
	end

	local p1 = createMockPlayer(1001, "Survivor_Elena")
	local p2 = createMockPlayer(1002, "Survivor_Marcus")

	local profile1 = ServiceRegistry.PlayerDataService:LoadPlayerProfile(p1)
	local profile2 = ServiceRegistry.PlayerDataService:LoadPlayerProfile(p2)
	check(profile1 ~= nil and profile2 ~= nil, "PlayerDataService failed to load profiles")
	check(profile1.Vitals.Health == 100, "Initial health must be 100")

	ServiceRegistry.InventoryService:GrantStarterKit(p1)
	ServiceRegistry.InventoryService:GrantStarterKit(p2)
	ServiceRegistry.QuestService:EnsureInitialQuests(p1)
	ServiceRegistry.QuestService:EnsureInitialQuests(p2)

	check(ServiceRegistry.InventoryService:CountItem(p1, "DamagedFlashlight") == 1, "Missing starter DamagedFlashlight")
	check(ServiceRegistry.InventoryService:CountItem(p1, "SmallBackpack") == 1, "Missing starter SmallBackpack")
	check(ServiceRegistry.InventoryService:CountItem(p1, "OldFieldRadio") == 1, "Missing starter OldFieldRadio")
	check(ServiceRegistry.InventoryService:CountItem(p1, "Echo07Note") == 1, "Missing starter Echo07Note")
	check(ServiceRegistry.InventoryService:CountItem(p1, "HalfCanteen") >= 1, "Missing starter HalfCanteen")
	check(profile1.Inventory.Rows >= 6, "SmallBackpack should expand inventory rows to >= 6")

	local savedOk = ServiceRegistry.PlayerDataService:SavePlayerProfile(p1, false)
	check(savedOk == true, "DataStore UpdateAsync save failed")
	print(string.format("  -> Stage 2 Passed (%d assertions)", stageCounts[currentStage]))

	-- =========================================================================
	-- STAGE 3: SURVIVAL VITALS, 6-REGION TRAUMA, GRID INVENTORY, CRAFTING & BUILDING
	-- =========================================================================
	setStage("Stage 3: Body-Part Trauma, Grid Spatial Ops, Crafting & Base Building")

	ServiceRegistry.SurvivalService:InflictLocalizedDamage(p1, 44, "LeftLeg", {
		BleedChance = 1.0,
		FractureChance = 1.0,
	})
	local leftLeg = profile1.BodyRegions.LeftLeg
	check(leftLeg.HP < leftLeg.MaxHP, "LeftLeg HP did not drop")
	check(leftLeg.Conditions.Bleeding == true, "Heavy slash should trigger LeftLeg Bleeding")
	check(leftLeg.Conditions.Fracture == true, "Heavy limb trauma should trigger LeftLeg Fracture")
	check(profile1.Vitals.BleedStacks >= 1, "BleedStacks should increment from limb bleeding")

	local hpBeforeBleed = profile1.Vitals.Health
	ServiceRegistry.SurvivalService:StepPlayer(p1, 2.0)
	check(profile1.Vitals.Health < hpBeforeBleed, "Active bleeding must drain player Health over time")
	check(p1.Character.Humanoid.WalkSpeed < Shared.Config.MOVEMENT.WalkSpeed, "Fractured leg must reduce WalkSpeed")

	local okBand, bandUids = ServiceRegistry.InventoryService:AddItem(p1, "SterileBandage", 1)
	local okSplint, splintUids = ServiceRegistry.InventoryService:AddItem(p1, "OrthopedicSplint", 1)
	check(okBand and bandUids and #bandUids >= 1, "Failed to grant SterileBandage")
	check(okSplint and splintUids and #splintUids >= 1, "Failed to grant OrthopedicSplint")

	local usedBand = ServiceRegistry.InventoryService:UseItem(p1, bandUids[1], "LeftLeg")
	local usedSplint = ServiceRegistry.InventoryService:UseItem(p1, splintUids[1], "LeftLeg")
	check(usedBand == true, "Using SterileBandage on LeftLeg failed")
	check(usedSplint == true, "Using OrthopedicSplint on LeftLeg failed")
	check(leftLeg.Conditions.Bleeding == false, "SterileBandage must clear LeftLeg Bleeding")
	check(leftLeg.Conditions.Fracture == false, "OrthopedicSplint must clear LeftLeg Fracture")

	local okRifle, rifleUids = ServiceRegistry.InventoryService:AddItem(p1, "HuntingRifle", 1)
	check(okRifle and rifleUids and #rifleUids == 1, "Failed to add 4x2 HuntingRifle to grid")
	local rifleInst = profile1.Inventory.Items[rifleUids[1]]
	check(rifleInst ~= nil, "Rifle instance missing")

	local badMove = ServiceRegistry.InventoryService:MoveOrRotateItem(p1, rifleInst.UID, 8, 5, false)
	check(badMove == false, "Out-of-bounds grid placement must be rejected")
	check(ServiceRegistry.InventoryService:SortInventory(p1) == true, "Grid Auto-Sort failed")

	ServiceRegistry.InventoryService:AddItem(p1, "WoodPlank", 6)
	ServiceRegistry.InventoryService:AddItem(p1, "ScrapMetal", 6)
	ServiceRegistry.InventoryService:AddItem(p1, "DuctTape", 4)
	p1.Character.HumanoidRootPart.CFrame = gEnv.CFrame.new(100, 3, 500)

	local noBenchOk, noBenchErr = ServiceRegistry.CraftingService:CraftRecipe(p1, "CraftSpikedBat")
	check(noBenchOk == false, "Workbench craft without station must fail (got " .. tostring(noBenchErr) .. ")")

	local wbPart = makeInstance("Part", "TestWorkbench")
	wbPart.CFrame = gEnv.CFrame.new(103, 3, 500)
	wbPart:SetAttribute("StationType", "Workbench")
	wbPart.Parent = WS.World

	local craftOk, craftErr = ServiceRegistry.CraftingService:CraftRecipe(p1, "CraftSpikedBat")
	check(craftOk == true, "Crafting SpikedBat at Workbench failed: " .. tostring(craftErr))
	check(ServiceRegistry.InventoryService:CountItem(p1, "SpikedBat") == 1, "Crafted SpikedBat missing from inventory")

	ServiceRegistry.InventoryService:AddItem(p1, "WoodPlank", 10)
	ServiceRegistry.InventoryService:AddItem(p1, "ScrapMetal", 6)
	local placedOk, structUid = ServiceRegistry.BuildingService:PlaceStructure(p1, "WoodWall", gEnv.Vector3.new(108, 4.5, 500), 0)
	check(placedOk == true and type(structUid) == "string", "PlaceStructure WoodWall failed: " .. tostring(structUid))

	ServiceRegistry.BuildingService:DamageStructure(structUid :: string, 120)
	local repOk = ServiceRegistry.BuildingService:RepairStructure(p1, structUid :: string)
	check(repOk == true, "RepairStructure failed")

	local dismOk = ServiceRegistry.BuildingService:DismantleStructure(p1, structUid :: string)
	check(dismOk == true, "DismantleStructure failed")
	print(string.format("  -> Stage 3 Passed (%d assertions)", stageCounts[currentStage]))

	-- =========================================================================
	-- STAGE 4: COMBAT BALLISTICS, 10 MUTANT AI SPECIES, QUESTS & CO-OP REVIVE
	-- =========================================================================
	setStage("Stage 4: Server-Authoritative Combat, 10-Species AI FSM, Quests & Co-op")

	local spawnedIds: { string } = {}
	for idx, sp in ipairs(expectedSpecies) do
		local pos = gEnv.Vector3.new(200 + idx * 14, 3.5, 500)
		local ag = ServiceRegistry.EnemyAIService:SpawnMutant(sp, pos)
		check(ag ~= nil, "Failed to spawn mutant species: " .. sp)
		table.insert(spawnedIds, ag.Id)
	end
	check(ServiceRegistry.EnemyAIService:GetActiveMutantCount() == 10, "Expected 10 active mutants")

	local hollowId = spawnedIds[1]
	local hollowAgent = ServiceRegistry.EnemyAIService:GetAgent(hollowId)
	check(hollowAgent ~= nil, "GetAgent failed for Hollow")

	ServiceRegistry.EnemyAIService:NotifySound(gEnv.Vector3.new(214, 3.5, 502), 95, p1)
	gEnv.AdvanceClock(1.0)
	ServiceRegistry.EnemyAIService:StepAgent(hollowAgent, 0.25)
	check(hollowAgent.State ~= "IDLE", "Hollow should react to nearby 95-stud acoustic gunshot")

	ServiceRegistry.InventoryService:AddItem(p1, "Ammo308", 20)
	local eqOk = ServiceRegistry.InventoryService:EquipItem(p1, rifleInst.UID, "Primary")
	check(eqOk == true, "Equipping HuntingRifle to Primary failed")
	p1.Character:SetAttribute("ActiveWeaponId", "HuntingRifle")

	local mHead = hollowAgent.Model:FindFirstChild("Head") or hollowAgent.Root
	check(mHead ~= nil, "Mutant Head/Root part missing")
	mHead.CanCollide = true
	mHead.Size = gEnv.Vector3.new(2.2, 2.2, 2.2)

	p1.Character.HumanoidRootPart.CFrame = gEnv.CFrame.new(mHead.Position - gEnv.Vector3.new(0, 1.4, 14))
	local shotOrigin = p1.Character.HumanoidRootPart.Position + gEnv.Vector3.new(0, 1.4, 0)
	local aimDir = (mHead.Position - shotOrigin).Unit

	for _ = 1, 4 do
		gEnv.AdvanceClock(1.3)
		ServiceRegistry.CombatService:HandleWeaponFire(p1, {
			Slot = "Primary",
			Origin = shotOrigin,
			Direction = aimDir,
			ADS = true,
		})
		if ServiceRegistry.EnemyAIService:GetActiveMutantCount() <= 9 then
			break
		end
	end
	check(ServiceRegistry.EnemyAIService:GetActiveMutantCount() <= 9, ".308 headshots must eliminate Hollow mutant")

	-- Cooperative Party Formation, Downed State, and Teammate Revive
	local partyOk, partyId = ServiceRegistry.PartyService:CreateParty(p1)
	if type(partyOk) == "string" then
		partyId = partyOk
	end
	check(type(partyId) == "string", "Party creation failed")
	local invOk = ServiceRegistry.PartyService:InvitePlayer(p1, p2.UserId)
	check(invOk == true, "Party invite failed")
	local accOk = ServiceRegistry.PartyService:AcceptInvite(p2, partyId)
	check(accOk == true, "Party accept failed")
	check(ServiceRegistry.PartyService:AreInSameParty(p1.UserId, p2.UserId) == true, "Players must be in same party")

	p1.Character.HumanoidRootPart.CFrame = gEnv.CFrame.new(100, 3, 500)
	p2.Character.HumanoidRootPart.CFrame = gEnv.CFrame.new(103, 3, 500)
	ServiceRegistry.SurvivalService:InflictLocalizedDamage(p2, 260, "Torso", {})
	check(profile2.Vitals.IsDowned == true, "Co-op party member should enter IsDowned state instead of instant death")

	local revOk = ServiceRegistry.PartyService:ReviveTeammate(p1, p2.UserId)
	check(revOk == true and profile2.Vitals.IsDowned == false, "Teammate cooperative revive failed")

	-- Full Act I -> Act VI Main Story Quest Progression Verification
	ServiceRegistry.QuestService:DiscoverLore(p1, "Lore_01_EchoTerminal")
	ServiceRegistry.QuestService:NotifyProgress(p1, "VisitRegion", "DeadSuburbs", 1)
	check(profile1.Quests["Main_01_EchoAwakening"].Status == "Completed", "Act I quest failed to complete")
	check(profile1.Quests["Main_02_SuburbanRelay"] ~= nil, "Act II quest failed to auto-unlock")
	check(profile2.Quests["Main_01_EchoAwakening"].Status == "Completed", "Co-op shared quest progress failed for party peer")

	ServiceRegistry.QuestService:DiscoverLore(p1, "Lore_03_EvacNotice")
	ServiceRegistry.QuestService:NotifyProgress(p1, "Interact", "Relay_Suburbs", 1)
	check(profile1.Quests["Main_02_SuburbanRelay"].Status == "Completed", "Act II quest failed to complete")

	ServiceRegistry.QuestService:NotifyProgress(p1, "VisitRegion", "ForgottenCity", 1)
	ServiceRegistry.QuestService:DiscoverLore(p1, "Lore_09_VanceTriageTape")
	check(profile1.Quests["Main_03_CityTriage"].Status == "Completed", "Act III quest failed to complete")

	ServiceRegistry.QuestService:DiscoverLore(p1, "Lore_06_RangerRecording")
	ServiceRegistry.QuestService:DiscoverLore(p1, "Lore_12_SubLabManifest")
	check(profile1.Quests["Main_04_BlackwoodAndAsh"].Status == "Completed", "Act IV quest failed to complete")

	ServiceRegistry.QuestService:NotifyProgress(p1, "VisitRegion", "MilitaryExclusionZone", 1)
	ServiceRegistry.QuestService:DiscoverLore(p1, "Lore_15_LazarusDirective")
	ServiceRegistry.InventoryService:AddItem(p1, "SignalDecoder", 1)
	check(profile1.Quests["Main_05_ExclusionProtocol"].Status == "Completed", "Act V quest failed to complete")

	ServiceRegistry.QuestService:NotifyProgress(p1, "VisitRegion", "RedWasteland", 1)
	ServiceRegistry.QuestService:DiscoverLore(p1, "Lore_18_FinalTransmission")
	ServiceRegistry.QuestService:NotifyProgress(p1, "Interact", "Console_TowerZero", 1)
	check(profile1.Quests["Main_06_OriginOfTheSignal"].Status == "Completed", "Act VI finale quest failed to complete")
	print(string.format("  -> Stage 4 Passed (%d assertions)", stageCounts[currentStage]))

	-- =========================================================================
	-- STAGE 5: ANTI-EXPLOIT SECURITY & MALFORMED PACKET REJECTION
	-- =========================================================================
	setStage("Stage 5: Server-Authoritative Anti-Exploit & Rate-Limiter Stress")

	gEnv.AdvanceClock(2.0)
	local activeBeforeSpoof = ServiceRegistry.EnemyAIService:GetActiveMutantCount()
	local spoofOk, spoofErr = ServiceRegistry.CombatService:HandleWeaponFire(p1, {
		Slot = "Primary",
		Origin = gEnv.Vector3.new(999, 50, 999),
		Direction = gEnv.Vector3.new(0, 0, -1),
		ADS = false,
	})
	check(spoofOk == false and spoofErr == "OriginOutOfBounds", "Spoofed distant weapon fire origin was not rejected")
	check(
		ServiceRegistry.EnemyAIService:GetActiveMutantCount() == activeBeforeSpoof,
		"Spoofed weapon fire altered mutant population"
	)

	check(Shared.NetworkSchema.ValidateVector3(gEnv.Vector3.new(0 / 0, 1, 1)) == false, "NaN Vector3 passed validation")
	check(Shared.NetworkSchema.ValidateNumber(math.huge, 0, 100) == false, "Inf number passed validation")

	local allowedCount = 0
	for _ = 1, 45 do
		if ServiceRegistry.SecurityService:CheckRateLimit(p2, "CraftRequest") then
			allowedCount += 1
		end
	end
	check(allowedCount < 20, "Rate limiter failed to throttle 45 rapid CraftRequest packets (allowed " .. allowedCount .. ")")

	local farCrate = makeInstance("Part", "FarLootCrate")
	farCrate.CFrame = gEnv.CFrame.new(-800, 3, -800)
	farCrate:SetAttribute("IsLootContainer", true)
	farCrate:SetAttribute("ContainerId", "Far_Military_999")
	farCrate:SetAttribute("LootTable", "Military")
	farCrate.Parent = WS.World
	ServiceRegistry.LootService:RegisterContainer(farCrate)

	local openFarOk, openFarErr = ServiceRegistry.LootService:OpenContainer(p2, "Far_Military_999")
	check(openFarOk == false, "Distant loot container open (>14 studs) must be rejected: " .. tostring(openFarErr))
	print(string.format("  -> Stage 5 Passed (%d assertions)", stageCounts[currentStage]))

	-- =========================================================================
	-- STAGE 6: CLIENT CONTROLLERS SYNTAX/LOAD + 300-TICK SERVER PERFORMANCE BENCHMARK
	-- =========================================================================
	setStage("Stage 6: Client Controller Compilation & 300-Tick Runtime Benchmark")

	local controllerNames = {
		"ClientController",
		"CameraController",
		"MovementController",
		"AnimationController",
		"WeaponController",
		"AudioController",
		"EffectsController",
		"UIController",
	}
	for _, cName in ipairs(controllerNames) do
		local src = MODULE_SOURCES["Controllers." .. cName]
		check(type(src) == "string" and #src > 100, "Missing source for controller: " .. cName)
		local fn, err = loadstring(src, "=" .. cName)
		check(fn ~= nil, "Client controller failed to compile: " .. cName .. " -> " .. tostring(err))
	end

	local benchAgentIds: { string } = {}
	for _, mId in ipairs(spawnedIds) do
		if ServiceRegistry.EnemyAIService:GetAgent(mId) ~= nil then
			table.insert(benchAgentIds, mId)
		end
	end
	while ServiceRegistry.EnemyAIService:GetActiveMutantCount() < 24 do
		local idx = ServiceRegistry.EnemyAIService:GetActiveMutantCount() + 1
		local sp = expectedSpecies[((idx - 1) % #expectedSpecies) + 1]
		local ag = ServiceRegistry.EnemyAIService:SpawnMutant(
			sp,
			gEnv.Vector3.new(-300 + idx * 22, 3.5, -200 + (idx % 5) * 30)
		)
		if ag then
			table.insert(benchAgentIds, ag.Id)
		end
	end
	check(ServiceRegistry.EnemyAIService:GetActiveMutantCount() == 24, "Expected 24 active mutants for stress benchmark")

	local t0 = gEnv.RealOsClock()
	for tickIdx = 1, 300 do
		gEnv.AdvanceClock(0.1)
		for _, mId in ipairs(benchAgentIds) do
			local ag = ServiceRegistry.EnemyAIService:GetAgent(mId)
			if ag then
				ServiceRegistry.EnemyAIService:StepAgent(ag, 0.1)
			end
		end
		if tickIdx % 5 == 0 then
			ServiceRegistry.SurvivalService:StepPlayer(p1, 0.5)
			ServiceRegistry.SurvivalService:StepPlayer(p2, 0.5)
			ServiceRegistry.WeatherService:Step(0.5)
		end
	end
	local elapsedMs = (gEnv.RealOsClock() - t0) * 1000.0
	local avgTickMs = elapsedMs / 300.0
	print(string.format("  -> 300-Tick Server + 24-Mutant AI Benchmark: total=%.2f ms (avg=%.4f ms/tick)", elapsedMs, avgTickMs))
	check(avgTickMs < 4.0, string.format("Server tick exceeded 4.0 ms budget: %.4f ms", avgTickMs))

	ServiceRegistry.PlayerDataService:SavePlayerProfile(p1, true)
	ServiceRegistry.PlayerDataService:SavePlayerProfile(p2, true)
	print(string.format("  -> Stage 6 Passed (%d assertions)", stageCounts[currentStage]))

	print(string.format("\nALL 6 QA STAGES PASSED — %d TOTAL ASSERTIONS VERIFIED!", totalAssertions))
end
