#include <ctype.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Defines how many layers deep the key parser can parse. This does not define
// how many layers the JSON parser itself can parse, merely how many layers
// of key values need to be stored in memory.
#ifndef JOSH_CONFIG_MAX_DEPTH
#define JOSH_CONFIG_MAX_DEPTH 16
#endif

// Defines how big of a memory pool Josh should allocate (on the stack). This
// is not the size of the Josh context itself, but the memory portion of the
// context. This memory section is mostly used for constructing JSON trees, not
// for doing extractions. Depending on how big your JSON data is, you might
// need to increase this value. Size is specified in bytes.
#ifndef JOSH_CONFIG_MAX_MEMORY
#define JOSH_CONFIG_MAX_MEMORY (unsigned)(1024 * 1024 * 8) // 8MB
#endif

// Allow for trailing comma support. This is not allowed by the spec, but is
// a common extension, and can be disabed easily in the parser if desired.
#ifndef JOSH_CONFIG_ALLOW_TRAILING_COMMA
#define JOSH_CONFIG_ALLOW_TRAILING_COMMA 0
#endif

enum josh_error {
	JOSH_ERROR_NONE,
	JOSH_ERROR_EXPECTED_ARRAY,
	JOSH_ERROR_EXPECTED_OBJECT,
	JOSH_ERROR_EMPTY_VALUE,
	JOSH_ERROR_STRING_NOT_CLOSED,
	JOSH_ERROR_DIGIT_EXPECTED,
	JOSH_ERROR_EXPECTED_TRUE,
	JOSH_ERROR_EXPECTED_FALSE,
	JOSH_ERROR_EXPECTED_NULL,
	JOSH_ERROR_EXPECTED_LITERAL,
	JOSH_ERROR_EXPECTED_KEY_CLOSING_BRACKET,
	JOSH_ERROR_EXPECTED_KEY_CLOSING_QUOTE,
	JOSH_ERROR_EXPECTED_KEY_VALUE,
	JOSH_ERROR_KEY_NUMBER_INVALID,
	JOSH_ERROR_ARRAY_INDEX_NOT_FOUND,
	JOSH_ERROR_OBJECT_KEY_NOT_FOUND,
	JOSH_ERROR_INVALID_ESCAPE_CODE,
	JOSH_ERROR_INVALID_UNICODE_ESCAPE_CODE,
	JOSH_ERROR_INVALID_KEY_OBJECT,
	JOSH_ERROR_EXPECTED_STRING,
	JOSH_ERROR_EXPECTED_COLON,
	JOSH_ERROR_NO_LEADING_ZERO,
	JOSH_ERROR_KEY_MAX_DEPTH_REACHED,
	JOSH_ERROR_OUT_OF_MEMORY,
	JOSH_ERROR_UNEXPECTED_CHAR,
	JOSH_ERROR_NO_TRAILING_COMMA,
};

enum josh_key_type_t {
	JOSH_KEY_TYPE_ARRAY,
	JOSH_KEY_TYPE_OBJECT,
};

struct josh_key_t {
	enum josh_key_type_t type;
	unsigned num;
	const char *str;
};

struct josh_ctx_t {
	const char *start;
	const char *ptr;
	size_t len;
	enum josh_error error_id;
	unsigned line;
	unsigned offset;
	unsigned column;

	struct josh_key_t keys[JOSH_CONFIG_MAX_DEPTH];
	unsigned key_count;

	unsigned current_index;
	unsigned current_level;
	unsigned match_count;
	bool found_key;
	bool create_node;
	const char *value_pos;

	size_t allocated;
	uint8_t memory[JOSH_CONFIG_MAX_MEMORY];
};

enum josh_node_type_t {
	JOSH_NODE_TYPE_NULL,
	JOSH_NODE_TYPE_TRUE,
	JOSH_NODE_TYPE_FALSE,
	JOSH_NODE_TYPE_INT,
	JOSH_NODE_TYPE_FLOAT,
	JOSH_NODE_TYPE_ARRAY,
	JOSH_NODE_TYPE_OBJECT,
};

struct josh_node_t {
	enum josh_node_type_t type;
	union {
		long double _float;
		long long int _int;
	} value;
};

static inline bool josh_is_value_terminator(char c) {
	return c == ',' || c == ']' || c == '}' || c == '\0';
}

