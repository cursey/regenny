using System.ComponentModel;
using System.Text.Json;
using ModelContextProtocol.Server;

namespace ReGennyMcp;

// ── Status & Connection ──────────────────────────────────────────────

[McpServerToolType]
public static class StatusTools
{
    [McpServerTool(Name = "regenny_status")]
    [Description("Get ReGenny app state: attached process name/PID, current type, current address, whether an SDK is loaded")]
    public static async Task<string> Status()
        => await Http.Get("/api/status");

    [McpServerTool(Name = "regenny_list_processes")]
    [Description("List available processes to attach to. Returns array of {pid, name}.")]
    public static async Task<string> ListProcesses()
        => await Http.Get("/api/processes");

    [McpServerTool(Name = "regenny_attach")]
    [Description("Attach to a target process by PID or name. Deferred to main thread.")]
    public static async Task<string> Attach(
        [Description("Process ID (integer)")] int? pid = null,
        [Description("Process name (e.g. 'game.exe')")] string? name = null)
        => await Http.Post("/api/attach", new { pid, name });

    [McpServerTool(Name = "regenny_detach")]
    [Description("Detach from the current process")]
    public static async Task<string> Detach()
        => await Http.Post("/api/detach", new { });

    [McpServerTool(Name = "regenny_help")]
    [Description("Get the agent navigation guide for ReGenny. Covers: .genny file format, Lua API, typical workflows, type system. **Call this FIRST in a new session.**")]
    public static async Task<string> Help()
    {
        try { return await Http.Get("/api/help"); }
        catch { /* ReGenny not running — fall through to local file */ }

        var agentMdPath = ResolveAgentMd();
        if (agentMdPath is not null && File.Exists(agentMdPath))
            return await File.ReadAllTextAsync(agentMdPath);

        return """{"error": "AGENT.md not found. Ensure ReGenny is running, or the MCP server is run from the regenny repo root."}""";
    }

    static string? ResolveAgentMd()
    {
        var dir = Path.GetDirectoryName(typeof(StatusTools).Assembly.Location);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir, "AGENT.md");
            if (File.Exists(candidate)) return candidate;
            dir = Path.GetDirectoryName(dir);
        }
        return null;
    }
}

// ── Memory ───────────────────────────────────────────────────────────

[McpServerToolType]
public static class MemoryTools
{
    [McpServerTool(Name = "regenny_read_memory")]
    [Description("Read N bytes at address as hex dump with ASCII sidebar. Requires attached process.")]
    public static async Task<string> ReadMemory(
        [Description("Memory address (hex e.g. '0x7FF6A000' or decimal)")] string address,
        [Description("Number of bytes to read (max 8192)")] int size = 256)
        => await Http.Get("/api/memory/read", new() { ["address"] = address, ["size"] = size.ToString() });

    [McpServerTool(Name = "regenny_read_typed")]
    [Description("Read typed values at address. Types: u8,i8,u16,i16,u32,i32,u64,i64,f32,f64,ptr. Use count>1 to read sequential values.")]
    public static async Task<string> ReadTyped(
        [Description("Memory address (hex or decimal)")] string address,
        [Description("Value type: u8,i8,u16,i16,u32,i32,u64,i64,f32,f64,ptr")] string type,
        [Description("Number of sequential values to read (default 1, max 50)")] int? count = null)
        => await Http.Get("/api/memory/read_typed", new() {
            ["address"] = address, ["type"] = type, ["count"] = count?.ToString()
        });

    [McpServerTool(Name = "regenny_write_memory")]
    [Description("Write a typed value at address. Types: u8,i8,u16,i16,u32,i32,u64,i64,f32,f64")]
    public static async Task<string> WriteMemory(
        [Description("Memory address (hex or decimal)")] string address,
        [Description("Value type: u8,i8,u16,i16,u32,i32,u64,i64,f32,f64")] string type,
        [Description("Value to write (JSON number)")] JsonElement value)
        => await Http.Post("/api/memory/write", new { address, type, value });

    [McpServerTool(Name = "regenny_read_string")]
    [Description("Read a null-terminated string at address")]
    public static async Task<string> ReadString(
        [Description("Memory address (hex or decimal)")] string address,
        [Description("Maximum string length (default 256, max 4096)")] int? max_length = null)
        => await Http.Get("/api/memory/read_string", new() {
            ["address"] = address, ["max_length"] = max_length?.ToString()
        });

    [McpServerTool(Name = "regenny_list_modules")]
    [Description("List loaded modules in the attached process: name, start address, end address, size")]
    public static async Task<string> ListModules()
        => await Http.Get("/api/modules");
}

// ── Genny Files ──────────────────────────────────────────────────────

[McpServerToolType]
public static class GennyTools
{
    [McpServerTool(Name = "regenny_get_file")]
    [Description("Get the current .genny file content and path")]
    public static async Task<string> GetFile()
        => await Http.Get("/api/genny/content");

