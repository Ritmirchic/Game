--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    6-Stage Automated QA, Simulation, Security & Stress Test Suite
    Executes directly inside the official Luau VM CLI (/usr/local/bin/luau)
    against the real production Shared and ServerScriptService modules!
]]

-- ============================================================================
-- MINIMAL ROBLOX ENGINE RUNTIME MOCK FOR STANDALONE LUAU CLI EXECUTION
-- ============================================================================
-- Set by the WebAssembly Luau runner; the native `luau` CLI leaves it unset.
gEnv = _G :: any
gEnv.AshesVmHost = if gEnv.ASHES_VM_HOST ~= nil then tostring(gEnv.ASHES_VM_HOST) else "native-luau-cli"

local clockNow = 100.0

local function makeVec3(x: number, y: number, z: number): any
    local v: any = {}
    v.X = x
    v.Y = y
    v.Z = z
    v.Magnitude = math.sqrt(x * x + y * y + z * z)
    local mt: any = {}
    function mt.__index(_t: any, k: string)
        if k == "Unit" then
            local m = v.Magnitude
            if m < 1e-6 then
                return makeVec3(0, 0, 0)
            end
            return makeVec3(x / m, y / m, z / m)
        end
        return rawget(v, k)
    end
    function mt.__add(a: any, b: any)
        return makeVec3(a.X + b.X, a.Y + b.Y, a.Z + b.Z)
    end
    function mt.__sub(a: any, b: any)
        return makeVec3(a.X - b.X, a.Y - b.Y, a.Z - b.Z)
    end
    function mt.__mul(a: any, b: any)
        if type(a) == "number" then
            return makeVec3(a * b.X, a * b.Y, a * b.Z)
        elseif type(b) == "number" then
            return makeVec3(a.X * b, a.Y * b, a.Z * b)
        end
        return makeVec3(a.X * b.X, a.Y * b.Y, a.Z * b.Z)
    end
    function mt.__div(a: any, b: number)
        return makeVec3(a.X / b, a.Y / b, a.Z / b)
    end
    function mt.__unm(a: any)
        return makeVec3(-a.X, -a.Y, -a.Z)
    end
    function mt.__eq(a: any, b: any)
        return math.abs(a.X - b.X) < 1e-5 and math.abs(a.Y - b.Y) < 1e-5 and math.abs(a.Z - b.Z) < 1e-5
    end
    function v:Dot(other: any): number
        return x * other.X + y * other.Y + z * other.Z
    end
    function v:Cross(other: any): any
        return makeVec3(y * other.Z - z * other.Y, z * other.X - x * other.Z, x * other.Y - y * other.X)
    end
    return setmetatable(v, mt)
end

local Vector3Mock: any = {
    new = function(x: number?, y: number?, z: number?)
        return makeVec3(x or 0, y or 0, z or 0)
    end,
    zero = makeVec3(0, 0, 0),
    xAxis = makeVec3(1, 0, 0),
    yAxis = makeVec3(0, 1, 0),
    zAxis = makeVec3(0, 0, 1),
}

-- CFrame mock with a real 3x3 rotation (right / up / back columns), so angle
-- composition (`CFrame.lookAt(...) * CFrame.Angles(...)`, used by weapon spread)
-- behaves like the engine instead of discarding the base orientation.
local function frameFromAxes(pos: any, rx: any, ry2: any, rz: any): any
    local cf: any = { Position = pos }
    cf._axes = { rx, ry2, rz }
    cf.RightVector = rx
    cf.UpVector = ry2
    cf.LookVector = -rz
    cf.X = pos.X
    cf.Y = pos.Y
    cf.Z = pos.Z
    local mt: any = {}
    function mt.__index(_t: any, k: string)
        if k == "Rotation" then
            return frameFromAxes(makeVec3(0, 0, 0), rx, ry2, rz)
        end
        return rawget(cf, k)
    end
    function mt.__mul(a: any, b: any)
        if b._axes then
            local function mapAxis(v: any): any
                return a._axes[1] * v.X + a._axes[2] * v.Y + a._axes[3] * v.Z
            end
            local nx = mapAxis(b._axes[1])
            local ny = mapAxis(b._axes[2])
            local nz = mapAxis(b._axes[3])
            local npos = a.Position + mapAxis(b.Position)
            return frameFromAxes(npos, nx, ny, nz)
        end
        -- CFrame * Vector3 (point transform)
        local v = b
        return a.Position + a._axes[1] * v.X + a._axes[2] * v.Y + a._axes[3] * v.Z
    end
    return setmetatable(cf, mt)
