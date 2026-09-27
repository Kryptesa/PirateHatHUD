-- Read-only executable-section scan; source definitions are the authority.
local config = GAME_RESEARCH_CONFIG or {}
assert(type(config.checkout) == 'string', 'Set GAME_RESEARCH_CONFIG.checkout to the repository directory')
local json = dofile(config.checkout .. '/tools/game-research/ce/json.lua')
local report = {schema_version = 1, tool = 'check-signatures', status = 'unknown', checks = json.array()}
local function check(id, status, details)
  report.checks[#report.checks + 1] = {id = id, status = status, details = details}
end
local function safe(fn, ...)
  local ok, result = pcall(fn, ...)
  if ok then return result end
end
local function hex(n) return string.format('0x%X', n) end
local function source(path)
  local file = assert(io.open(config.checkout .. '/' .. path, 'rb'), 'Cannot read source: ' .. path)
  local text = file:read('*a'); file:close()
  return text
end
local function number(token)
  token = token:match('^%s*(.-)%s*$'):gsub('[uUlL]+$', '')
  assert(token:match('^0[xX]%x+$') or token:match('^%d+$'), 'Unsupported numeric source token')
  return assert(tonumber(token))
end
local function opcode(text, name)
  local body = assert(text:match(name .. '%s*%[%s*%]%s*=%s*{(.-)}'), 'Missing opcode: ' .. name)
  local bytes = {}
  for token in (body .. ','):gmatch('(.-),') do
    local value = number(token); assert(value >= 0 and value <= 255, 'Invalid opcode byte')
    bytes[#bytes + 1] = value
  end
  assert(#bytes > 0, 'Empty opcode'); return bytes
end
local function main()
  local patterns, scanner = source('include/patterns.hpp'), source('src/pattern_scan.cpp')
  -- Fail on source refactors rather than silently keeping stale copied definitions.
  assert(scanner:find('section.Misc.VirtualSize', 1, true), 'Review scanner section semantics')
  assert(scanner:find('offset + delta', 1, true), 'Review scanner pair semantics')
  local definitions = {
    {id = 'treasure', first = opcode(patterns, 'kEnter'), last = opcode(patterns, 'kLeave'),
     delta = number(assert(patterns:match('kExpectedDelta%s*=%s*([^;]+)')))},
    {id = 'menu', first = opcode(scanner, 'clear'), last = opcode(scanner, 'set'),
     delta = number(assert(scanner:match('delta%s*=%s*menu%s*%?%s*(%w+)')))},
    {id = 'ui-root', first = opcode(scanner, 'context'), last = {}, delta = 0}
  }
  local base = safe(getAddressSafe, 'CrimsonDesert.exe')
  assert(base and base > 0, 'Attach CE to CrimsonDesert.exe')
  local function u16(address) return safe(readSmallInteger, address) end
  local function u32(address) return safe(readInteger, address) end
  assert(u16(base) == 0x5A4D, 'Invalid DOS signature')
  local pe = u32(base + 0x3C)
  assert(pe and pe >= 0 and pe <= 0x1000, 'Invalid PE offset')
  pe = base + pe
  local count, optional_size = u16(pe + 6), u16(pe + 20)
  local image_size = u32(pe + 24 + 56)
  assert(u32(pe) == 0x4550 and u16(pe + 24) == 0x20B and count and count > 0 and
         count <= 96 and image_size and image_size >= 0x1000 and optional_size,
         'Invalid x64 PE headers')
  local sections = {}
  for i = 0, count - 1 do
    local header = pe + 24 + optional_size + i * 40
    local size, rva, flags = u32(header + 8), u32(header + 12), u32(header + 36)
    assert(size and rva and flags, 'Unreadable section header')
    if math.floor(flags / 0x20000000) % 2 == 1 then
      assert(rva >= 0 and rva < image_size and size >= 0, 'Invalid executable section range')
      sections[#sections + 1] = {address = base + rva, size = math.min(size, image_size - rva)}
    end
  end
  assert(#sections > 0, 'No executable sections')
  for _, definition in ipairs(definitions) do
    local matches = json.array()
    for _, section in ipairs(sections) do
      if #matches >= 2 then break end
      local last_start = section.size - math.max(#definition.first, definition.delta + #definition.last)
      -- Chunk reads avoid a full image-sized Lua allocation. Include pair lookahead.
      local chunk_size = 65536
      for start = 0, last_start, chunk_size do
        if #matches >= 2 then break end
        local starts = math.min(chunk_size, last_start - start + 1)
        local length = starts + math.max(#definition.first, definition.delta + #definition.last) - 1
        assert(start + length <= section.size, 'Unsupported first opcode length')
        local bytes = safe(readBytes, section.address + start, length, true)
        assert(type(bytes) == 'table' and #bytes == length, 'Unreadable executable section')
        local function at(offset, expected)
          for j, byte in ipairs(expected) do local masked = definition.id == 'menu' and j >= 3 and j <= 6 or
              definition.id == 'ui-root' and (j >= 5 and j <= 8 or j >= 15 and j <= 18 or j >= 21 and j <= 24)
            if not masked and bytes[offset + j] ~= byte then return false end end
          return true
        end
        for offset = 0, starts - 1 do
          local valid = true
          if definition.id == 'menu' then
            local function displacement(o)
              local n = 0
              for j = 0, 3 do n = n + bytes[o + 3 + j] * 256^j end
              return n
            end
            local d = displacement(offset)
            valid = d > 0 and d <= 0x10000 and d == displacement(offset + definition.delta)
          end
          if valid and at(offset, definition.first) and at(offset + definition.delta, definition.last) then
            local address = section.address + start + offset
            if definition.id == 'ui-root' then
              local d = 0
              for j = 0, 3 do d = d + bytes[offset + 15 + j] * 256^j end
              if d >= 0x80000000 then d = d - 0x100000000 end
              local slot = address + 18 + d
              matches[#matches + 1] = {instruction_rva = hex(address - base), slot_rva = hex(slot - base),
                valid_slot = slot >= base and slot <= base + image_size - 8 and slot % 8 == 0}
            else
            matches[#matches + 1] = {enter_rva = hex(address - base),
                                    leave_rva = hex(address + definition.delta - base)}
            end
            if #matches >= 2 then break end
          end
        end
      end
    end
    check(definition.id, #matches == 1 and matches[1].valid_slot ~= false and 'pass' or 'fail',
          {candidate_pairs = #matches, count_is_lower_bound = #matches >= 2,
           delta = hex(definition.delta), matches = matches})
  end
  report.status = 'pass'
  for _, item in ipairs(report.checks) do if item.status ~= 'pass' then report.status = 'fail' end end
end
local ok, err = pcall(main)
if not ok then
  report.status = 'unknown'
  check('execution', 'unknown', {error = tostring(err):gsub('^.-:%d+:%s*', '')})
end
return json.encode(report)