void josh_reset(struct josh_ctx_t *ctx);
bool josh_parse_key(struct josh_ctx_t *ctx, const char *key);
bool josh_iter_value(struct josh_ctx_t *ctx);
bool josh_iter_array(struct josh_ctx_t *ctx);
bool josh_iter_object(struct josh_ctx_t *ctx);
bool josh_iter_string(struct josh_ctx_t *ctx);
bool josh_iter_number(struct josh_ctx_t *ctx);
bool josh_iter_literal(struct josh_ctx_t *ctx);
static inline char josh_iter_whitespace(struct josh_ctx_t *ctx);
static inline char josh_step_char(struct josh_ctx_t *ctx);
static inline char josh_step_n_chars(struct josh_ctx_t *ctx, unsigned n);
void *josh_malloc(struct josh_ctx_t *ctx, size_t bytes);

#define JOSH_ERROR(ctx, id) \
	(ctx)->error_id = (id); \
	(ctx)->offset = (unsigned)((ctx)->ptr - (ctx)->start); \
	(ctx)->len = 0;

void josh_reset(struct josh_ctx_t *ctx) {
	memset(ctx, 0, sizeof(*ctx));
	ctx->line = ctx->column = 1;
}

struct josh_node_t *josh_parse(struct josh_ctx_t *ctx, const char *json) {
	josh_reset(ctx);

	ctx->ptr = ctx->start = json;
	ctx->create_node = true;

	if (!*ctx->ptr) {
		JOSH_ERROR(ctx, JOSH_ERROR_EMPTY_VALUE);

		return NULL;
	}

	josh_iter_whitespace(ctx);

	struct josh_node_t *root = (void *)ctx->memory;

	if (josh_iter_value(ctx)) {
		if (!josh_iter_whitespace(ctx)) return root;

		JOSH_ERROR(ctx, JOSH_ERROR_UNEXPECTED_CHAR);

		return NULL;
	}

	return NULL;
}

#define josh_is_null(node) ((node)->type == JOSH_NODE_TYPE_NULL)
#define josh_is_true(node) ((node)->type == JOSH_NODE_TYPE_TRUE)
#define josh_is_false(node) ((node)->type == JOSH_NODE_TYPE_FALSE)
#define josh_is_bool(node) ((node)->type == JOSH_NODE_TYPE_TRUE || (node)->type == JOSH_NODE_TYPE_FALSE)
#define josh_is_int(node) ((node)->type == JOSH_NODE_TYPE_INT)
#define josh_is_float(node) ((node)->type == JOSH_NODE_TYPE_FLOAT)
#define josh_is_numeric(node) ((node)->type == JOSH_NODE_TYPE_INT || (node)->type == JOSH_NODE_TYPE_FLOAT)
#define josh_int_value(node) ((node)->value._int)
#define josh_float_value(node) ((node)->value._float)
#define josh_is_array(node) ((node)->type == JOSH_NODE_TYPE_ARRAY)
#define josh_is_array_empty(node) (true)
#define josh_is_object(node) ((node)->type == JOSH_NODE_TYPE_OBJECT)
#define josh_is_object_empty(node) (true)

const char *josh_extract(struct josh_ctx_t *ctx, const char *json, const char *key) {
	josh_reset(ctx);

	ctx->ptr = ctx->start = json;

	if (!josh_parse_key(ctx, key)) return NULL;

	if (!ctx->key_count) {
		ctx->found_key = true;
		ctx->value_pos = json;
	}

	if (!*ctx->ptr) {
		JOSH_ERROR(ctx, JOSH_ERROR_EMPTY_VALUE);

		return NULL;
	}

	josh_iter_whitespace(ctx);

	if (!ctx->key_count) {
		if (josh_iter_value(ctx)) {
			if (!josh_iter_whitespace(ctx)) return ctx->value_pos;

			JOSH_ERROR(ctx, JOSH_ERROR_UNEXPECTED_CHAR);

			return NULL;
		}
	}
	else if (ctx->keys[0].type == JOSH_KEY_TYPE_OBJECT) {
		if (josh_iter_object(ctx)) return ctx->value_pos;
	}
	else if (ctx->keys[0].type == JOSH_KEY_TYPE_ARRAY) {
		if (josh_iter_array(ctx)) return ctx->value_pos;
	}

	return NULL;
}

bool josh_iter_value(struct josh_ctx_t *ctx) {
	// Parse a JSON value from ctx. Return true if the function succeeds.

	const char c = *ctx->ptr;

	if (c == '\"') {
		if (!josh_iter_string(ctx)) return false;
	}
	else if (c == '[') {
		const unsigned temp = ctx->current_index;
		ctx->current_index = 0;
		ctx->current_level++;

		if (!josh_iter_array(ctx)) return false;

		ctx->current_level--;
		ctx->current_index = temp;
	}
	else if (c == '{') {
		ctx->current_level++;

		if (!josh_iter_object(ctx)) return false;

		ctx->current_level--;
	}
	else if (isdigit(c) || c == '-') {
		if (!josh_iter_number(ctx)) return false;
	}
	else {
		if (!josh_iter_literal(ctx)) return false;
	}

	return true;
}