end

local function makeCFrame(pos: any, look: any?): any
    local lv = if look and look.Magnitude > 1e-5 then look.Unit else makeVec3(0, 0, -1)
    local upHint = if math.abs(lv.Z) > 0.98 then makeVec3(0, 0, 1) else makeVec3(0, 1, 0)
    local rt = lv:Cross(upHint)
    if rt.Magnitude < 1e-6 then
        rt = makeVec3(1, 0, 0)
    end
    rt = rt.Unit
    local up = rt:Cross(lv)
    return frameFromAxes(pos, rt, up, -lv)
end

local CFrameMock: any = {
    new = function(a: any, b: any?, c: any?)
        if type(a) == "table" and a.X ~= nil then
            return makeCFrame(a, makeVec3(0, 0, -1))
        end
        return makeCFrame(makeVec3(a or 0, b or 0, c or 0), makeVec3(0, 0, -1))
    end,
    identity = nil :: any,
    lookAt = function(at: any, target: any)
        return makeCFrame(at, (target - at).Unit)
    end,
    Angles = function(rx: number, ry: number, rz: number)
        -- Roblox order: R = Ry(ry) * Rx(rx) * Rz(rz)
        local cx, sx = math.cos(rx), math.sin(rx)
        local cy, sy = math.cos(ry), math.sin(ry)
        local cz, sz = math.cos(rz), math.sin(rz)
        local m00 = cy * cz + sy * sx * sz
        local m01 = -cy * sz + sy * sx * cz
        local m02 = sy * cx
        local m10 = cx * sz
        local m11 = cx * cz
        local m12 = -sx
        local m20 = -sy * cz + cy * sx * sz
        local m21 = sy * sz + cy * sx * cz
        local m22 = cy * cx
        return frameFromAxes(
            makeVec3(0, 0, 0),
            makeVec3(m00, m10, m20),
            makeVec3(m01, m11, m21),
            makeVec3(m02, m12, m22)
        )
    end,
    fromEulerAnglesYXZ = function(rx: number, ry: number, rz: number)
        return CFrameMock.Angles(rx, ry, rz)
    end,
}
CFrameMock.identity = CFrameMock.new(0, 0, 0)

local Color3Mock: any = {
    fromRGB = function(r: number, g: number, b: number)
        return { R = r / 255, G = g / 255, B = b / 255 }
    end,
    new = function(r: number, g: number, b: number)
        return { R = r, G = g, B = b }
    end,
}

local function createSignal()
    local callbacks: { (any) -> () } = {}
    return {
        Connect = function(_self: any, fn: (any) -> ())
            table.insert(callbacks, fn)
            return { Disconnect = function() end }
        end,
        Fire = function(_self: any, ...: any)
            for _, fn in ipairs(callbacks) do
                fn(...)
            end
        end,
    }
end

