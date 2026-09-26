-- Historical 2.03.02 layout probe. Readability never establishes compatibility.
local config = GAME_RESEARCH_CONFIG or {}
assert(type(config.checkout) == 'string', 'Set GAME_RESEARCH_CONFIG.checkout to the repository directory')
local json = dofile(config.checkout .. '/tools/game-research/ce/json.lua')
local report = {schema_version = 1, tool = 'inspect-ui', status = 'unverified',
                baseline_version = '2.03.02', checks = json.array()}
local function check(id, status, details)
  report.checks[#report.checks + 1] = {id = id, status = status, details = details}
end
local function safe(fn, ...)
  local ok, value = pcall(fn, ...)
  if ok then return value end
end
local function hex(n) return n and string.format('0x%X', n) or 'unreadable' end
local function pointer(base, offset)
  if not base or base <= 0 or base > 0x7FFFFFFFFFFF - offset then return nil end
  return safe(readPointer, base + offset)
end
local function main()
  local module = safe(getAddressSafe, 'CrimsonDesert.exe')
  assert(module and module > 0, 'Attach CE to CrimsonDesert.exe')
  local current = pointer(module, 0x6C8CC00)
  local chain = json.array({{offset = '0x6C8CC00', value = hex(current)}})
  assert(current and current > 0, 'Historical UI slot is unreadable or null')
  for _, offset in ipairs({0x30, 0x18, 0x88, 0x78, 0}) do
    current = pointer(current, offset)
    chain[#chain + 1] = {offset = hex(offset), value = hex(current)}
    if not current or current <= 0 then
      check('ui-owner', 'unknown', {chain = chain, error = 'Unreadable or null link'})
      return
    end
  end
  check('ui-owner', 'unverified', {chain = chain})
  local owner = current
  local array = pointer(owner, 0x30EB8)
  local count = safe(readInteger, owner + 0x30EC0)
  assert(array and array > 0 and count and count > 0 and count <= 4096,
         'Historical UI array/count invalid (expected 1..4096 entries)')
  local function rtti(script)
    local vtable = pointer(script, 0)
    if not vtable or vtable < 8 then return nil, 'Unreadable vtable' end
    local col = pointer(vtable, -8)
    if not col or col <= 0 then return nil, 'Unreadable RTTI locator' end
    local locator = safe(readBytes, col, 24, true)
    if type(locator) ~= 'table' or #locator ~= 24 then return nil, 'Unreadable RTTI locator' end
    local function u32(offset)
      local value = 0
      for i = 3, 0, -1 do value = value * 256 + locator[offset + i + 1] end
      return value
    end
    if u32(0) ~= 1 or col - u32(20) ~= module then return false, 'Other RTTI module/layout' end
    local name_address = module + u32(12) + 16
    local chars = {}
    for i = 0, 255 do
      local byte = safe(readBytes, name_address + i, 1)
      if type(byte) ~= 'number' then return nil, 'Unreadable RTTI name' end
      if byte == 0 then return table.concat(chars) end
      if byte < 32 or byte > 126 then return nil, 'Invalid RTTI name byte' end
      chars[#chars + 1] = string.char(byte)
    end
    return nil, 'RTTI name exceeds 256-byte bound'
  end
  local objects, menu_roots = json.array(), json.array()
  local incomplete = false
  for i = 0, count - 1 do
    local item = {index = i, status = 'empty'}
    local entry = pointer(array, i * 8)
    if entry == nil then item.status = 'unknown'; item.error = 'Unreadable array entry'
    elseif entry ~= 0 then
      local node = pointer(entry, 0xA0)
      local root = node and node ~= 0 and pointer(node, 0x10) or node
      local script = root and root ~= 0 and pointer(root, 0x118) or root
      if node == nil or root == nil or script == nil then
        item.status = 'unknown'; item.error = 'Unreadable object link'
      elseif script ~= 0 then
        item.root = hex(root)
        local name, err = rtti(script)
        if name == nil then item.status = 'unknown'; item.error = err
        elseif name == false then item.status = 'other'; item.reason = err
        else
          item.status = 'unverified'; item.type_name = name
          if name == '.?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@' then
            menu_roots[#menu_roots + 1] = root
            item.field_address = hex(root + 0x25B)
            item.menu_state_byte = safe(readBytes, root + 0x25B, 1)
            if item.menu_state_byte ~= 0 and item.menu_state_byte ~= 1 then
              item.status = 'unknown'; item.error = 'Unreadable or invalid menu state'
            end
          end
        end
      end
    end
    if item.status == 'unknown' then incomplete = true end
    objects[#objects + 1] = item
  end
  check('ui-objects', incomplete and 'unknown' or 'unverified', {count = count, objects = objects})
  check('menu-identity', (incomplete or #menu_roots ~= 1) and 'unknown' or 'unverified',
        {candidate_count = #menu_roots, complete = not incomplete,
         warning = 'Uniqueness is invalid if any potentially relevant entry is unreadable'})
  current = array
  local minimap_chain = json.array()
  for _, offset in ipairs({0x28, 0xA0, 0x10, 0x48, 0, 0x290, 0x18}) do
    current = pointer(current, offset)
    minimap_chain[#minimap_chain + 1] = {offset = hex(offset), value = hex(current)}
    if not current or current <= 0 then
      check('minimap', 'unknown', {chain = minimap_chain, error = 'Unreadable or null link'})
      return
    end
  end
  local value = safe(readBytes, current + 0xBE, 1)
  check('minimap', (value == 0 or value == 1) and 'unverified' or 'unknown',
        {chain = minimap_chain, state_byte = value, field_address = hex(current + 0xBE),
         warning = 'Fixed array slot; verify identity and state transitions in game'})
end
local ok, err = pcall(main)
if not ok then check('execution', 'unknown', {error = tostring(err):gsub('^.-:%d+:%s*', '')}) end
for _, item in ipairs(report.checks) do if item.status == 'unknown' then report.status = 'unknown' end end
return json.encode(report)
