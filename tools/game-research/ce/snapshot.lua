-- Read-only baseline collector for Crimson Desert 2.03.02.
-- Historical RVAs are probes, not signatures or compatibility guarantees.
local lines = {'Historical baseline: 2.03.02; compatibility NOT established'}
local function log(text) lines[#lines + 1] = text end
local function safe(fn, ...)
  local ok, value = pcall(fn, ...)
  if ok then return value end
end
local function hex(value)
  return value and string.format('0x%X', value) or 'unreadable'
end
local module = safe(getAddressSafe, 'CrimsonDesert.exe')
if not module or module == 0 then
  return 'CrimsonDesert.exe unavailable: attach CE to the game first.'
end
log('Module: ' .. hex(module))
local current = safe(readPointer, module + 0x6C8CC00)
log('UI slot +6C8CC00 -> ' .. hex(current))
for _, offset in ipairs({0x30, 0x18, 0x88, 0x78, 0, 0x30EB8,
                         0x28, 0xA0, 0x10, 0x48, 0, 0x290, 0x18}) do
  if not current or current == 0 then
    log('Chain stopped: unreadable or null pointer')
    break
  end
  local address = current + offset
  current = safe(readPointer, address)
  log('Read ' .. hex(address) .. ' (+' .. string.format('%X', offset) ..
      ') -> ' .. hex(current))
end
if current and current ~= 0 then
  log('Candidate +BE: ' .. tostring(safe(readBytes, current + 0xBE, 1)))
end
for _, site in ipairs({
  {name = 'UI root writer', rva = 0x86D2AA8},
  {name = 'Menu clear', rva = 0x3D356F0},
  {name = 'Menu set', rva = 0x3D35831}
}) do
  local address = module + site.rva
  local bytes = safe(readBytes, address, 32, true)
  local formatted = {}
  if type(bytes) == 'table' then
    for _, byte in ipairs(bytes) do
      formatted[#formatted + 1] = string.format('%02X', byte)
    end
  end
  log(site.name .. ' historical RVA ' .. hex(site.rva) .. ': ' ..
      (#formatted > 0 and table.concat(formatted, ' ') or 'unreadable'))
  log(tostring(safe(disassemble, address) or 'disassembly unavailable'))
end
return table.concat(lines, '\n')