-- Signal-shaped properties (part.Touched, humanoid.Died, remote.OnServerEvent, ...)
-- are handed out lazily so production code that subscribes to them keeps working.
local SIGNAL_NAMES: { [string]: boolean } = {
    Touched = true,
    TouchEnded = true,
    Died = true,
    Destroying = true,
    Changed = true,
    ChildAdded = true,
    ChildRemoved = true,
    DescendantAdded = true,
    DescendantRemoving = true,
    AncestryChanged = true,
    Equipped = true,
    Unequipped = true,
    Activated = true,
    Deactivated = true,
    MouseClick = true,
    MouseEnter = true,
    MouseLeave = true,
    Finished = true,
    LoopStarted = true,
    LoopEnded = true,
    Stepped = true,
    Heartbeat = true,
    RenderStepped = true,
    UnbufferedRenderStepped = true,
    Triggered = true,
    TriggeredChanged = true,
    Button1Down = true,
    Button1Up = true,
    MouseButton1Click = true,
    MouseButton1Down = true,
    MouseButton1Up = true,
    MouseButton2Down = true,
    InputBegan = true,
    InputChanged = true,
    InputEnded = true,
    StateChanged = true,
    GetPropertyChangedSignal = nil,
}

local function makeInstance(className: string, name: string?): any
    local attrs: { [string]: any } = {}
    local signals: { [string]: any } = {}
    local children: { any } = {}
    local parentRef: any = nil
    local posVal: any = makeVec3(0, 0, 0)
    local cfVal: any = makeCFrame(makeVec3(0, 0, 0), makeVec3(0, 0, -1))
    local inst: any = nil
    inst = {
        ClassName = className,
        Name = name or className,
        Size = makeVec3(2, 2, 2),
        AssemblyLinearVelocity = makeVec3(0, 0, 0),
        Transparency = 0,
        CanCollide = true,
        Anchored = true,
        Material = { Name = "Concrete" },
        WalkSpeed = 12.5,
        JumpHeight = 6.2,
        MaxHealth = 100,
        Health = 100,
        ClockTime = 7.5,
        PrimaryPart = nil,
        SetAttribute = function(_s: any, k: string, v: any)
            attrs[k] = v
        end,
        GetAttribute = function(_s: any, k: string)
            return attrs[k]
        end,
        GetChildren = function(_s: any)
            return table.clone(children)
        end,
        GetDescendants = function(s: any)
            local out = {}
            local function walk(n: any)
                for _, c in ipairs(n._children) do
                    table.insert(out, c)
                    walk(c)
                end
            end
            walk(s)
            return out
        end,
        FindFirstChild = function(_s: any, cname: string)
            for _, c in ipairs(children) do
                if c.Name == cname then
                    return c
                end
            end
            return nil
        end,
        FindFirstChildOfClass = function(_s: any, cls: string)
            for _, c in ipairs(children) do
                if c.ClassName == cls then
                    return c
                end
            end
            return nil
        end,
        FindFirstChildWhichIsA = function(_s: any, cls: string)
            for _, c in ipairs(children) do
                if c.ClassName == cls or (cls == "BasePart" and (c.ClassName == "Part" or c.ClassName == "SpawnLocation")) then
                    return c
                end
            end
            return nil
        end,
        WaitForChild = function(s: any, cname: string)
            return s:FindFirstChild(cname)
        end,
        IsA = function(s: any, cls: string)
            if s.ClassName == cls then
                return true
            end
            if cls == "BasePart" and (s.ClassName == "Part" or s.ClassName == "TrussPart" or s.ClassName == "SpawnLocation") then
                return true
            end
            if cls == "Instance" then
                return true
            end
            return false
        end,
        PivotTo = function(s: any, cf: any)
            local oldPos = if s.PrimaryPart then s.PrimaryPart.Position else s.Position
            local delta = cf.Position - oldPos
            s.CFrame = cf
            for _, ch in ipairs(children) do
                if ch:IsA("BasePart") then
                    ch.Position = ch.Position + delta
                end
            end
        end,
        GetPivot = function(s: any)
            if s.PrimaryPart then
                return s.PrimaryPart.CFrame
            end
            return s.CFrame
        end,
        Clone = function(s: any)
            local copy = makeInstance(s.ClassName, s.Name)
            copy.Size = s.Size
            copy.Position = s.Position
            copy.CFrame = s.CFrame
            for k, v in pairs(attrs) do
                copy:SetAttribute(k, v)
            end
            for _, ch in ipairs(children) do
                local chCopy = ch:Clone()
                chCopy.Parent = copy
                if s.PrimaryPart == ch then
                    copy.PrimaryPart = chCopy
                end
            end
            return copy
        end,
        Destroy = function(s: any)
            if s.Parent and s.Parent._children then
                local idx = table.find(s.Parent._children, s)
                if idx then
                    table.remove(s.Parent._children, idx)
                end
            end
            s.Parent = nil
        end,
        _children = children,
    }

    local mt: any = {}
    function mt.__index(_t: any, k: string)
        if k == "Parent" then
            return parentRef
        elseif k == "Position" then
            return posVal
        elseif k == "CFrame" then
            return cfVal
        end
        if SIGNAL_NAMES[k] or (type(k) == "string" and string.sub(k, 1, 2) == "On") then
            local sig = signals[k]
            if not sig then
                sig = createSignal()
                signals[k] = sig
            end
            return sig
        end
        for _, c in ipairs(children) do
            if c.Name == k then
                return c
            end
        end
        return nil
    end
    function mt.__newindex(t: any, k: string, v: any)
        if k == "Parent" then
            if parentRef and parentRef._children then
                local idx = table.find(parentRef._children, t)
                if idx then
                    table.remove(parentRef._children, idx)
                end
            end
            parentRef = v
            if v and v._children then
                table.insert(v._children, t)
            end
        elseif k == "Position" then
            posVal = v
            cfVal = makeCFrame(v, cfVal.LookVector)
        elseif k == "CFrame" then
            cfVal = v
            posVal = v.Position
        else
            rawset(t, k, v)
        end
    end
    return setmetatable(inst, mt)
