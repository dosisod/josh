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

	const char *x = josh_iter_value(ctx);
	return x;
}

const char *josh_iter_value(struct josh_ctx_t *ctx) {
	if (*ctx->ptr != '[') {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_ARRAY);

		return NULL;
	}

	ctx->ptr++;
	if (ctx->found_key) ctx->len++;

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
			if (josh_iter_string(ctx)) return NULL;
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
			ctx->ptr++;
			if (ctx->found_key) ctx->len++;
		}
		else if (isdigit(c) || c == '-') {
			if (josh_iter_number(ctx)) return NULL;
		}
		else {
			if (josh_iter_literal(ctx)) return NULL;
		}

		if (found_key) break;

		josh_iter_whitespace(ctx);
		c = *ctx->ptr;

		if (c == ',') {
			ctx->current_index++;
			ctx->ptr++;
			if (ctx->found_key) ctx->len++;
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
	// if there is an error.

	do {
		ctx->ptr++;
		if (ctx->found_key) {
			ctx->len++;
		}
	} while (*ctx->ptr && *ctx->ptr != '\"');

	if (!*ctx->ptr) {
		JOSH_ERROR(ctx, JOSH_ERROR_STRING_NOT_CLOSED);

		return true;
	}

	if (ctx->found_key) ctx->len++;

	return false;
}

bool josh_iter_number(struct josh_ctx_t *ctx) {
	// Iterate the context until the end of the current number. Return true
	// if there is an error.

	ctx->ptr++;
	if (ctx->found_key) ctx->len++;

	if (ctx->ptr[-1] == '-' && !isdigit(*ctx->ptr)) {
		JOSH_ERROR(ctx, JOSH_ERROR_NUMBER_INVALID);

		return true;
	}

	char c = *ctx->ptr;
	bool has_seen_period = false;

	while (c && (isdigit(c) || c == '.')) {
		if (c == '.') {
			if (has_seen_period) {
				JOSH_ERROR(ctx, JOSH_ERROR_NUMBER_INVALID);

				return true;
			}

			has_seen_period = true;
		}

		ctx->ptr++;
		if (ctx->found_key) ctx->len++;

		c = *ctx->ptr;
	}

	if (!josh_is_value_terminator(*ctx->ptr)) {
		JOSH_ERROR(ctx, JOSH_ERROR_NUMBER_INVALID);

		return true;
	}

	return false;
}

bool josh_iter_literal(struct josh_ctx_t *ctx) {
	// Iterate to the end of a JSON literal, such as null, true, or false. Return
	// true if there is an error.

	const char c = *ctx->ptr;

	if (c == 't') {
		if (strcmp(ctx->ptr, "true") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_TRUE);

			return true;
		}

		ctx->ptr += 4;
		if (ctx->found_key) ctx->len += 4;
	}
	else if (c == 'f') {
		if (strcmp(ctx->ptr, "false") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_FALSE);

			return true;
		}

		ctx->ptr += 5;
		if (ctx->found_key) ctx->len += 5;
	}
	else if (c == 'n') {
		if (strcmp(ctx->ptr, "null") < 0) {
			JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_NULL);

			return true;
		}

		ctx->ptr += 4;
		if (ctx->found_key) ctx->len += 4;
	}
	else {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_LITERAL);

		return true;
	}

	return false;
}

static inline void josh_iter_whitespace(struct josh_ctx_t *ctx) {
	// Iterate context to next non whitespace character. This function does not
	// update the `len` field in the context.

	while (isspace(*ctx->ptr)) {
		ctx->ptr++;

		if (ctx->found_key) ctx->len++;
	}
}
