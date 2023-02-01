#include <ctype.h>
#include <stdbool.h>
#include <string.h>

// Defines how many layers deep the key parser can parse. This does not define
// how many layers the JSON parser itself can parse, merely how many layers
// of key values need to be stored in memory.
#ifndef JOSH_CONFIG_MAX_DEPTH
#define JOSH_CONFIG_MAX_DEPTH 16
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
};

enum josh_key_type_t {
	JOSH_KEY_TYPE_ARRAY,
	JOSH_KEY_TYPE_OBJECT,
};

struct josh_key_t {
	enum josh_key_type_t type;
	union {
		unsigned num;
		const char *str;
	} key;
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
	const char *current_index_str;
	unsigned current_level;
	bool found_key;
	const char *value_pos;
};

static inline bool josh_is_value_terminator(char c) {
	return c == ',' || c == ']' || c == '}';
}

bool josh_parse_key(struct josh_ctx_t *ctx, const char *key);
bool josh_iter_value(struct josh_ctx_t *ctx);
bool josh_iter_array(struct josh_ctx_t *ctx);
bool josh_iter_object(struct josh_ctx_t *ctx);
bool josh_iter_string(struct josh_ctx_t *ctx);
bool josh_iter_number(struct josh_ctx_t *ctx);
bool josh_iter_literal(struct josh_ctx_t *ctx);
static inline void josh_iter_whitespace(struct josh_ctx_t *ctx);
static inline char josh_step_char(struct josh_ctx_t *ctx);

#define JOSH_ERROR(ctx, id) \
	(ctx)->error_id = (id); \
	(ctx)->offset = (unsigned)((ctx)->ptr - (ctx)->start); \
	(ctx)->len = 0;

const char *josh_extract(struct josh_ctx_t *ctx, const char *json, const char *key) {
	memset(ctx, 0, sizeof(*ctx));
	ctx->ptr = ctx->start = json;
	ctx->line = ctx->column = 1;

	if (!josh_parse_key(ctx, key)) return NULL;

	if (!*ctx->ptr) {
		JOSH_ERROR(ctx, JOSH_ERROR_EMPTY_VALUE);

		return NULL;
	}

	josh_iter_whitespace(ctx);

	if (*ctx->ptr == '{') {
		if (josh_iter_object(ctx)) return ctx->value_pos;
	}
	else if (josh_iter_array(ctx)) {
		return ctx->value_pos;
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
		const char *temp = ctx->current_index_str;
		ctx->current_index_str = NULL;
		ctx->current_level++;

		if (!josh_iter_object(ctx)) return false;

		ctx->current_level--;
		ctx->current_index_str = temp;
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

	for (;;) {
		if (*ctx->ptr == ']') {
			if (ctx->found_key) {
				josh_step_char(ctx);

				return true;
			}

			// TODO: throw error if "]" comes after ","
			JOSH_ERROR(ctx, JOSH_ERROR_ARRAY_INDEX_NOT_FOUND);

			return false;
		}

		if (
			(ctx->current_level == ctx->key_count - 1) &&
			ctx->keys[ctx->current_level].type == JOSH_KEY_TYPE_ARRAY &&
			ctx->keys[ctx->current_level].key.num == ctx->current_index
		) {
			ctx->found_key = true;
			ctx->value_pos = ctx->ptr;
		}

		if (!josh_iter_value(ctx)) return false;

		if (ctx->found_key && ctx->current_level < ctx->key_count) break;

		josh_iter_whitespace(ctx);

		if (*ctx->ptr == ',') {
			ctx->current_index++;
			josh_step_char(ctx);
			josh_iter_whitespace(ctx);
		}
	}

	return true;
}

bool josh_iter_object(struct josh_ctx_t *ctx) {
	// Iterate until the end of an object, exiting early if the key indicated
	// by `ctx->keys` is found. Return true upon success, or false on error.

	if (
		!ctx->found_key &&
		ctx->keys[0].type == JOSH_KEY_TYPE_OBJECT &&
		*ctx->ptr != '{'
	) {
		// TODO: test this
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_OBJECT);

		return false;
	}

	josh_step_char(ctx);
	josh_iter_whitespace(ctx);

	for (;;) {
		if (*ctx->ptr == '}') {
			if (ctx->found_key) {
				josh_step_char(ctx);

				return true;
			}

			// TODO: throw error if "}" comes after ","
			JOSH_ERROR(ctx, JOSH_ERROR_OBJECT_KEY_NOT_FOUND);

			return false;
		}

		const char *string_start_pos = ctx->ptr + 1;

		if (*ctx->ptr != '\"' || !josh_iter_string(ctx)) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_STRING);

			return false;
		}

		const char *string_end_pos = ctx->ptr - 1;

		josh_iter_whitespace(ctx);

		if (*ctx->ptr != ':') {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_COLON);

			return false;
		}

		josh_step_char(ctx);
		josh_iter_whitespace(ctx);

		if (
			(ctx->current_level == ctx->key_count - 1) &&
			ctx->keys[ctx->current_level].type == JOSH_KEY_TYPE_OBJECT &&
			strncmp(
				ctx->keys[ctx->current_level].key.str,
				string_start_pos,
				(unsigned)(string_end_pos - string_start_pos)
			) == 0
		) {
			ctx->found_key = true;
			ctx->value_pos = ctx->ptr;
		}

		if (!josh_iter_value(ctx)) return false;

		if (ctx->found_key && ctx->current_level < ctx->key_count) break;

		if (*ctx->ptr == ',') {
			josh_step_char(ctx);
			josh_iter_whitespace(ctx);
		}
	}

	return true;
}

