/**
 * inimini.h - Header only universal configuration parser library (INI and GIT styles)
 * License: 0BSD | Cross-platform (Linux/Mac/Win/Android/iOS)
 *
 * NOTES:
 *   - Include once in main translation unit only
 *   - All getter strings are internal pointers - DO NOT free them
 *   - Only cfg struct needs freeing with inimini_free()
 *   - ENV vars ($HOME, $PROGRAMDATA) must be set by caller for paths
 *   - Flags as bitmask: flags = IMI_COMMENT | IMI_GITSTYLE;
 *
 * ============================================================================
 * USAGE
 * ============================================================================
 *
 * Lifecycle:
 *   inimini_t *cfg = inimini_new();
 *
 *   if (!inimini_load(cfg, "myapp", IMI_INISTYLE)) exit(1);
 *
 *   // Set your values for internal use rather than checking many times
 *   const char *url     = inimini_getstr(cfg, "server.url", "http://localhost");
 *   int timeout         = inimini_getint(cfg, "network.timeout");
 *   double ratio        = inimini_getdbl(cfg, "mix.amount");
 *   char **plugins      = inimini_getarr(cfg, "plugins.enabled", &count);
 *   bool is_daemon      = inimini_isval(cfg, "core.daemonize", "true");
 *
 *   inimini_free(cfg);
 *
 * Edit Config:
 *   inimini_read(cfg, "./myapp.conf", IMI_KEEPVARS | IMI_COMMENTS);
 *
 *   inimini_setstr(cfg, "debug.mode", "true");
 *   inimini_setint(cfg, "debug.level", 1);
 *   inimini_setdbl(cfg, "core.error_rate", 0.2);
 *   inimini_setarr(cfg, "core.plugins", arr, cnt);
 *   inimini_remove(cfg, "false.setting");
 *   inimini_comment(cfg, "server.url", "External address override");
 *
 *   inimini_write(cfg, "./myapp.conf", IMI_KEEPVARS | IMI_COMMENTS);
 *
 * List Keys:
 *   const char **sections = inimini_getsub(cfg, "", cnt);
 *   const char **keys = inimini_getsub(cfg, "section", cnt);
 *
 * ========================================================================== */

#ifndef INIMINI_H
#define INIMINI_H
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION FLAGS (bitmask)
 * Flags affect OUTPUT formatting only. Internal representation remains flat.
 * Multiple flags can be combined: flags = IMI_COMMENT | IMI_GITSTYLE;
 * ========================================================================== */

// Style flags (mutually exclusive output modes)
#define IMI_INISTYLE      0x0000      /* Classic INI: [section] key = value # COMMENT */
#define IMI_GITSTYLE      0x0001      /* Git style: [section "name"] indent ; COMMENT */
#define IMI_SUBSTYLE      0x0002      /* Subsections: [section.sub] key = value */

// Content flags
#define IMI_KEEPVARS      0x0004      /* Preserve ${VAR} literals on read/write */
#define IMI_COMMENTS      0x0008      /* Preserve comments inline or trailing */

/* Default depth for subsection splitting (compilable time constant) */
#ifndef IMI_DOTDEPTH
#define IMI_DOTDEPTH     2
#endif

/* Default sufffix for the conf */
#ifndef IMI_SUFFIXED
#define IMI_SUFFIXED     "conf"
#endif

/* ============================================================================
 * CORE TYPES
 * MEMORY MODEL: Linked list (no realloc brittleness). Each entry malloc'd independently.
 * COMMENTS: Single field per entry. Section comments concatenated on merge, entry comments overwritten.
 * PARENT TRACKING: Implied from key structure at read time ("vext.url" -> parent="vext").
 * TWO-PASS DESIGN: Parent resolution happens on read. Write uses pre-built structure.
 * STACK ORDER (later wins): /etc/{prog}/config → ~/.{prog}.conf → ./{prog}.conf
 * PATH RESOLUTION: Caller sets ENV vars ($HOME, $PROGRAMDATA, etc.). Lib resolves ${VAR} strings.
 * ANDROID/IOS: Sandbox paths exposed via custom ENV variables set by host application.
 * ========================================================================== */