bool josh_iter_array(struct josh_ctx_t *ctx) {
	// Iterate until the end of an array, exiting early if the index indicated
	// by `ctx->keys` is found. Return true upon success, or false on error.

	if (
		!ctx->found_key &&
		ctx->keys[ctx->current_level].type == JOSH_KEY_TYPE_ARRAY &&
		*ctx->ptr != '['
	) {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_ARRAY);

		return false;
	}

	josh_step_char(ctx);
	josh_iter_whitespace(ctx);

	if (ctx->create_node) {
		struct josh_node_t *node = josh_malloc(ctx, sizeof(struct josh_node_t));
		node->type = JOSH_NODE_TYPE_ARRAY;
	}

	for (;;) {
		if (*ctx->ptr == ']') {
			if (ctx->found_key || ctx->create_node) {
				josh_step_char(ctx);

				return true;
			}

			JOSH_ERROR(ctx, JOSH_ERROR_ARRAY_INDEX_NOT_FOUND);

			return false;
		}

		if (
			ctx->current_level < ctx->key_count &&
			ctx->keys[ctx->current_level].type == JOSH_KEY_TYPE_ARRAY &&
			ctx->keys[ctx->current_level].num == ctx->current_index
		) {
			ctx->match_count++;

			if (ctx->match_count == ctx->key_count) {
				ctx->found_key = true;
				ctx->value_pos = ctx->ptr;
			}
		}

		if (!josh_iter_value(ctx)) return false;

		if (ctx->found_key && ctx->current_level < ctx->key_count) break;

		josh_iter_whitespace(ctx);

		if (*ctx->ptr == ',') {
			ctx->current_index++;
			josh_step_char(ctx);
			const char c = josh_iter_whitespace(ctx);

#if JOSH_CONFIG_ALLOW_TRAILING_COMMA == 0
			if (c == ']') {
				JOSH_ERROR(ctx, JOSH_ERROR_NO_TRAILING_COMMA);

				return false;
			}
#endif
		}
	}

	return true;
}

bool josh_iter_object(struct josh_ctx_t *ctx) {
	// Iterate until the end of an object, exiting early if the key indicated
	// by `ctx->keys` is found. Return true upon success, or false on error.

	if (
		!ctx->found_key &&
		ctx->keys[ctx->current_level].type == JOSH_KEY_TYPE_OBJECT &&
		*ctx->ptr != '{'
	) {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_OBJECT);

		return false;
	}

	josh_step_char(ctx);
	josh_iter_whitespace(ctx);

	if (ctx->create_node) {
		struct josh_node_t *node = josh_malloc(ctx, sizeof(struct josh_node_t));
		node->type = JOSH_NODE_TYPE_OBJECT;
	}


	for (;;) {
		if (*ctx->ptr == '}') {
			if (ctx->found_key || ctx->create_node) {
				josh_step_char(ctx);

				return true;
			}

			JOSH_ERROR(ctx, JOSH_ERROR_OBJECT_KEY_NOT_FOUND);

			return false;
		}

		const char *key = ctx->ptr + 1;

		if (*ctx->ptr != '\"' || !josh_iter_string(ctx)) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_STRING);

			return false;
		}

		josh_iter_whitespace(ctx);

		if (*ctx->ptr != ':') {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_COLON);

			return false;
		}

		josh_step_char(ctx);
		josh_iter_whitespace(ctx);

		const unsigned old_match_count = ctx->match_count;

		if (
			ctx->current_level < ctx->key_count &&
			ctx->keys[ctx->current_level].type == JOSH_KEY_TYPE_OBJECT &&
			strncmp(
				ctx->keys[ctx->current_level].str,
				key,
				ctx->keys[ctx->current_level].num
			) == 0
		) {
			ctx->match_count++;

			if (ctx->match_count == ctx->key_count) {
				ctx->found_key = true;
				ctx->value_pos = ctx->ptr;
			}
		}

		if (!josh_iter_value(ctx)) return false;

		ctx->match_count = old_match_count;

		if (ctx->found_key && ctx->current_level < ctx->key_count) break;

		if (*ctx->ptr == ',') {
			josh_step_char(ctx);
			const char c = josh_iter_whitespace(ctx);

#if JOSH_CONFIG_ALLOW_TRAILING_COMMA == 0
			if (c == '}') {
				JOSH_ERROR(ctx, JOSH_ERROR_NO_TRAILING_COMMA);

				return false;
			}
#endif
		}
	}

	return true;
}

