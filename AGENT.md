# ReGenny Agent Guide

ReGenny is a reverse engineering GUI tool for interactively reconstructing memory structures in live Windows processes. It parses `.genny` files (C-like struct definitions), attaches to a target process, and renders live memory as typed structures.

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

Metadata tags control how values are displayed: `[[i32]]` = signed 32-bit int, `[[f32]]` = float, `[[utf8*]]` = null-terminated string pointer, etc.

### Structs

```
struct MyStruct {
    int field_a
    float field_b
    bool flag
}
```

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

Types in namespaces are referenced with dot notation: `app.Player`

## Lua Scripting API

ReGenny embeds Lua 5.4 with full access to the type system and process memory.

### Key Globals

- `regenny` -- the ReGenny app instance
  - `regenny:type()` -- current selected type
  - `regenny:address()` -- current address
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

-- Available: read/write_uint8/16/32/64, read/write_int8/16/32/64
-- read/write_float, read/write_double, read_string(addr, true)

-- Module info
for _, mod in ipairs(proc:modules()) do
    print(mod.name, string.format("0x%X", mod.start), mod.size)
end
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
local next_node = overlay.next:d()  -- dereference pointer
print(next_node.value)

-- Array indexing
local first = overlay.items[0]
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

### RTTI Discovery (Windows)

Use `regenny_rtti_typename` to identify C++ class types at runtime via their vtable pointers.

### Iterative Type Reconstruction

1. Start with a minimal struct: `struct Unknown 0x100 { int field_0 }`
2. Use `regenny_read_memory` to see raw bytes at the address
3. Identify patterns (floats at +0x10, pointer at +0x20, etc.)
4. Update the .genny file with `regenny_set_file`
5. Use `regenny_select_type` to view the updated layout live

## Gotchas

- Addresses must be valid in the target process. Reading invalid addresses returns null/errors.
- `.genny` files auto-reload when modified on disk (1-second poll). `regenny_set_file` also triggers immediate re-parse.
- The Lua state persists between eval calls. Variables set in one call are available in the next.
- `regenny_lua_eval` tries to evaluate as an expression first (prefixes `return`), then falls back to statement execution.
- Process attachment, file opening, and type selection are deferred to the main UI thread. They execute on the next frame after the API call returns.
