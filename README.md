# Inimini Miny Moe - "Config the thing and let it go!"

Inimini is a universal configuration parser for INI and Git-style config files. Header-only, zero dependencies.

- **Git Styles**: `[section] key = value # comment` & `[section "topic"] key = value ; comment`
- **Sub Headings & Sub Keys**: `[section.topic] deep.key = value` with depth  splits.
- **Cross-Platform Paths**: Windows/macOS/Linux/Android/iOS auto-resolution
- **Comment Preservation**: Inline and trailing comments retained with `IMI_COMMENT` flag
- **Environment Expansion**: `${VAR}` variables expanded on read/write
- **Stacked Configs**: System → User → Local load order (later overrides earlier)
- **Array Parsing**: Comma-separated values trimmed automatically
- **Merge Logic**: Values overwritten, section comments concatenated
- **Header-Only**: Single file include, static inline functions

---

## The How and Why...

Config should be easy like git. It should be versatile and just work. You only need a few lines of code to make a deep and meaningful config system in an age old format that just works!

YAML, TOML, and JSON be damned!!! - Sorry guy - config is not whitespace. 

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