static inline bool josh_is_key_terminator(char c) {
	return c == '[' || c == '.';
}

bool josh_parse_key(struct josh_ctx_t *ctx, const char *key) {
	// Parse the JSON extraction schema (the key) into a codified format. Returns
	// false if an error occurs.

	while (*key) {
		if (ctx->key_count >= JOSH_CONFIG_MAX_DEPTH) {
			JOSH_ERROR(ctx, JOSH_ERROR_KEY_MAX_DEPTH_REACHED);

			return false;
		}

		if (*key == '[') {
			if (isdigit(key[1])) {
				unsigned index = 0;
				key++;

				for (;;) {
					const char c = *key++;

					if (c == ']') break;

					if (!c) {
						JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_CLOSING_BRACKET);

						return false;
					}

					if (c < '0' || c > '9') {
						JOSH_ERROR(ctx, JOSH_ERROR_KEY_NUMBER_INVALID);

						return false;
					}

					index = (index * 10) + ((unsigned)c - '0');
				}

				ctx->keys[ctx->key_count].num = index;
				ctx->keys[ctx->key_count].type = JOSH_KEY_TYPE_ARRAY;
				ctx->key_count++;
			}
			else if (key[1] == '\"') {
				const char *string_end = strchr(key + 2, '\"');

				if (!string_end) {
					JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_CLOSING_QUOTE);

					return false;
				}

				const unsigned len = (unsigned)(string_end - key - 2);

				if (key[len + 3] != ']') {
					JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_CLOSING_BRACKET);

					return false;
				}

				char *new_key = josh_malloc(ctx, len + 1);
				strncpy(new_key, key + 2, len);
				new_key[len] = '\0';

				ctx->keys[ctx->key_count].str = new_key;
				ctx->keys[ctx->key_count].num = len;
				ctx->keys[ctx->key_count].type = JOSH_KEY_TYPE_OBJECT;
				ctx->key_count++;

				key += len + 4;
			}
			else {
				JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_VALUE);

				return false;
			}
		}
		else if (*key == '.') {
			if (!key[1]) {
				JOSH_ERROR(ctx, JOSH_ERROR_INVALID_KEY_OBJECT);

				return false;
			}

			const char *start = key + 1;
			char c = 0;

			while ((c = *(++key))) {
				if (
					c == '_' ||
					(c >= 'A' && c <= 'Z') ||
					(c >= 'a' && c <= 'z') ||
					(c >= '0' && c <= '9')
				) {
					continue;
				}

				if (josh_is_key_terminator(c)) break;

				JOSH_ERROR(ctx, JOSH_ERROR_INVALID_KEY_OBJECT);

				return false;
			}

			const unsigned len = (unsigned)(key - start);

			char *new_key = josh_malloc(ctx, len + 1);
			strncpy(new_key, start, len);
			new_key[len] = '\0';

			ctx->keys[ctx->key_count].str = new_key;
			ctx->keys[ctx->key_count].num = len;
			ctx->keys[ctx->key_count].type = JOSH_KEY_TYPE_OBJECT;
			ctx->key_count++;
		}
		else {
			// should be impossible

			return false;
		}
	}

	return true;
}

bool josh_iter_string(struct josh_ctx_t *ctx) {
	// Iterate the context until the end of the current string. Return true
	// if the function succeeds.

	char c = josh_step_char(ctx);

	while (c) {
		if (c == '\\') {
			c = josh_step_char(ctx);

			if (
				c == '\"' ||
				c == '\\' ||
				c == '/' ||
				c == 'b' ||
				c == 'f' ||
				c == 'n' ||
				c == 't' ||
				c == 'r'
			) {
				c = josh_step_char(ctx);

				continue;
			}

			if (c == 'u') {
				for (unsigned i = 0; i < 4; i++) {
					c = josh_step_char(ctx);

					if (!isxdigit(c)) {
						JOSH_ERROR(ctx, JOSH_ERROR_INVALID_UNICODE_ESCAPE_CODE);

						return false;
					}
				}

				continue;
			}

			JOSH_ERROR(ctx, JOSH_ERROR_INVALID_ESCAPE_CODE);

			return false;
		}

		if (c == '\"') {
			josh_step_char(ctx);
			break;
		}

		c = josh_step_char(ctx);
	}

	if (!c) {
		JOSH_ERROR(ctx, JOSH_ERROR_STRING_NOT_CLOSED);

		return false;
	}

	return true;
}

