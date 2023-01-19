#include <ctype.h>
#include <stdbool.h>

enum josh_error {
	JOSH_ERROR_EXPECTED_ARRAY,
	JOSH_ERROR_EMPTY_VALUE,
	JOSH_ERROR_STRING_NOT_CLOSED,
	JOSH_ERROR_NUMBER_INVALID
};

struct josh_ctx_t {
	const char *start;
	const char *ptr;
	size_t len;
	enum josh_error error_id;
	unsigned line;
	unsigned offset;
	unsigned column;
};

static bool inline josh_is_value_terminator(char c) {
	return c == ',' || c == ']' || c == '}';
}

bool josh_iter_string(struct josh_ctx_t *ctx);
bool josh_iter_number(struct josh_ctx_t *ctx);
static void josh_iter_whitespace(struct josh_ctx_t *ctx);
static void inline josh_skip_whitespace(struct josh_ctx_t *ctx);

#define JOSH_ERROR(ctx, id) \
	(ctx)->error_id = (id); \
	(ctx)->line = 1; \
	(ctx)->column = (unsigned)((ctx)->ptr -(ctx)->start + 1); \
	(ctx)->offset = (unsigned)((ctx)->ptr - (ctx)->start); \
	(ctx)->len = 0;

const char *josh_extract(struct josh_ctx_t *ctx, const char *json, const char *key) {
	(void)key;

	ctx->ptr = json;
	ctx->start = json;

	if (!*ctx->ptr) {
		JOSH_ERROR(ctx, JOSH_ERROR_EMPTY_VALUE);

		return NULL;
	}

	josh_iter_whitespace(ctx);

	if (*ctx->ptr != '[') {
		JOSH_ERROR(ctx, JOSH_ERROR_EXPECTED_ARRAY);

		return NULL;
	}

	ctx->ptr++;
	josh_iter_whitespace(ctx);

	const char *value_pos = ctx->ptr;

	if (*ctx->ptr == '\"') {
		if (josh_iter_string(ctx)) return NULL;
	}
	else if (isdigit(*ctx->ptr)) {
		if (josh_iter_number(ctx)) return NULL;
	}

	return value_pos;
}

bool josh_iter_string(struct josh_ctx_t *ctx) {
	// Iterate the context until the end of the current string. Return false
	// if there is an error.

	do {
		ctx->ptr++;
		ctx->len++;
	} while (*ctx->ptr && *ctx->ptr != '\"');

	if (!*ctx->ptr) {
		JOSH_ERROR(ctx, JOSH_ERROR_STRING_NOT_CLOSED);

		return true;
	}

	ctx->len++;

	return false;
}

bool josh_iter_number(struct josh_ctx_t *ctx) {
	// Iterate the context until the end of the current number. Return false
	// if there is an error.

	do {
		ctx->ptr++;
		ctx->len++;
	} while (*ctx->ptr && isdigit(*ctx->ptr));

	if (!josh_is_value_terminator(*ctx->ptr)) {
		JOSH_ERROR(ctx, JOSH_ERROR_NUMBER_INVALID);

		return true;
	}

	return false;
}

static void inline josh_iter_whitespace(struct josh_ctx_t *ctx) {
	// Iterate context to next non whitespace character. This function does not
	// update the `len` field in the context.

	while (isspace(*ctx->ptr)) ctx->ptr++;
}
