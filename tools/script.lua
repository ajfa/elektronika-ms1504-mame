-- Drives a run from an action file named by $MS_SCRIPT; $MS_EVERY > 0 adds a snapshot every N seconds.
-- Each line is "<delay> <action> <argument>"; the delay counts from the previous action.
--   keys <text>                 natural keyboard (post_coded)
--   tap <port>|<field>          press a key for 6 frames
--   hold <port>|<field>|<secs>  keep a key pressed
--   flop1|flop2 <image path>    insert an image (empty path ejects)
--   snap <name>                 save a snapshot
--   wait <text>                 block until <text> (UTF-8, Cyrillic allowed) is in the 80x25 text screen
--   exit                        stop the machine
local every = tonumber(os.getenv('MS_EVERY') or '0')
local scr
for _, s in pairs(manager.machine.screens) do scr = s break end
local mem = manager.machine.devices[':maincpu'].spaces['program']

local function to_cp866(s)
    local out = {}
    for _, cp in utf8.codes(s) do
        if cp < 0x80 then out[#out + 1] = string.char(cp)
        elseif cp >= 0x410 and cp <= 0x43f then out[#out + 1] = string.char(cp - 0x410 + 0x80)
        elseif cp >= 0x440 and cp <= 0x44f then out[#out + 1] = string.char(cp - 0x440 + 0xe0)
        elseif cp == 0x401 then out[#out + 1] = string.char(0xf0)
        elseif cp == 0x451 then out[#out + 1] = string.char(0xf1)
        else out[#out + 1] = '?' end
    end
    return table.concat(out)
end

local function screen_text()
    local base = 0xb8000 + (mem:read_u8(0x44e) | (mem:read_u8(0x44f) << 8))
    local t = {}
    for i = 0, 80 * 25 - 1 do t[#t + 1] = string.char(mem:read_u8(base + i * 2)) end
    return table.concat(t)
end

local actions = {}
local path = os.getenv('MS_SCRIPT')
if path and path ~= '' then
    for line in io.lines(path) do
        local d, kind, arg = line:match('^%s*([%d%.]+)%s+(%S+)%s*(.*)$')
        if d then
            if kind == 'wait' then arg = to_cp866(arg) end
            actions[#actions + 1] = { d = tonumber(d), kind = kind, arg = arg, label = line }
        end
    end
end

local images = {}
for _, img in pairs(manager.machine.images) do images[img.brief_instance_name] = img end

local function field(spec)
    local port, name = spec:match('^(%S+)|(.-)$')
    return manager.machine.ioport.ports[port].fields[name]
end

local nextshot, n, idx, due, releases, frame = every, 0, 1, nil, {}, 0
local function tick()
    local t = manager.machine.time.seconds
    frame = frame + 1
    for i = #releases, 1, -1 do
        local r = releases[i]
        r.frames = r.frames - 1
        if r.frames <= 0 then r.field:set_value(0); table.remove(releases, i)
        elseif r.hold then r.field:set_value(1) end
    end
    while idx <= #actions do
        local a = actions[idx]
        if not due then due = t + a.d end
        if t < due then break end
        if a.kind == 'wait' then
            if frame % 10 ~= 0 or not screen_text():find(a.arg, 1, true) then break end
        elseif a.kind == 'keys' then manager.machine.natkeyboard:post_coded(a.arg)
        elseif a.kind == 'hold' then
            local spec, secs = a.arg:match('^(.*)|([%d%.]+)$')
            local f = field(spec); f:set_value(1)
            releases[#releases + 1] = { field = f, frames = math.floor(tonumber(secs) * 60), hold = true }
        elseif a.kind == 'tap' then
            local f = field(a.arg); f:set_value(1); releases[#releases + 1] = { field = f, frames = 6 }
        elseif a.kind == 'snap' then scr:snapshot(a.arg .. '.png')
        elseif a.kind == 'exit' then manager.machine:exit()
        elseif a.kind == 'dumpfont' then
            local r = manager.machine.memory.regions[':lcdc:lcdc:gfx1']
            local f = io.open(a.arg, 'wb')
            for i = 0, 0xfff do f:write(string.char(r:read_u8(i))) end
            f:close()
        elseif a.kind == 'dump' then
            local f = io.open(a.arg, 'wb')
            for i = 0, 0x3fff do f:write(string.char(mem:read_u8(0xb8000 + i))) end
            f:close()
        elseif images[a.kind] then
            if a.arg == '' then images[a.kind]:unload() else images[a.kind]:load(a.arg) end
        end
        print(string.format('%7.2f s: %s', t, a.label))
        idx = idx + 1
        due = nil
    end
    if every > 0 and t >= nextshot then
        n = n + 1
        scr:snapshot(string.format('f%04d_%05.0fs.png', n, t))
        nextshot = nextshot + every
    end
end

SUB = emu.add_machine_frame_notifier(tick)