bool josh_iter_number(struct josh_ctx_t *ctx) {
	// Iterate the context until the end of the current number. Return true
	// if the function succeeds.

	const char *start = ctx->ptr;

	if (*ctx->ptr == '-') josh_step_char(ctx);
	char c = *ctx->ptr;
	const char *started_at = ctx->ptr;

	if (c == '0' && isdigit(ctx->ptr[1])) {
		JOSH_ERROR(ctx, JOSH_ERROR_NO_LEADING_ZERO);

		return false;
	}

	while (c && isdigit(c)) c = josh_step_char(ctx);
	if (ctx->ptr == started_at) goto fail;

	bool is_floating_point = false;

	if (c == '.') {
		is_floating_point = true;

		c = josh_step_char(ctx);

		started_at = ctx->ptr;
		while (c && isdigit(c)) c = josh_step_char(ctx);
		if (ctx->ptr == started_at) goto fail;
	}

	if (c == 'e' || c == 'E') {
		is_floating_point = true;

		c = josh_step_char(ctx);

		if (c == '-' || c == '+') c = josh_step_char(ctx);

		started_at = ctx->ptr;
		while (c && isdigit(c)) c = josh_step_char(ctx);
		if (ctx->ptr == started_at) goto fail;
	}

	if (ctx->create_node) {
		struct josh_node_t *node = josh_malloc(ctx, sizeof(struct josh_node_t));

		// TODO: throw error for huge values (potentially replace with inf/nan)
		if (is_floating_point) {
			node->type = JOSH_NODE_TYPE_FLOAT;
			node->value._float = strtold(start, NULL);
		}
		else {
			node->type = JOSH_NODE_TYPE_INT;
			node->value._int = strtoll(start, NULL, 10);
		}
	}

	if (!josh_is_value_terminator(*ctx->ptr)) goto fail;

	return true;

fail:
	JOSH_ERROR(ctx, JOSH_ERROR_DIGIT_EXPECTED);

	return false;
}

bool josh_iter_literal(struct josh_ctx_t *ctx) {
	// Iterate to the end of a JSON literal, such as null, true, or false. Return
	// true if the function succeeds.

	const char c = *ctx->ptr;

	if (c == 't') {
		if (strcmp(ctx->ptr, "true") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_TRUE);

			return false;
		}

		if (ctx->create_node) {
			struct josh_node_t *node = josh_malloc(ctx, sizeof(struct josh_node_t));
			node->type = JOSH_NODE_TYPE_TRUE;
		}

		josh_step_n_chars(ctx, 4);
	}
	else if (c == 'f') {
		if (strcmp(ctx->ptr, "false") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_FALSE);

			return false;
		}

		if (ctx->create_node) {
			struct josh_node_t *node = josh_malloc(ctx, sizeof(struct josh_node_t));
			node->type = JOSH_NODE_TYPE_FALSE;
		}

		josh_step_n_chars(ctx, 5);
	}
	else if (c == 'n') {
		if (strcmp(ctx->ptr, "null") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_NULL);

			return false;
		}

		if (ctx->create_node) {
			struct josh_node_t *node = josh_malloc(ctx, sizeof(struct josh_node_t));
			node->type = JOSH_NODE_TYPE_NULL;
		}

		josh_step_n_chars(ctx, 4);
	}
	else {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_LITERAL);

		return false;
	}

	return true;
}

static inline char josh_iter_whitespace(struct josh_ctx_t *ctx) {
	// Iterate context to next non whitespace character.

	char c = *ctx->ptr;

	while (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f') {
		if (c == '\n') {
			ctx->line++;
			ctx->column = 0;
		}

		c = josh_step_char(ctx);
	}

	return c;
}

static inline char josh_step_char(struct josh_ctx_t *ctx) {
	return josh_step_n_chars(ctx, 1);
}

static inline char josh_step_n_chars(struct josh_ctx_t *ctx, unsigned n) {
	// Advance the context by n characters, returning the last character.

	ctx->ptr += n;
	ctx->column += n;
	if (ctx->found_key) ctx->len += n;

	return *ctx->ptr;
}

void *josh_malloc(struct josh_ctx_t *ctx, size_t bytes) {
	void *memory = ctx->memory + ctx->allocated;

	ctx->allocated += bytes;

	if (ctx->allocated > JOSH_CONFIG_MAX_MEMORY) {
		JOSH_ERROR(ctx, JOSH_ERROR_OUT_OF_MEMORY);

		return NULL;
	}

	return memory;
}