end

-- Ray-AABB intersection for real 3D line-of-sight, shelter roof, & gunshot raycast testing!
local WorkspaceInst = makeInstance("Workspace", "Workspace")
function WorkspaceInst:Raycast(origin: any, direction: any, params: any): any
    local dirLen = direction.Magnitude
    if dirLen < 1e-5 then
        return nil
    end
    local unit = direction.Unit
    local exclude = if params and params.FilterDescendantsInstances then params.FilterDescendantsInstances else {}
    local function isExcluded(obj: any): boolean
        for _, ex in ipairs(exclude) do
            local cur = obj
            while cur do
                if cur == ex then
                    return true
                end
                cur = cur.Parent
            end
        end
        return false
    end

    local bestT = dirLen + 1
    local bestPart: any = nil

    for _, desc in ipairs(self:GetDescendants()) do
        if desc:IsA("BasePart") and desc.CanCollide and not isExcluded(desc) then
            local half = desc.Size * 0.5
            local minB = desc.Position - half
            local maxB = desc.Position + half
            local tmin = 0.0
            local tmax = dirLen

            local hitAxis = true
            for _, axis in ipairs({ "X", "Y", "Z" }) do
                local o = (origin :: any)[axis]
                local d = (unit :: any)[axis]
                local bmin = (minB :: any)[axis]
                local bmax = (maxB :: any)[axis]
                if math.abs(d) < 1e-6 then
                    if o < bmin or o > bmax then
                        hitAxis = false
                        break
                    end
                else
                    local inv = 1.0 / d
                    local t1 = (bmin - o) * inv
                    local t2 = (bmax - o) * inv
                    if t1 > t2 then
                        t1, t2 = t2, t1
                    end
                    tmin = math.max(tmin, t1)
                    tmax = math.min(tmax, t2)
                    if tmin > tmax then
                        hitAxis = false
                        break
                    end
                end
            end

            if hitAxis and tmin >= 0 and tmin <= dirLen and tmin < bestT then
                bestT = tmin
                bestPart = desc
            end
        end
    end

    if bestPart then
        return {
            Instance = bestPart,
            Position = origin + unit * bestT,
            Distance = bestT,
        }
    end
    return nil
