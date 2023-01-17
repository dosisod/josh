#include <ctype.h>

enum josh_error {
	JOSH_EXPECTED_ARRAY,
};

struct josh_ctx_t {
	enum josh_error error_id;
	unsigned line;
	unsigned offset;
	unsigned column;
};

const char *josh_extract(struct josh_ctx_t *ctx, const char *json, const char *key, size_t *len) {
	(void)ctx;
	(void)key;

	const char *c = json;

	for (;;) {
		if (!isspace(*c)) {
			break;
		}
		c++;
	}

	if (*c != '[') {
		ctx->error_id = JOSH_EXPECTED_ARRAY;
		ctx->line = 1;
		ctx->column = 1;
		ctx->offset = (unsigned)(c - json);

		return NULL;
	}

	*len = 1;
	return c + 1;
}
