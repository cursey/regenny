# ReGenny Agent Guide

ReGenny is a reverse engineering GUI tool for interactively reconstructing memory structures in live Windows processes. It parses `.genny` files (C-like struct definitions), attaches to a target process, and renders live memory as typed structures.

Full scripting reference: https://praydog.github.io/regenny-book/

## Quick Start Workflow

1. **Check status**: `regenny_status` -- see if a file is open and a process is attached
2. **List processes**: `regenny_list_processes` -- find the target process
3. **Attach**: `regenny_attach` with PID or process name
4. **Open/create a .genny file**: `regenny_open_file` or `regenny_new_file`
5. **List types**: `regenny_list_types` -- see what structs are defined
6. **Select a type**: `regenny_select_type` with a name and address to view it in the UI
7. **Read memory**: `regenny_read_memory` or `regenny_read_typed` for raw access
8. **Modify definitions**: `regenny_set_file` to update the .genny content (triggers re-parse)
9. **Use Lua**: `regenny_lua_eval` for complex operations using the full scripting API

## Address Syntax

The address field (and `regenny_select_type` address parameter) supports:

- **Absolute hex**: `0x7FF6A000`
- **Module-relative**: `game.exe+0x5000` (partial module name OK, e.g. `client.dll+0x1234`)
- **Pointer dereference chains**: `game.exe+0x5000->0x24->0x8` (follows pointers at each `->` offset)

## The .genny File Format

`.genny` files define types using a C-like syntax. They are parsed by ReGenny and used to overlay memory.

### Primitive Types

Primitives must be declared with a name, byte size, and optional metadata tag:

```
type bool 1 [[bool]]
type byte 1 [[u8]]
type ubyte 1 [[u8]]
type char 1 [[utf8]]
type wchar_t 2 [[utf16]]
type short 2 [[i16]]
type ushort 2 [[u16]]
type int 4 [[i32]]
type uint 4 [[u32]]
type float 4 [[f32]]
type double 8 [[f64]]
type int64_t 8 [[i64]]
type uint64_t 8 [[u64]]
type uintptr_t 8 [[u64]]
```

Metadata tags control how values are displayed: `[[i32]]` = signed 32-bit int, `[[f32]]` = float, `[[utf8*]]` = null-terminated string pointer, etc. Metadata can also be applied per-variable inline: `char* name [[utf8*]]`.

### Structs

```
struct MyStruct {
    int field_a
    float field_b
    bool flag
}
```

`class` is also supported (identical semantics to `struct` in .genny).

### Explicit Offsets

Use `+N` to place a field at a specific byte offset:

```
struct Player {
    int health +0x10
    int max_health
    float position_x +0x40
    float position_y
    float position_z
}
```

Fields without `+N` are placed sequentially after the previous field.

### Explicit Struct Size

```
struct PaddedStruct 0x100 {
    int value
}
```

This declares the struct as 0x100 bytes total, regardless of field layout.

### Pointers

```
struct Node {
    int value
    Node* next
    char* name [[utf8*]]
}
```

### Arrays

```
struct Container {
    int values[10]
    float matrix[4][4]
    Item* items
}
```

### Enums

```
enum Direction {
    NORTH = 0,
    SOUTH = 1,
    EAST = 2,
    WEST = 3,
}

enum Color : byte {
    RED = 1,
    GREEN = 2,
    BLUE = 3
}
```

### Bitfields

```
struct Flags {
    uint enabled : 1
    uint mode : 3
    uint level : 4
}
```

### Inheritance

```
struct Base {
    int base_field
}

struct Derived : Base {
    int derived_field
}

struct MultiDerived : Base1, Base2 {
    int own_field
}
```

### Imports

```
import "types.genny"
import "player.genny"
```

### Namespaces

```
namespace app {
    struct Player {
        int health
    }
}
```

Types in namespaces are referenced with dot notation: `app.Player`. Namespaces can be nested. Structs, enums, and classes can be nested within other structs/classes.

## Lua Scripting API