end

local PlayersInst = makeInstance("Players", "Players")
PlayersInst.PlayerRemoving = createSignal()
PlayersInst.PlayerAdded = createSignal()
local mockPlayersList: { any } = {}
function PlayersInst:GetPlayers()
    return mockPlayersList
end
function PlayersInst:GetPlayerByUserId(uid: number)
    for _, p in ipairs(mockPlayersList) do
        if p.UserId == uid then
            return p
        end
    end
    return nil
end

local LightingInst = makeInstance("Lighting", "Lighting")
local RepStorageInst = makeInstance("ReplicatedStorage", "ReplicatedStorage")
local ServerStorageInst = makeInstance("ServerStorage", "ServerStorage")
local ServerScriptInst = makeInstance("ServerScriptService", "ServerScriptService")

local memoryDataStore: { [string]: any } = {}
local DataStoreServiceMock = {
    GetDataStore = function(_s: any, storeName: string)
        return {
            UpdateAsync = function(_ds: any, key: string, transformFn: (any) -> any)
                local k = storeName .. "::" .. key
                memoryDataStore[k] = transformFn(memoryDataStore[k])
                return memoryDataStore[k]
            end,
            SetAsync = function(_ds: any, key: string, val: any)
                memoryDataStore[storeName .. "::" .. key] = val
            end,
            GetAsync = function(_ds: any, key: string)
                return memoryDataStore[storeName .. "::" .. key]
            end,
        }
    end,
}

local PathfindingServiceMock = {
    CreatePath = function(_s: any, _cfg: any)
        local goalCache = makeVec3(0, 0, 0)
        return {
            Status = "Success",
            ComputeAsync = function(_p: any, _start: any, goal: any)
                goalCache = goal
            end,
            GetWaypoints = function(_p: any)
                return { { Position = makeVec3(0, 0, 0) }, { Position = goalCache } }
            end,
        }
    end,
}

local RunServiceMock = {
    IsStudio = function()
        return false -- Test REAL DataStoreService UpdateAsync path!
    end,
}

local ServicesMap: { [string]: any } = {
    Workspace = WorkspaceInst,
    Players = PlayersInst,
    Lighting = LightingInst,
    ReplicatedStorage = RepStorageInst,
    ServerStorage = ServerStorageInst,
    ServerScriptService = ServerScriptInst,
    DataStoreService = DataStoreServiceMock,
    PathfindingService = PathfindingServiceMock,
    RunService = RunServiceMock,
}