    [McpServerTool(Name = "regenny_set_file")]
    [Description("Write new content to the current .genny file. Triggers re-parse and UI update. Use this to modify struct definitions.")]
    public static async Task<string> SetFile(
        [Description("New .genny file content (full file text)")] string content)
        => await Http.Post("/api/genny/content", new { content });

    [McpServerTool(Name = "regenny_open_file")]
    [Description("Open a .genny file by path in ReGenny")]
    public static async Task<string> OpenFile(
        [Description("Absolute or relative path to a .genny file")] string path)
        => await Http.Post("/api/genny/open", new { path });

    [McpServerTool(Name = "regenny_new_file")]
    [Description("Create a new .genny file at the given path with initial content, and open it in ReGenny")]
    public static async Task<string> NewFile(
        [Description("Path for the new .genny file")] string path,
        [Description("Initial .genny content (optional — default template provided if empty)")] string? content = null)
        => await Http.Post("/api/genny/new", new { path, content });

    [McpServerTool(Name = "regenny_reload")]
    [Description("Force re-parse the current .genny file (useful after external edits)")]
    public static async Task<string> Reload()
        => await Http.Post("/api/genny/reload", new { });
}

// ── Type Introspection ───────────────────────────────────────────────

[McpServerToolType]
public static class TypeTools
{
    [McpServerTool(Name = "regenny_list_types")]
    [Description("List all struct/class type names from the parsed .genny SDK, with their sizes")]
    public static async Task<string> ListTypes()
        => await Http.Get("/api/types");

    [McpServerTool(Name = "regenny_get_type")]
    [Description("Get full struct layout: fields with offsets, sizes, types, parent classes, metadata. Use dot-separated names for namespaced types.")]
    public static async Task<string> GetType(
        [Description("Type name (e.g. 'MyStruct' or 'Namespace.MyStruct')")] string name)
        => await Http.Get("/api/type", new() { ["name"] = name });

    [McpServerTool(Name = "regenny_select_type")]
    [Description("Set the active type and address in the ReGenny UI. This controls what is displayed in the Memory View.")]
    public static async Task<string> SelectType(
        [Description("Type name to display")] string name,
        [Description("Memory address to view the type at (hex or decimal)")] string? address = null)
        => await Http.Post("/api/type/select", new { name, address });
}

// ── Lua Scripting ────────────────────────────────────────────────────

[McpServerToolType]
public static class LuaTools
{
    [McpServerTool(Name = "regenny_lua_eval")]
    [Description("Execute Lua code in ReGenny's embedded Lua state. Returns stdout output and expression result. Has access to: regenny (app), sdkgenny (type system), process read/write. Expressions are auto-returned; statements execute normally.")]
    public static async Task<string> LuaEval(
        [Description("Lua code to execute")] string code)
        => await Http.Post("/api/lua/eval", new { code });

    [McpServerTool(Name = "regenny_lua_exec_file")]
    [Description("Execute a .lua script file by path in ReGenny's Lua state")]
    public static async Task<string> LuaExecFile(
        [Description("Path to the .lua file")] string path)
        => await Http.Post("/api/lua/exec_file", new { path });

    [McpServerTool(Name = "regenny_lua_write_script")]
    [Description("Write a Lua script file to disk (creates directories as needed)")]
    public static async Task<string> LuaWriteScript(
        [Description("File path for the script")] string path,
        [Description("Lua script content")] string content)
        => await Http.Post("/api/lua/script", new { path, content });

    [McpServerTool(Name = "regenny_lua_read_script")]
    [Description("Read a Lua script file from disk")]
    public static async Task<string> LuaReadScript(
        [Description("Path to the .lua file")] string path)
        => await Http.Get("/api/lua/script", new() { ["path"] = path });
}

// ── Project ──────────────────────────────────────────────────────────

[McpServerToolType]
public static class ProjectTools
{
    [McpServerTool(Name = "regenny_get_project")]
    [Description("Get current project state: process info, type addresses, tabs, chosen type")]
    public static async Task<string> GetProject()
        => await Http.Get("/api/project");
}

// ── RTTI ─────────────────────────────────────────────────────────────

[McpServerToolType]
public static class RttiTools
{
    [McpServerTool(Name = "regenny_rtti_typename")]
    [Description("Get RTTI type name at a pointer address (Windows only). The pointer should point to an object with a vtable.")]
    public static async Task<string> RttiTypename(
        [Description("Object pointer address (hex or decimal)")] string address)
        => await Http.Get("/api/rtti/typename", new() { ["address"] = address });

    [McpServerTool(Name = "regenny_rtti_vtable_typename")]
    [Description("Get RTTI type name from a vtable pointer address (Windows only)")]
    public static async Task<string> RttiVtableTypename(
        [Description("Vtable pointer address (hex or decimal)")] string address)
        => await Http.Get("/api/rtti/vtable_typename", new() { ["address"] = address });
}