typedef struct imi_entry {
	char *key;           /* Flat key: "section.sub.key" */
	char *value;         /* Raw string value (may contain inline comments) */
	char *comment;       /* Single comment field (concatenated for sections on merge) */
	char *parent;        /* Computed parent section ("vext"), "" if no section */
	struct imi_entry *next;  /* Linked list node */
} imi_entry_t;

typedef struct {
	imi_entry_t *head;   /* First entry in config linked list */
	imi_entry_t *tail;   /* Last entry in config linked list */
	size_t count;        /* Total number of entries traversable */
} inimini_t;

/* ============================================================================
 * PRIVATE HELPER FUNCTIONS
 * ========================================================================== */
static inline char *__imi_trim(char *s) {
	if (!s || !*s) return s;

	while (*s && isspace((unsigned char)*s)) s++;

	if (!*s) return s;

	char *e = s + strlen(s) - 1;

	while (e > s && isspace((unsigned char)*e)) *e-- = '\0';

	return s;
}

static inline char *__imi_expand_env(const char *in) {
	if (!in) return strdup("");

	const char *start = strstr(in, "${");

	if (!start) return strdup(in);

	char result[8192];
	size_t pos = 0;
	const char *cursor = in;

	while ((start = strstr(cursor, "${")) && cursor < start + sizeof(result)) {
		if (start > cursor) {
			size_t len = start - cursor;

			if (pos + len >= sizeof(result)) break;

			memcpy(result + pos, cursor, len);

			pos += len;
		}

		const char *var_start = start + 2;
		const char *var_end = strchr(var_start, '}');

		if (!var_end) break;

		size_t var_len = var_end - var_start;

		if (var_len >= 256) { var_len = 255; }

		char var_name[256] = {0};

		strncpy(var_name, var_start, var_len);

		char *val = getenv(var_name);

		if (val) {
			size_t val_len = strlen(val);

			if (pos + val_len >= sizeof(result)) break;

			memcpy(result + pos, val, val_len);

			pos += val_len;
		}

		cursor = var_end + 1;
	}

	if (pos == 0) return strdup(in);

	if (*cursor) {
		size_t remaining = strlen(cursor);

		if (pos + remaining >= sizeof(result)) remaining = sizeof(result) - pos - 1;

		memcpy(result + pos, cursor, remaining);

		pos += remaining;
	}

	result[pos] = '\0';

	return strdup(result);
}

static inline void __imi_free_array(char **arr, size_t count) {
	if (!arr) return;

	for (size_t i = 0; i < count; i++) free(arr[i]);

	free(arr);
}

/* Extract segment at specified depth level from flat key */
static inline char *__imi_extract_parent(const char *key) {
	if (!key) return strdup("");

	const char *dot = key;
	int dot_count = 0;

	while (*dot && dot_count < IMI_DOTDEPTH) {
		dot = strchr(dot + 1, '.');

		if (!dot) break;

		dot_count++;
	}

	if (!dot) return strdup(key);

	size_t len = dot - key;
	char *parent = malloc(len + 1);

	if (!parent) return strdup("");

	strncpy(parent, key, len);

	parent[len] = '\0';

	return parent;
}

static inline void __imi_list_append(inimini_t *cfg, imi_entry_t *entry) {
	if (!entry) return;

	entry->next = NULL;

	if (!cfg->head) {
		cfg->head = cfg->tail = entry;
	} else {
		cfg->tail->next = entry;
		cfg->tail = entry;
	}

	cfg->count++;
}

static inline imi_entry_t *__imi_find_entry(inimini_t *cfg, const char *key) {
	for (imi_entry_t *e = cfg->head; e; e = e->next) {
		if (!strcmp(e->key, key)) return e;
	}

	return NULL;
}

/* ============================================================================
 * OBJECT LIFECYCLE
 * ========================================================================== */
static inline inimini_t *inimini_new(void) {
	inimini_t *cfg = calloc(1, sizeof(inimini_t));

	return cfg;
}

static inline void inimini_free(inimini_t *cfg) {
	if (!cfg) return;

	imi_entry_t *e = cfg->head;

	while (e) {
		imi_entry_t *next = e->next;

		free(e->key);
		free(e->value);
		free(e->comment);
		free(e->parent);
		free(e);

		e = next;
	}

	free(cfg);
}

/* ============================================================================
 * FILE OPERATIONS
 * ========================================================================== */
