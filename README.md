# Inimini Miny Moe - "Config the thing and let it go!"

**Inimini** is a universal configuration parser for INI and Git-style config files. Header-only, zero dependencies, and built for the pragmatic developer who wants deep nesting without the YAML whitespace drama.

---

## Features

- **Git & INI Styles**: `[section] key = value # comment` OR `[section "topic"] key = value ; comment`
- **Deep Sub-Headings**: `[a.b.c] d.e.f = value` (configurable depth)
- **Parsed Array Values**: Comma-separated lists parsed and trimmed automatically
- **Variable Expansion**: `${VAR}` expanded on read/write
- **Comment Preservation**: Inline and trailing comments preserved with `IMI_COMMENT` flag
- **Cross-Platform Paths**: Windows/macOS/Linux/Android/iOS auto-resolution
- **Stacked Configs**: System → User → Local load order (later overrides earlier)
- **Merge Logic**: Values overwritten, section comments concatenated (`|`)
- **Header-Only**: Single file include, static inline functions, no linking required

YAML, TOML, and JSON? Be damned!

😤 Config isn't about whitespace; it's about simple clarity.

---

## The How and Why

Config should be easy like Git. It should be versatile and just work. You only need a few lines of code to build a deep, meaningful config system in an age-old format that actually works.

```c
#include "inimini.h"  // Include once per translation unit

int main(int argc, char **argv) {
    inimini_t *cfg = inimini_new();
    
    if (!inimini_load(cfg, "myapp", IMI_COMMENT)) {
        fprintf(stderr, "Config load failed\n");
        inimini_free(cfg);
        return 1;
    }
    
    const char *url     = inimini_getstr(cfg, "server.url", "http://localhost");
    int timeout         = inimini_getint(cfg, "network.timeout", 30);
    double rate_limit   = inimini_getdbl(cfg, "rate.limit", 1.0);
    
    size_t plugin_count = 0;
    char **plugins      = inimini_getarr(cfg, "plugins.enabled", &plugin_count);
    
    bool daemon_mode = inimini_isval(cfg, "core.daemonize", "true");
    
    // Modify if needed
    inimini_setint(cfg, "debug.mode", 1);
    inimini_comment(cfg, "server.url", "External address override");
    
    // Write back
    inimini_write(cfg, "./myapp.conf", IMI_KEEPENV | IMI_COMMENT);
    
    // Clean up arrays returned by getarr()
    for (size_t i = 0; i < plugin_count; i++) free(plugins[i]);
    free(plugins);
    
    inimini_free(cfg);
    return 0;
}
```

---

## Quick Start Guide

### 1. Setup
Copy `inimini.h` into your project. Include it **once** per source file:
```c
#include "inimini.h"
```

### 2. Load Config
Use `inimini_load()` to stack system/user/local configs automatically:
```c
inimini_t *cfg = inimini_new();
if (!inimini_load(cfg, "myapp", IMI_SUBSTYLE | IMI_COMMENTS)) {
    // Handle error
}
```

### 3. Read Values
Getters handle defaults and type conversion:
```c
const char *host = inimini_getstr(cfg, "db.host", "localhost");
int port = inimini_getint(cfg, "db.port", 5432);
```

### 4. Modify & Save
Change values in memory and write back to disk:
```c
inimini_setstr(cfg, "api.key", "new-secret-key");
inimini_write(cfg, "./config/myapp.conf", IMI_SUBSTYLE | IMI_COMMENTS);
```

---

## Flags Bitmask

Combine flags to control output style and behavior:

| Flag | Value | Effect |
|------|-------|--------|
| `IMI_INISTYLE` | `0x0000` | Classic INI: `[section] key = value` |
| `IMI_GITSTYLE` | `0x0001` | Git style: `[section "name"]` + indented children |
| `IMI_SUBSTYLE` | `0x0002` | Deep nesting: `[section.sub] key = value` |
| `IMI_KEEPVARS` | `0x0004` | Preserve `${VAR}` literals (don't expand) |
| `IMI_COMMENTS` | `0x0008` | Track/preserve inline/trailing comments |

**Example:** `flags = IMI_SUBSTYLE | IMI_COMMENTS;`

---

## Configuration Depth

Control nesting depth at compile time:
```c
#ifndef IMI_DOTDEPTH
#define IMI_DOTDEPTH 2  // Default: splits at 2nd dot [a.b] c.d = val
#endif
```

This determines how many dots are used to form the section header before writing the key.

---

## Memory Ownership Summary

**MUST FREE BY CALLER:**
- Config structs from `new/read/load/merge`
- Array pointers from `getarr()` + each element string
- Any new allocations explicitly documented above

**DO NOT FREE:**
- Strings from getters (`getstr/getint/getdbl`) — internal references tied to cfg lifetime
- Entry `comment` / `parent` fields — freed automatically with cfg
- Arrays from `getarr()` are cache copies — caller owns the array itself

**SAFETY:** `inimini_free()` is idempotent. Safe to call on NULL or multiple times.

---

## License

**0BSD** — Public Domain equivalent. Use anywhere, free of restrictions.

---

*My mommy told me you were the very best one... And I chose Y O U!!!*