local gEnv: any = getfenv(0)
gEnv.Vector3 = Vector3Mock
gEnv.CFrame = CFrameMock
gEnv.Color3 = Color3Mock
gEnv.RaycastParams = {
    new = function()
        return { FilterType = "Exclude", FilterDescendantsInstances = {} }
    end,
}
gEnv.NumberRange = {
    new = function(a: number, b: number?)
        return { Min = a, Max = b or a }
    end,
}
gEnv.NumberSequenceKeypoint = {
    new = function(t: number, v: number)
        return { Time = t, Value = v }
    end,
}
gEnv.NumberSequence = {
    new = function(v: any)
        return { Keypoints = v }
    end,
}
gEnv.ColorSequence = {
    new = function(v: any)
        return { Color = v }
    end,
}
gEnv.Random = {
    new = function(_seed: number?)
        return {
            NextNumber = function(_r: any, lo: number?, hi: number?)
                local u = math.random()
                if lo and hi then
                    return lo + u * (hi - lo)
                end
                return u
            end,
            NextInteger = function(_r: any, lo: number, hi: number)
                if hi <= lo then
                    return lo
                end
                return math.random(lo, hi)
            end,
        }
    end,
}
gEnv.Enum = {
    RaycastFilterType = { Exclude = "Exclude" },
    PartType = { Ball = "Ball", Block = "Block", Cylinder = "Cylinder" },
    Material = {
        Concrete = { Name = "Concrete" },
        Slate = { Name = "Slate" },
        Metal = { Name = "Metal" },
        CorrodedMetal = { Name = "CorrodedMetal" },
        WoodPlanks = { Name = "WoodPlanks" },
        Fabric = { Name = "Fabric" },
    },
    PathStatus = { Success = "Success" },
    ActuatorType = { Servo = "Servo" },
}
-- Any Enum.<Group>.<Item> not spelled out above resolves to a stable unique
-- token, so production modules that key tables by (e.g.) Enum.Material.Concrete
-- load exactly like they do on the Roblox runtime.
local enumGroupCache: { [string]: any } = {}
local enumItemCache: { [string]: any } = {}
local function makeEnumGroup(groupName: string): any
    local cached = enumGroupCache[groupName]
    if cached then
        return cached
    end
    local group: any = setmetatable({}, {
        __index = function(_t: any, itemName: string)
            local key = groupName .. "." .. tostring(itemName)
            local item = enumItemCache[key]
            if not item then
                item = { Name = itemName, EnumKey = key }
                enumItemCache[key] = item
            end
            return item
        end,
    })
    enumGroupCache[groupName] = group
    return group
end
local function decorateGroup(group: any, groupName: string): any
    if type(group) ~= "table" then
        return group
    end
    if getmetatable(group) == nil then
        setmetatable(group, {
            __index = function(_t: any, itemName: string)
                local key = groupName .. "." .. tostring(itemName)
                local item = enumItemCache[key]
                if not item then
                    item = { Name = itemName, EnumKey = key }
                    enumItemCache[key] = item
                end
                return item
            end,
        })
    end
    return group
end
for groupName, group in pairs(gEnv.Enum :: any) do
    decorateGroup(group, tostring(groupName))
end
setmetatable(gEnv.Enum :: any, {
    __index = function(_t: any, groupName: string)
        return decorateGroup(makeEnumGroup(tostring(groupName)), tostring(groupName))
    end,
})
gEnv.Instance = {
    new = function(cls: string)
        return makeInstance(cls, cls)
    end,
}
gEnv.typeof = function(v: any): string
    if type(v) == "table" then
        if v.X ~= nil and v.Y ~= nil and v.Z ~= nil then
            return "Vector3"
        end
        if v.ClassName ~= nil then
            return "Instance"
        end
    end
    return type(v)
end
gEnv.task = {
    spawn = function(_fn: any, ...: any)
        -- Do not launch infinite background loops during deterministic step tests
    end,
    defer = function(fn: any, ...: any)
        fn(...)
    end,
    delay = function(_sec: number, _fn: any) end,
    wait = function(_sec: number?)
        return 0.1
    end,
}
gEnv.game = {
    JobId = "QA_JOB_001",
    GetService = function(_g: any, name: string)
        return ServicesMap[name] or makeInstance(name, name)
    end,
    BindToClose = function(_g: any, _fn: any) end,
}

-- Override os.clock for deterministic time-advancement tests while preserving real benchmark timer
local realClock = os.clock
gEnv.os = setmetatable({
    clock = function()
        return clockNow
    end,
    time = os.time,
}, { __index = os })

-- Dynamic module loader mapping ReplicatedStorage.Shared.* and ServerScriptService.Services.* to disk files!
local moduleCache: { [string]: any } = {}
local function loadDiskModule(relPath: string): any
    if moduleCache[relPath] then
        return moduleCache[relPath]
    end
    -- Luau CLI does not expose io/loadfile in sandboxed mode unless standard loadfile is available; check!
    error("Unexpected missing preload for " .. relPath)
end
_ = loadDiskModule
