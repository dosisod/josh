#include <stdio.h>

#include "josh.h"

#define TEST(x) puts("# test " x);

#define RED "\x1b[38;2;214;79;79m"
#define RESET "\x1b[0m"

#define INT_TO_STR(x) TO_STR(x)
#define TO_STR(x) #x

#define ASSERT(x) { \
	if (!(x)) { \
		puts( \
			RED \
			__FILE__":"INT_TO_STR(__LINE__) \
			": Expected `"TO_STR(x)"` to be truthy" \
			RESET \
		); \
		return 1; \
	} \
}

int main(void) {
	TEST("simple array access") {
		const char *json = "[1]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 1);
		ASSERT(out == json + 1);
	}

	TEST("leading whitespace is skipped") {
		const char *json = "   [1]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 1);
		ASSERT(out == json + 4);
	}

	TEST("error set if non-array value found for array index key") {
		const char *json = "{}";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_ARRAY);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("error set if JSON string is empty") {
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, "", "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EMPTY_VALUE);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("multi digit numbers return correct length") {
		const char *json = "[123]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 3);
		ASSERT(out == json + 1);
	}

	TEST("string is able to be parsed") {
		const char *json = "[\"abc\"]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 5);
		ASSERT(out == json + 1);
	}

	TEST("error set if JSON string is never closed") {
		static struct josh_ctx_t ctx;
		const char *json = "[\"abc";

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_STRING_NOT_CLOSED);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 6);
		ASSERT(ctx.offset == 5);
	}

	TEST("error set if JSON number is invalid") {
		static struct josh_ctx_t ctx;
		const char *json = "[1x]";

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_NUMBER_INVALID);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 3);
		ASSERT(ctx.offset == 2);
	}

	TEST("parse JSON true literal") {
		const char *json = "[true]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 4);
		ASSERT(out == json + 1);
	}

	TEST("parse JSON false literal") {
		const char *json = "[false]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 5);
		ASSERT(out == json + 1);
	}

	TEST("parse JSON null literal") {
		const char *json = "[null]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 4);
		ASSERT(out == json + 1);
	}

	TEST("set error when parsing unknown JSON literal") {
		const char *json = "[xyz]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_LITERAL);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("parse negative number") {
		const char *json = "[-123]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 4);
		ASSERT(out == json + 1);
	}

	TEST("set error when number contains multiple periods") {
		const char *json = "[1.2.3]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_NUMBER_INVALID);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 5);
		ASSERT(ctx.offset == 4);
	}

	TEST("set error when decimal doesnt have leading digit") {
		const char *json = "[-.1]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_NUMBER_INVALID);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 3);
		ASSERT(ctx.offset == 2);
	}

	TEST("set error when key number is invalid") {
		const char *json = "[123]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[xyz]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_KEY_NUMBER_INVALID);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("set error when array index is not found") {
		const char *json = "[]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_ARRAY_INDEX_NOT_FOUND);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("set error when array index is not found") {
		const char *json = "[]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_ARRAY_INDEX_NOT_FOUND);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("parse nth key from array") {
		const char *json = "[1, 2, 3]";
		static struct josh_ctx_t ctx;

		const char * out = josh_extract(&ctx, json, "[2]");

		ASSERT(ctx.len == 1);
		ASSERT(out == json + 7);
	}

	TEST("parse nested array") {
		const char *json = "[[1, 2, 3]]";
		static struct josh_ctx_t ctx;
		memset(&ctx, 0, sizeof ctx);

		const char * out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 9);
		ASSERT(out == json + 1);
	}
}