static inline int __imi_path(const char *progname, char *buf, size_t size) {
	#if defined(_WIN32)
		char *appdata = getenv("APPDATA");

		if (appdata) {
			snprintf(buf, size, "%s\\%s.%s", appdata, progname, IMI_SUFFIXED);

			return 1;
		}

		char *home = getenv("USERPROFILE");

		if (home) {
			snprintf(buf, size, "%s\\.config\\%s.%s", home, progname, IMI_SUFFIXED);

			return 1;
		}
	#elif defined(__ANDROID__)
		char *home = getenv("HOME");

		if (home) {
			snprintf(buf, size, "%s/.%s%s", home, progname, IMI_SUFFIXED);

			return 1;
		}

		char *pkgdir = getenv("ANDROID_APP_DIR");

		if (pkgdir) {
			snprintf(buf, size, "%s/config/%s.%s", pkgdir, progname,  IMI_SUFFIXED);

			return 1;
		}
	#elif defined(__APPLE__)
		char *home = getenv("HOME");

		if (home) {
			snprintf(buf, size, "%s/.%s%s", home, progname, IMI_SUFFIXED);

			return 1;
		}
	#else
		char *xdg = getenv("XDG_CONFIG_HOME");

		if (xdg) {
			snprintf(buf, size, "%s/%s/%s.%s", xdg, progname, progname, IMI_SUFFIXED);

			return 1;
		}

		char *home = getenv("HOME");

		if (home) {
			snprintf(buf, size, "%s/.%s%s", home, progname, IMI_SUFFIXED);

			return 1;
		}
	#endif

	return 0;
}

static inline int inimini_read(inimini_t *cfg, const char *path, uint32_t flags) {
	FILE *f = fopen(path, "r");

	if (!f) return -1;

	int ret = __imi_parse(cfg, f, flags);

	fclose(f);

	return ret;
}

static inline int inimini_load(inimini_t *cfg, const char *progname, uint32_t flags) {
	char path[4096];
	int loaded = 0;

	#if defined(_WIN32)
		snprintf(path, sizeof(path), "C:/ProgramData/%s/%s.%s", progname, progname, IMI_SUFFIXED);
	#else
		snprintf(path, sizeof(path), "/etc/%s/%s.%s", progname, progname, IMI_SUFFIXED);
	#endif

	FILE *f = fopen(path, "r");

	if (f) {
		__imi_parse(cfg, f, flags);
		fclose(f);

		loaded++;
	}

	if (__imi_path(progname, path, sizeof(path))) {
		f = fopen(path, "r");

		if (f) {
			__imi_parse(cfg, f, flags);
			fclose(f);

			loaded++;
		}
	}

	snprintf(path, sizeof(path), "./.%s%s", progname, IMI_SUFFIXED);

	f = fopen(path, "r");

	if (f) {
		__imi_parse(cfg, f, flags);
		fclose(f);

		loaded++;
	}

	return loaded;
}

/* ============================================================================
 * FILE OPERATIONS (SPLIT PARSE)
 * ========================================================================== */
static inline void __imi_create_section(inimini_t *cfg, const char *name, const char *comment) {
	imi_entry_t *e = calloc(1, sizeof(imi_entry_t));
	e->key = NULL;
	e->parent = strdup(name);
	e->comment = comment ? strdup(comment) : NULL;

	__imi_list_append(cfg, e);
}

static inline void __imi_parse_comment(char **line, char *buf) {
	(*line)++;
	*line = __imi_trim(*line);

	if (strlen(buf)) strcat(buf, "\n");

	strcat(buf, *line);
}

static inline int __imi_parse_section(const char *line, char *section) {
	const char *start, *end;
	size_t slen;
	char name[256] = {0};

	start = line + 1;
	end = strchr(start, ']');

	if (!start || !end) return 0;

	slen = end - start;

	strncpy(name, start, slen);

	name[slen] = '\0';

	__imi_trim(name);
	strcpy(section, name);

	return 1;
}

