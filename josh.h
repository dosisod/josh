#include <ctype.h>
#include <stdbool.h>

enum josh_error {
	JOSH_ERROR_EXPECTED_ARRAY,
	JOSH_ERROR_EMPTY_VALUE,
	JOSH_ERROR_STRING_NOT_CLOSED,
	JOSH_ERROR_NUMBER_INVALID
};

struct josh_ctx_t {
	enum josh_error error_id;
	unsigned line;
	unsigned offset;
	unsigned column;
};

bool josh_is_value_terminator(char c) {
	return c == ',' || c == ']' || c == '}';
}

const char *josh_extract(struct josh_ctx_t *ctx, const char *json, const char *key, size_t *len) {
	(void)ctx;
	(void)key;

	const char *c = json;

	if (!*c) {
		ctx->error_id = JOSH_ERROR_EMPTY_VALUE;
		ctx->line = 1;
		ctx->column = 1;
		ctx->offset = 0;

		return NULL;
	}

	for (;;) {
		if (!isspace(*c)) {
			break;
		}
		c++;
	}

	if (*c != '[') {
		ctx->error_id = JOSH_ERROR_EXPECTED_ARRAY;
		ctx->line = 1;
		ctx->column = 1;
		ctx->offset = (unsigned)(c - json);

		return NULL;
	}

	c++;

	while (isspace(*c)) {
		c++;
	}

	const char *value_pos = c;

	if (*c == '\"') {
		do {
			c++;
			*len += 1;
		} while (*c && *c != '\"');

		if (!*c) {
			ctx->error_id = JOSH_ERROR_STRING_NOT_CLOSED;
			ctx->line = 1;
			ctx->column = (unsigned)(c - json + 1);
			ctx->offset = (unsigned)(c - json);
			*len = 0;

			return NULL;
		}

		*len += 1;
	}

	if (isdigit(*c)) {
		do {
			c++;
			*len += 1;
		} while (*c && isdigit(*c));

		if (!josh_is_value_terminator(*c)) {
			ctx->error_id = JOSH_ERROR_NUMBER_INVALID;
			ctx->line = 1;
			ctx->column = (unsigned)(c - json + 1);
			ctx->offset = (unsigned)(c - json);
			*len = 0;

			return NULL;
		}
	}

	return value_pos;
}
