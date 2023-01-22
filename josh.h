#include <ctype.h>
#include <stdbool.h>

enum josh_error {
	JOSH_ERROR_NONE,
	JOSH_ERROR_EXPECTED_ARRAY,
	JOSH_ERROR_EMPTY_VALUE,
	JOSH_ERROR_STRING_NOT_CLOSED,
	JOSH_ERROR_NUMBER_INVALID,
	JOSH_ERROR_EXPECTED_TRUE,
	JOSH_ERROR_EXPECTED_FALSE,
	JOSH_ERROR_EXPECTED_NULL,
	JOSH_ERROR_EXPECTED_LITERAL,
	JOSH_ERROR_EXPECTED_KEY_ARRAY,
	JOSH_ERROR_KEY_NUMBER_INVALID,
	JOSH_ERROR_ARRAY_INDEX_NOT_FOUND,
	JOSH_ERROR_INVALID_ESCAPE_CODE,
	JOSH_ERROR_INVALID_UNICODE_ESCAPE_CODE,
};

struct josh_ctx_t {
	const char *start;
	const char *ptr;
	size_t len;
	enum josh_error error_id;
	unsigned line;
	unsigned offset;
	unsigned column;

	// subject to change
	unsigned key;
	unsigned current_index;
	unsigned current_level;
	bool found_key;
};

static inline bool josh_is_value_terminator(char c) {
	return c == ',' || c == ']' || c == '}';
}

bool josh_parse_key(struct josh_ctx_t *ctx, const char *key);
const char *josh_iter_value(struct josh_ctx_t *ctx);
bool josh_iter_string(struct josh_ctx_t *ctx);
bool josh_iter_number(struct josh_ctx_t *ctx);
bool josh_iter_literal(struct josh_ctx_t *ctx);
static inline void josh_iter_whitespace(struct josh_ctx_t *ctx);
static inline char josh_step_char(struct josh_ctx_t *ctx);

#define JOSH_ERROR(ctx, id) \
	(ctx)->error_id = (id); \
	(ctx)->line = 1; \
	(ctx)->column = (unsigned)((ctx)->ptr -(ctx)->start + 1); \
	(ctx)->offset = (unsigned)((ctx)->ptr - (ctx)->start); \
	(ctx)->len = 0;

const char *josh_extract(struct josh_ctx_t *ctx, const char *json, const char *key) {
	ctx->ptr = json;
	ctx->start = json;
	ctx->current_index = 0;
	ctx->current_level = 0;
	ctx->found_key = false;
	ctx->error_id = JOSH_ERROR_NONE;

	if (!josh_parse_key(ctx, key)) return NULL;

	if (!*ctx->ptr) {
		JOSH_ERROR(ctx, JOSH_ERROR_EMPTY_VALUE);

		return NULL;
	}

	josh_iter_whitespace(ctx);

	return josh_iter_value(ctx);
}

const char *josh_iter_value(struct josh_ctx_t *ctx) {
	if (*ctx->ptr != '[') {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_ARRAY);

		return NULL;
	}

	josh_step_char(ctx);

	josh_iter_whitespace(ctx);

	const char *value_pos = ctx->ptr;
	char c;
	bool found_key = false;

	for (;;) {
		c = *ctx->ptr;

		if (c == ']') {
			if (ctx->found_key) return value_pos;

			// TODO: throw error if "]" comes after ","
			JOSH_ERROR(ctx, JOSH_ERROR_ARRAY_INDEX_NOT_FOUND);

			return NULL;
		}

		if (ctx->key == ctx->current_index && ctx->current_level == 0) {
			ctx->found_key = true;
			found_key = true;
			value_pos = ctx->ptr;
		}

		if (c == '\"') {
			if (!josh_iter_string(ctx)) return NULL;
		}
		else if (c == '[') {
			const unsigned temp = ctx->current_index;
			ctx->current_index = 0;
			ctx->current_level++;

			const char *pos = josh_iter_value(ctx);

			ctx->current_level--;
			ctx->current_index = temp;

			if (!pos) return NULL;

			if (!found_key) value_pos = pos;

			// TODO: check that "]" actually exists
			josh_step_char(ctx);
		}
		else if (isdigit(c) || c == '-') {
			if (!josh_iter_number(ctx)) return NULL;
		}
		else {
			if (!josh_iter_literal(ctx)) return NULL;
		}

		if (found_key) break;

		josh_iter_whitespace(ctx);
		c = *ctx->ptr;

		if (c == ',') {
			ctx->current_index++;
			josh_step_char(ctx);
			josh_iter_whitespace(ctx);
		}
	}

	return value_pos;
}

bool josh_parse_key(struct josh_ctx_t *ctx, const char *key) {
	// Parse the JSON extraction schema (the key) into a codified format. Returns
	// false if an error occurs.

	const size_t len = strlen(key);

	if (key[0] != '[' || key[len - 1] != ']') {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_KEY_ARRAY);

		return false;
	}

	unsigned index = 0;

	for (unsigned i = 1; i < len - 1; i++) {
		const unsigned num = key[i];

		if (num < '0' || num > '9') {
			JOSH_ERROR(ctx, JOSH_ERROR_KEY_NUMBER_INVALID);

			return false;
		}

		index = (index * 10) + (num - '0');
	}

	ctx->key = index;
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

	josh_step_char(ctx);

	if (ctx->ptr[-1] == '-' && !isdigit(*ctx->ptr)) {
		JOSH_ERROR(ctx, JOSH_ERROR_NUMBER_INVALID);

		return false;
	}

	char c = *ctx->ptr;
	bool has_seen_period = false;

	while (c && (isdigit(c) || c == '.')) {
		if (c == '.') {
			if (has_seen_period) {
				JOSH_ERROR(ctx, JOSH_ERROR_NUMBER_INVALID);

				return false;
			}

			has_seen_period = true;
		}

		c = josh_step_char(ctx);
	}

	if (!josh_is_value_terminator(*ctx->ptr)) {
		JOSH_ERROR(ctx, JOSH_ERROR_NUMBER_INVALID);

		return false;
	}

	return true;
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

	while (c == ' ' || c == '\t' || c == '\f' || c == '\r') {
		c = josh_step_char(ctx);
	}
}

static inline char josh_step_char(struct josh_ctx_t *ctx) {
	// Advance the context by a single character, returning the new character.

	if (ctx->found_key) ctx->len++;
	return *(++ctx->ptr);
}