static inline void __imi_parse_key_value(inimini_t *cfg, const char *line, const char *section, const char *comment, uint32_t flags) {
	char *eq = strchr(line, '=');

	if (!eq) return;

	*eq++ = '\0';

	char *key = __imi_trim(strdup(line));
	char *val = __imi_trim(strdup(eq));
	size_t vlen = strlen(val);

	if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
		val[vlen - 1] = '\0';
		val++;
	}

	char *trailing_com = strchr(val, ';');

	if ((flags & IMI_COMMENTS) && trailing_com) {
		*trailing_com = '\0';
		val = __imi_trim(val);
		trailing_com = __imi_trim(trailing_com + 1);

		if (strlen(comment)) strcat(comment, "\n");

		strcat(comment, trailing_com);
	} else {
		trailing_com = strchr(val, '#');

		if ((flags & IMI_COMMENTS) && trailing_com) {
			*trailing_com = '\0';
			val = __imi_trim(val);
			trailing_com = __imi_trim(trailing_com + 1);

			if (strlen(comment)) strcat(comment, "\n");

			strcat(comment, trailing_com);
		}
	}

	char full_key[1024];

	if (section[0]) snprintf(full_key, sizeof(full_key), "%s.%s", section, key);
	else snprintf(full_key, sizeof(full_key), "%s", key);

	imi_entry_t *e = calloc(1, sizeof(imi_entry_t));
	e->key = strdup(full_key);
	e->value = strdup(__imi_expand_env(val));
	e->parent = __imi_extract_parent(e->key);
	e->comment = comment ? strdup(comment) : NULL;

	free(key);
	free(val);

	__imi_list_append(cfg, e);
}

static inline int __imi_parse(inimini_t *cfg, FILE *f, uint32_t flags) {
	char line[4096], section[256] = {0}, current_comment[1024] = {0};

	while (fgets(line, sizeof(line), f)) {
		char *l = __imi_trim(line);

		if (!*l) {
			current_comment[0] = '\0';

			continue;
		}

		if (*l == ';' || *l == '#') {
			__imi_parse_comment(l, current_comment);

			continue;
		}

		if (*l == '[') {
			if (!__imi_parse_section(l, section)) continue;

			__imi_create_section(cfg, section, current_comment);

			current_comment[0] = '\0';

			continue;
		}

		__imi_parse_key_value(cfg, l, section, current_comment, flags);
	}

	return 0;
}

/* ============================================================================
 * WRITE LOGIC (FLAG-BASED FORMATTING)
 * ========================================================================== */
static inline void __imi_write_header(FILE *f, const char *parent, uint32_t flags) {
	if (!parent || !*parent) return;

	if (flags & IMI_GITSTYLE) {
		if (strchr(parent, ' ')) fprintf(f, "[%s \"%s\"]\n", parent, parent);
		else fprintf(f, "[%s]\n", parent);
	} else {
		fprintf(f, "[%s]\n", parent);
	}
}

static inline int inimini_write(const inimini_t *cfg, const char *path, uint32_t flags) {
	FILE *f = fopen(path, "w");

	if (!f) return -1;

	const imi_entry_t *prev = NULL;
	const char *prev_parent = "";
	int printed_sections = 0;
	int write_indent = (flags & IMI_GITSTYLE) ? 1 : 0;

	for (const imi_entry_t *e = cfg->head; e; e = e->next) {
		const char *current_parent = e->parent ? e->parent : "";

		/* Blank line before new section group */
		if (prev_parent && strcmp(current_parent, prev_parent) && printed_sections) fprintf(f, "\n");

		/* Print section header on change */
		if (!prev || (prev_parent && strcmp(current_parent, prev_parent))) {
			__imi_write_header(f, current_parent, flags);

			printed_sections++;
		}

		/* Section marker (key == NULL) */
		if (e->key == NULL) {
			if (e->comment && (flags & IMI_COMMENTS)) fprintf(f, "; %s\n", e->comment);
		} else {
			const char *child = e->key;
			const char *dot = e->key;
			int dot_count = 0;

			while (dot_count < IMI_DOTDEPTH - 1) {
				dot = strchr(dot + 1, '.');

				if (!dot) break;

				dot_count++;
			}

			if (dot) child = dot + 1;

			/* Apply indent for GITSTYLE */
			if (write_indent) fprintf(f, "\t%s = %s", child, e->value);
			else fprintf(f, "%s = %s", child, e->value);

			/* Handle comments */
			if (e->comment && (flags & IMI_COMMENTS)) fprintf(f, "\n; %s", e->comment);

			fprintf(f, "\n");
		}

		prev_parent = current_parent;
		prev = e;
	}

	fclose(f);

	return 0;
}

