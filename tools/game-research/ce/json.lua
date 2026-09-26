-- Small deterministic JSON encoder. Use array({}) for an empty JSON array.
local M = {}
local array_tag = {}
function M.array(items) return setmetatable(items or {}, array_tag) end
local function quote(value)
  return '"' .. value:gsub('[%z\1-\31\\"]', function(c)
    local escapes = {['"'] = '\\"', ['\\'] = '\\\\', ['\n'] = '\\n',
                     ['\r'] = '\\r', ['\t'] = '\\t'}
    return escapes[c] or string.format('\\u%04x', c:byte())
  end) .. '"'
end
function M.encode(value)
  local kind = type(value)
  if kind == 'nil' then return 'null' end
  if kind == 'boolean' then return tostring(value) end
  if kind == 'string' then return quote(value) end
  if kind == 'number' then
    assert(value == value and value ~= math.huge and value ~= -math.huge, 'nonfinite JSON number')
    return tostring(value)
  end
  assert(kind == 'table', 'unsupported JSON value')
  local parts = {}
  if getmetatable(value) == array_tag or #value > 0 then
    for i = 1, #value do parts[i] = M.encode(value[i]) end
    return '[' .. table.concat(parts, ',') .. ']'
  end
  local keys = {}
  for key in pairs(value) do
    assert(type(key) == 'string', 'JSON object key must be a string')
    keys[#keys + 1] = key
  end
  table.sort(keys)
  for _, key in ipairs(keys) do parts[#parts + 1] = quote(key) .. ':' .. M.encode(value[key]) end
  return '{' .. table.concat(parts, ',') .. '}'
end
return M