ReGenny embeds Lua 5.4 with full access to the type system and process memory.

### Key Globals

- `regenny` -- the ReGenny app instance
  - `regenny:type()` -- current selected type (sdkgenny.Type; use `:as_struct()` to get Struct methods)
  - `regenny:address()` -- current address (uintptr_t)
  - `regenny:overlay()` -- StructOverlay for current type at current address
  - `regenny:sdk()` -- the parsed SDK (Sdk object)
  - `regenny:process()` -- the attached Process

- `sdkgenny` -- type system library
  - `sdkgenny.StructOverlay(address, struct)` -- create an overlay
  - `sdkgenny.PointerOverlay(address, pointer)` -- create a pointer overlay
  - `sdkgenny.parse(code)` -- parse .genny source code string, returns Sdk
  - `sdkgenny.parse_file(path)` -- parse a .genny file, returns Sdk

### Process Memory Access

```lua
local proc = regenny:process()
local val = proc:read_uint32(0x7FF6A000)
proc:write_uint32(0x7FF6A000, 42)

-- All read/write methods:
-- proc:read_uint8/16/32/64(addr)     proc:write_uint8/16/32/64(addr, val)
-- proc:read_int8/16/32/64(addr)      proc:write_int8/16/32/64(addr, val)
-- proc:read_float(addr)              proc:write_float(addr, val)
-- proc:read_double(addr)             proc:write_double(addr, val)
-- proc:read_string(addr, do_strlen)  -- do_strlen=true for null-terminated

-- Module lookup
local mod = proc:get_module("client.dll")  -- by name
print(mod.name, mod.start, mod.end, mod.size)

local mod2 = proc:get_module_within(some_addr)  -- find module containing address

for _, mod in ipairs(proc:modules()) do
    print(mod.name, string.format("0x%X", mod.start), mod.size)
end

-- Memory regions
for _, alloc in ipairs(proc:allocations()) do
    -- alloc.start, alloc.end, alloc.size, alloc.read, alloc.write, alloc.execute
end

-- Memory management
proc:protect(addr, size, flags)         -- VirtualProtectEx; returns old flags
proc:allocate(addr, size, flags)        -- VirtualAllocEx; addr=0 lets OS choose
```

### WindowsProcess Extensions

When attached to a Windows process, the process object has additional methods:

```lua
local proc = regenny:process()  -- actually a WindowsProcess on Windows

-- RTTI
local name = proc:get_typename(ptr_addr)               -- RTTI class name from object pointer
local name = proc:get_typename_from_vtable(vtable_addr) -- RTTI class name from vtable pointer

-- Code injection
proc:allocate_rwx(addr, size)       -- allocate PAGE_EXECUTE_READWRITE
proc:protect_rwx(addr, size)        -- change to PAGE_EXECUTE_READWRITE
proc:create_remote_thread(addr, param)  -- run thread in target process
```

### Struct Overlays (typed memory access)

```lua
local overlay = regenny:overlay()

-- Access fields by name -- reads from process memory automatically
print(overlay.health)
print(overlay.position_x)

-- Write back
overlay.health = 100

-- Pointer traversal
local next_node = overlay.next:d()  -- dereference pointer (:deref() and :dereference() are aliases)
print(next_node.value)
print(overlay.next:ptr())           -- raw destination address (uintptr_t)
print(overlay.next:address())       -- address of the pointer variable itself

-- Array indexing (treats struct as inline array: addr + N * struct_size)
local second = overlay[1]

-- Overlay introspection
print(overlay:address())   -- base memory address
print(overlay:type():name()) -- the sdkgenny.Struct name

-- Writing raw pointer values
overlay.some_ptr = other_overlay:address()
```

### Type System