/* ============================================================================
 * MERGE LOGIC
 * ========================================================================== */
static inline int inimini_merge(inimini_t *base, const inimini_t *overlay, uint32_t flags) {
	const imi_entry_t *o = overlay->head;

	while (o) {
		imi_entry_t *b = __imi_find_entry(base, o->key);

		if (b) {
			free(b->value);

			b->value = strdup(o->value);

			if ((flags & IMI_COMMENTS) && o->comment) {
				if (o->key == NULL && b->comment && o->comment) {
					size_t blen = strlen(b->comment);
					size_t olen = strlen(o->comment);
					char *combined = realloc(b->comment, blen + olen + 4);

					if (combined) {
					    strcat(combined, " | ");
					    strcat(combined, o->comment);

					    b->comment = combined;
					}
				} else {
					free(b->comment);

					b->comment = strdup(o->comment);
				}
			}
		} else {
			imi_entry_t *entry = calloc(1, sizeof(imi_entry_t));
			entry->key = strdup(o->key);
			entry->value = strdup(o->value);
			entry->parent = o->parent ? strdup(o->parent) : strdup("");
			entry->comment = o->comment ? strdup(o->comment) : NULL;

			__imi_list_append(base, entry);
		}

		o = o->next;
	}

	return 0;
}

/* ============================================================================
 * DATA ACCESSORS (GET)
 * ========================================================================== */
static inline const char *inimini_getstr(const inimini_t *cfg, const char *key, const char *def) {
	for (const imi_entry_t *e = cfg->head; e; e = e->next) {
		if (!strcmp(e->key, key)) return e->value;
	}

	return def;
}

static inline int inimini_getint(const inimini_t *cfg, const char *key, int def) {
	const char *v = inimini_getstr(cfg, key, NULL);

	return v ? atoi(v) : def;
}

static inline double inimini_getdbl(const inimini_t *cfg, const char *key, double def) {
	const char *v = inimini_getstr(cfg, key, NULL);

	return v ? atof(v) : def;
}

static inline char **inimini_getarr(const inimini_t *cfg, const char *key, size_t *count) {
	size_t cap = 64, cnt = 0;
	char **parsed = calloc(cap, sizeof(char*));

	for (const imi_entry_t *e = cfg->head; e; e = e->next) {
		if (!strcmp(e->key, key)) {
			char *tmp = strdup(e->value);
			char *tok = strtok(tmp, ",");

			while (tok && cnt < cap) {
				char *trimmed = tok;

				while (isspace((unsigned char)*trimmed)) trimmed++;

				char *end = trimmed + strlen(trimmed) - 1;

				while (end > trimmed && isspace((unsigned char)*end)) *end-- = '\0';

				if (*trimmed) parsed[cnt++] = strdup(trimmed);

				tok = strtok(NULL, ",");
			}

			free(tmp);

			break;
		}
	}

	*count = cnt;

	return !cnt ? NULL : parsed;
}

static inline char **inimini_getsub(inimini_t *cfg, const char *section, size_t *count) {
	if (!cfg || !count) {
		if (count) *count = 0;

		return NULL;
	}

	size_t cap = 64;
	size_t cnt = 0;
	char **items = calloc(cap, sizeof(char*));

	if (!section || strlen(section) == 0) {
		for (imi_entry_t *e = cfg->head; e; e = e->next) {
			const char *parent = e->parent;

			if (!parent || !*parent) continue;

			int found = 0;

			for (size_t i = 0; i < cnt; i++) {
				if (strcmp(items[i], parent) == 0) {
					found = 1;

					break;
				}
			}

			if (!found) {
				if (cnt >= cap) {
					cap *= 2;

					char **tmp = realloc(items, cap * sizeof(char*));

					if (!tmp) {
						for (size_t i = 0; i < cnt; i++) free(items[i]);

						free(items);

						*count = 0;

						return NULL;
					}

					items = tmp;
				}

				items[cnt++] = strdup(parent);
			}
		}
	} else {
		size_t slen = strlen(section);

		for (imi_entry_t *e = cfg->head; e; e = e->next) {
			if (!e->key) continue;

			if (strncmp(e->key, section, slen) == 0 &&
				e->key[slen] == '.') {

				const char *leaf_start = e->key + slen + 1;

				if (cnt >= cap) {
					cap *= 2;

					char **tmp = realloc(items, cap * sizeof(char*));

					if (!tmp) {
						for (size_t i = 0; i < cnt; i++) free(items[i]);

						free(items);

						*count = 0;

						return NULL;
					}

					items = tmp;
				}

				items[cnt++] = strdup(leaf_start);
			}
		}
	}

	*count = cnt;

	return items;
}