static inline bool josh_is_key_terminator(char c) {
	return c == ']' || c == '.';
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
					const char num = *key++;

					if (josh_is_key_terminator(num)) {
						break;
					}

					if (!num) {
						JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_CLOSING_BRACKET);

						return false;
					}

					if (num < '0' || num > '9') {
						JOSH_ERROR(ctx, JOSH_ERROR_KEY_NUMBER_INVALID);

						return false;
					}

					index = (index * 10) + ((unsigned)num - '0');
				}

				ctx->keys[ctx->key_count].key.num = index;
				ctx->keys[ctx->key_count].type = JOSH_KEY_TYPE_ARRAY;
				ctx->key_count++;
			}
			else if (key[1] == '\"') {
				// TODO: make a copy of this string. Currently we just take a
				// pointer to the key, which means that it will not be null
				// terminated. This shouldn't be an issue because a string
				// cannot be matched to quote ('"'), as it must be escaped.
				// Still, this hackery should be averted if possible.
				ctx->keys[ctx->key_count].key.str = key + 2;
				ctx->keys[ctx->key_count].type = JOSH_KEY_TYPE_OBJECT;
				ctx->key_count++;

				key += 2;

				while (*key && *key != '\"') key++;

				if (!*key++) {
					JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_CLOSING_QUOTE);

					return false;
				}

				if (*key++ != ']') {
					JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_CLOSING_BRACKET);

					return false;
				}
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
			char c = *(++key);

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

			ctx->keys[ctx->key_count].key.str = start;
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
			else if (c == 'u') {
				for (unsigned i = 0; i < 4; i++) {
					c = josh_step_char(ctx);

					if (!isxdigit(c)) {
						JOSH_ERROR(ctx, JOSH_ERROR_INVALID_UNICODE_ESCAPE_CODE);

						return false;
					}
				}

				continue;
			}
			else {
				JOSH_ERROR(ctx, JOSH_ERROR_INVALID_ESCAPE_CODE);

				return false;
			}
		}
		else if (c == '\"') {
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

	if (*ctx->ptr == '-') josh_step_char(ctx);
	char c = *ctx->ptr;
	const char *started_at = ctx->ptr;

	if (c == '0' && ctx->ptr[1] != '.') {
		JOSH_ERROR(ctx, JOSH_ERROR_NO_LEADING_ZERO);

		return false;
	}

	while (c && isdigit(c)) c = josh_step_char(ctx);
	if (ctx->ptr == started_at) goto fail;

	if (c == '.') {
		c = josh_step_char(ctx);

		started_at = ctx->ptr;
		while (c && isdigit(c)) c = josh_step_char(ctx);
		if (ctx->ptr == started_at) goto fail;
	}

	if (c == 'e' || c == 'E') {
		c = josh_step_char(ctx);

		if (c == '-' || c == '+') c = josh_step_char(ctx);

		started_at = ctx->ptr;
		while (c && isdigit(c)) c = josh_step_char(ctx);
		if (ctx->ptr == started_at) goto fail;
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

		ctx->ptr += 4;
		if (ctx->found_key) ctx->len += 4;
	}
	else if (c == 'f') {
		if (strcmp(ctx->ptr, "false") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_FALSE);

			return false;
		}

		ctx->ptr += 5;
		if (ctx->found_key) ctx->len += 5;
	}
	else if (c == 'n') {
		if (strcmp(ctx->ptr, "null") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_NULL);

			return false;
		}

		ctx->ptr += 4;
		if (ctx->found_key) ctx->len += 4;
	}
	else {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_LITERAL);

		return false;
	}

	return true;
}

static inline void josh_iter_whitespace(struct josh_ctx_t *ctx) {
	// Iterate context to next non whitespace character.

	char c = *ctx->ptr;

	while (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f') {
		if (c == '\n') {
			ctx->line++;
			ctx->column = 0;
		}

		c = josh_step_char(ctx);
	}
}

static inline char josh_step_char(struct josh_ctx_t *ctx) {
	// Advance the context by a single character, returning the new character.

	ctx->column++;
	if (ctx->found_key) ctx->len++;
	return *(++ctx->ptr);
}