```lua
local sdk = regenny:sdk()
local ns = sdk:global_ns()

-- Create types programmatically
local my_struct = ns:struct("NewStruct")
my_struct:size(0x50)
local hp_var = my_struct:variable("health")
hp_var:type("int")
hp_var:offset(0x10)

-- Type introspection (Object base methods, available on all types)
local s = regenny:type():as_struct()  -- cast Type to Struct
print(s:name())
print(s:size())
for _, var in ipairs(s:get_all_variable()) do
    print(var:name(), var:offset(), var:type():name())
end

-- Parent traversal
for _, parent in ipairs(s:parents()) do
    print("inherits:", parent:name())
end

-- Type checking and casting
if obj:is_struct() then
    local s = obj:as_struct()
end
-- Also: is_enum(), as_enum(), is_pointer(), as_pointer(), etc.
-- find_struct(name), find_variable(name), find_in_parents("variable", name)

-- Variable details
local v = s:get_all_variable()[1]
print(v:offset(), v:end())  -- byte range
print(v:is_bitfield(), v:bit_offset(), v:bit_size())
v:append()      -- auto-place after previous variable
v:bit_append()  -- auto-place as next bitfield in same storage unit

-- Generate C++ headers
sdk:generate("output/path/")
```

## Common Patterns

### Exploring Unknown Memory

```lua
-- Read a block and look for patterns
local proc = regenny:process()
local addr = 0x7FF6A000
for i = 0, 0x100, 4 do
    local val = proc:read_uint32(addr + i)
    if val and val ~= 0 then
        print(string.format("+0x%X: %d (0x%X)", i, val, val))
    end
end
```

### Finding Pointers and Strings

```lua
local proc = regenny:process()
local addr = 0x7FF6A000
for i = 0, 0x100, 8 do
    local ptr = proc:read_uint64(addr + i)
    if ptr and ptr > 0x10000000000 and ptr < 0x7FFFFFFFFFFF then
        local str = proc:read_string(ptr, true)
        if #str > 0 and #str < 100 then
            print(string.format("+0x%X: 0x%X -> \"%s\"", i, ptr, str))
        else
            print(string.format("+0x%X: 0x%X (ptr)", i, ptr))
        end
    end
end
```

### RTTI Discovery (Windows)

Use `regenny_rtti_typename` to identify C++ class types at runtime via their vtable pointers. In Lua: `proc:get_typename(object_addr)` or `proc:get_typename_from_vtable(vtable_addr)`.

### Iterative Type Reconstruction

1. Start with a minimal struct: `struct Unknown 0x100 { int field_0 }`
2. Use `regenny_read_memory` to see raw bytes at the address
3. Identify patterns (floats at +0x10, pointer at +0x20, etc.)
4. Update the .genny file with `regenny_set_file`
5. Use `regenny_select_type` to view the updated layout live

### Traversing Pointer Arrays

```lua
local proc = regenny:process()
local overlay = regenny:overlay()

-- If a struct has a pointer to an array of structs:
local array_ptr = proc:read_uint64(overlay:address() + 0x48)
local count = proc:read_uint32(overlay:address() + 0x40)
for i = 0, count - 1 do
    local entry_ptr = proc:read_uint64(array_ptr + i * 8)  -- array of pointers
    local name_ptr = proc:read_uint64(entry_ptr)             -- first field is a char*
    local name = proc:read_string(name_ptr, true)
    print(string.format("[%d] %s", i, name))
end
```

## Gotchas

- Addresses must be valid in the target process. Reading invalid addresses returns null/errors.
- `.genny` files auto-reload when modified on disk (1-second poll). `regenny_set_file` also triggers immediate re-parse.
- The Lua state persists between eval calls. Variables set in one call are available in the next.
- `regenny_lua_eval` tries to evaluate as an expression first (prefixes `return`), then falls back to statement execution.
- Process attachment, file opening, and type selection are deferred to the main UI thread. They execute on the next frame after the API call returns.
- `regenny:type()` returns a base `Type`. To access struct-specific methods (size, parents, variables), cast with `:as_struct()`.
- Lua 5.4 has strict integer/float distinction. Large `u64` values may overflow Lua integers. Use `string.format("0x%X", val)` for display.
- `proc:read_*` methods return `nil` on failure (invalid address, unreadable memory). Always nil-check in loops.
