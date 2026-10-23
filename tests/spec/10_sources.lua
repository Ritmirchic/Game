--[=[
	Static source checks that run on the real files in src/.

	These are honesty checks: no invented Roblox asset ids, no references to
	remote names that do not exist, no placeholder code left in shipped modules.
]=]

local ROOT = ROOT_DIR or "."
local RemoteDef = require("Remotes")

local function readTree(dir)
	local out = {}
	local function walk(path)
		for _, name in ipairs(TestHost.listDir(path) or {}) do
			local full = path .. "/" .. name
			if TestHost.isDir(full) then
				walk(full)
			elseif string.sub(name, -4) == ".lua" then
				out[#out + 1] = { path = full, name = name, src = TestHost.readFile(full) or "" }
			end
		end
	end
	walk(dir)
	return out
end

local files = {
	shared = readTree(ROOT .. "/src/shared"),
	server = readTree(ROOT .. "/src/server"),
	client = readTree(ROOT .. "/src/client"),
}

local function allFiles()
	local out = {}
	for _, group in pairs(files) do
		for _, file in ipairs(group) do
			table.insert(out, file)
		end
	end
	return out
end

local function countLines(src)
	local n = 0
	for _ in string.gmatch(src, "\n") do n += 1 end
	return n + 1
end

describe("Sources: asset honesty", function()
	it("contains no numeric Roblox asset ids", function()
		local offenders = {}
		for _, file in ipairs(allFiles()) do
			-- rbxasset://textures/... (built-in engine textures) is fine;
			-- rbxassetid://123 or "SoundId = 123" is not, because those ids are
			-- not ours to publish.
			if string.find(file.src, "rbxassetid://") or string.find(file.src, "rbxthumb://") then
				table.insert(offenders, file.path)
			end
			local numeric = string.match(file.src, "SoundId%s*=%s*\"%d+\"")
			if numeric then
				table.insert(offenders, file.path)
			end
		end
		eq(#offenders, 0, "fabricated asset ids in: " .. table.concat(offenders, ", "))
	end)

	it("declares the audio situation explicitly", function()
		local audio = TestHost.readFile(ROOT .. "/src/client/AudioController.lua") or ""
		expect(string.find(audio, "no audio assets") or string.find(audio, "no licensed audio"),
			"AudioController must document that no audio ships")
		expect(string.find(audio, "id = \"\""), "audio library should be empty by default")
	end)

	it("has no leftover TODO/placeholder markers in shipped code", function()
		local offenders = {}
		for _, file in ipairs(allFiles()) do
			local lower = string.lower(file.src)
			if string.find(lower, "todo:") or string.find(lower, "fixme") or string.find(lower, "placeholder_") then
				table.insert(offenders, file.path)
			end
		end
		eq(#offenders, 0, "unfinished markers in: " .. table.concat(offenders, ", "))
	end)
end)

describe("Sources: structure", function()
	it("every shared module is non-trivial and returns a table", function()
		for _, file in ipairs(files.shared) do
			expect(countLines(file.src) > 8, file.name .. " is suspiciously short")
			expect(string.find(file.src, "return "), file.name .. " has no return statement")
			expect(not string.find(file.src, "require%(%s*script%.Parent%.%.%."), file.name .. " uses an unsafe require")
		end
	end)

	it("every server service implements Init and Start", function()
		for _, file in ipairs(files.server) do
			if string.find(file.name, "%.server%.lua$") then
				continue
			end
			expect(string.find(file.src, "%.Init%("), file.name .. " has no Init")
			expect(string.find(file.src, "%.Start%("), file.name .. " has no Start")
		end
	end)

	it("the bootstrap lists every service that exists on disk", function()
		local bootstrap = TestHost.readFile(ROOT .. "/src/server/Bootstrap.server.lua") or ""
		for _, file in ipairs(files.server) do
			if not string.find(file.name, "%.server%.lua$") and file.name ~= "Remotes.lua" then
				local moduleName = string.sub(file.name, 1, -5)
				expect(string.find(bootstrap, moduleName),
					"Bootstrap does not load " .. moduleName)
			end
		end
	end)
end)

describe("Sources: remotes", function()
	it("every remote the client fires exists in the definition", function()
		local declared = {}
		for _, name in ipairs(RemoteDef.Events) do declared[name] = "event" end
		for _, name in ipairs(RemoteDef.Functions) do declared[name] = "function" end
		local bad = {}
		for _, file in ipairs(files.client) do
			for name in string.gmatch(file.src, "fire%(\"([%w_]+)\"") do
				if not declared[name] then table.insert(bad, file.name .. " -> " .. name) end
			end
			for name in string.gmatch(file.src, "Network%.invoke%(\"([%w_]+)\"") do
				if not declared[name] then table.insert(bad, file.name .. " -> " .. name) end
			end
		end
		eq(#bad, 0, "undeclared remotes: " .. table.concat(bad, ", "))
	end)

	it("every server handler is bound to a declared remote", function()
		local declared = {}
		for _, name in ipairs(RemoteDef.Events) do declared[name] = true end
		local bad = {}
		for _, file in ipairs(files.server) do
			for name in string.gmatch(file.src, "remotes%.On%(\"([%w_]+)\"") do
				if not declared[name] then table.insert(bad, file.name .. " -> " .. name) end
			end
		end
		eq(#bad, 0, "unknown handlers: " .. table.concat(bad, ", "))
	end)

	it("does not bind the same remote twice in different services", function()
		local owner = {}
		local duplicates = {}
		for _, file in ipairs(files.server) do
			for name in string.gmatch(file.src, "remotes%.On%(\"([%w_]+)\"") do
				if owner[name] and owner[name] ~= file.name then
					table.insert(duplicates, name .. " (" .. owner[name] .. ", " .. file.name .. ")")
				end
				owner[name] = file.name
			end
		end
		eq(#duplicates, 0, "remote bound twice: " .. table.concat(duplicates, ", "))
	end)
end)

describe("Sources: size", function()
	it("the codebase is substantial", function()
		local lines = 0
		for _, file in ipairs(allFiles()) do
			lines += countLines(file.src)
		end
		expect(lines > 6000, "expected a real implementation, saw " .. lines .. " lines")
		print(string.format("        (source: %d files, %d lines)", #allFiles(), lines))
	end)

	it("no file is a stub shorter than 20 lines", function()
		local stubs = {}
		for _, file in ipairs(allFiles()) do
			if countLines(file.src) < 20 then
				table.insert(stubs, file.name)
			end
		end
		eq(#stubs, 0, "stub files: " .. table.concat(stubs, ", "))
	end)
end)