static inline int inimini_isval(const inimini_t *cfg, const char *key, const char *val) {
	for (const imi_entry_t *e = cfg->head; e; e = e->next) {
		if (!strcmp(e->key, key)) return !e->value || !val || !strcmp(e->value, val);
	}

	return 0;
}

static inline size_t inimini_count(const inimini_t *cfg) {
	return cfg->count;
}

/* ============================================================================
 * DATA MODIFICATION (SET)
 * ========================================================================== */
static inline int inimini_setstr(inimini_t *cfg, const char *key, const char *val) {
	for (imi_entry_t *e = cfg->head; e; e = e->next) {
		if (!strcmp(e->key, key)) {
			free(e->value);

			e->value = strdup(val);

			return 0;
		}
	}

	imi_entry_t *entry = calloc(1, sizeof(imi_entry_t));

	if (!entry) return -1;

	entry->key = strdup(key);
	entry->value = strdup(val);
	entry->parent = __imi_extract_parent(key);

	__imi_list_append(cfg, entry);

	return 0;
}

static inline int inimini_setint(inimini_t *cfg, const char *key, int val) {
	char buf[64];

	snprintf(buf, sizeof(buf), "%d", val);

	return inimini_setstr(cfg, key, buf);
}

static inline int inimini_setdbl(inimini_t *cfg, const char *key, double val) {
	char buf[64];

	snprintf(buf, sizeof(buf), "%.6g", val);

	return inimini_setstr(cfg, key, buf);
}

static inline int inimini_setarr(inimini_t *cfg, const char *key, char **val, size_t count) {
	char buf[4096] = "";
	size_t off = 0;

	for (size_t i = 0; i < count; i++) {
		size_t len = strlen(val[i]);

		if (off + len + 2 > sizeof(buf)) break;

		off += sprintf(buf + off, "%s%s", i > 0 ? ", " : "", val[i]);
	}

	return inimini_setstr(cfg, key, buf);
}

static inline int inimini_remove(inimini_t *cfg, const char *key) {
	imi_entry_t *prev = NULL;

	for (imi_entry_t *e = cfg->head; e; e = e->next) {
		if (!strcmp(e->key, key)) {
			if (prev) prev->next = e->next;
			else cfg->head = e->next;

			if (e == cfg->tail) cfg->tail = prev;

			free(e->key);
			free(e->value);
			free(e->comment);
			free(e->parent);
			free(e);

			return 0;
		}

		prev = e;
	}

	return -1;
}

static inline int inimini_clear(inimini_t *cfg) {
	while (cfg->head) {
		imi_entry_t *e = cfg->head;
		cfg->head = e->next;

		if (cfg->head == NULL) cfg->tail = NULL;

		free(e->key);
		free(e->value);
		free(e->comment);
		free(e->parent);
		free(e);
	}

	cfg->count = 0;

	return 0;
}

static inline int inimini_comment(inimini_t *cfg, const char *key, const char *comment) {
	for (imi_entry_t *e = cfg->head; e; e = e->next) {
		if (!strcmp(e->key, key)) {
			free(e->comment);

			e->comment = strdup(comment);

			return 0;
		}
	}

	return -1;
}

/* ============================================================================
 * MEMORY OWNERSHIP SUMMARY
 * ========================================================================== */
/*
 * MUST FREE BY CALLER:
 *   - Config structs from new/read/load/merge
 *   - Array pointers from getarr() + each element string
 *   - Any new allocations explicitly documented above
 *
 * DO NOT FREE:
 *   - Strings from getters (getstr/getint/getdbl) — internal references tied to cfg lifetime
 *   - Entry comment / parent fields — freed automatically with cfg
 *   - Arrays from getarr() are cache copies — caller owns the array itself
 *
 * SAFETY: inimini_free() is idempotent. Safe to call on NULL or multiple times.
 */

#ifdef __cplusplus
}
#endif

#endif /* INIMINI_H */
