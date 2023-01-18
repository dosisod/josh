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

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "[0]", &len);

		ASSERT(len == 1);
		ASSERT(out == json + 1);
	}

	TEST("leading whitespace is skipped") {
		const char *json = "   [1]";
		static struct josh_ctx_t ctx;

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "[0]", &len);

		ASSERT(len == 1);
		ASSERT(out == json + 4);
	}

	TEST("error set if non-array value found for array index key") {
		const char *json = "{}";
		static struct josh_ctx_t ctx;

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "[0]", &len);

		ASSERT(!out);
		ASSERT(!len);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_ARRAY);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("error set if JSON string is empty") {
		static struct josh_ctx_t ctx;

		size_t len = 0;
		const char * out = josh_extract(&ctx, "", "[0]", &len);

		ASSERT(!out);
		ASSERT(!len);
		ASSERT(ctx.error_id == JOSH_ERROR_EMPTY_VALUE);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("multi digit numbers return correct length") {
		const char *json = "[123]";
		static struct josh_ctx_t ctx;

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "[0]", &len);

		ASSERT(len == 3);
		ASSERT(out == json + 1);
	}

	TEST("string is able to be parsed") {
		const char *json = "[\"abc\"]";
		static struct josh_ctx_t ctx;

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "[0]", &len);

		ASSERT(len == 5);
		ASSERT(out == json + 1);
	}

	TEST("error set if JSON string is never closed") {
		static struct josh_ctx_t ctx;
		const char *json = "[\"abc";

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "[0]", &len);

		ASSERT(!out);
		ASSERT(!len);
		ASSERT(ctx.error_id == JOSH_ERROR_STRING_NOT_CLOSED);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 6);
		ASSERT(ctx.offset == 5);
	}

	TEST("error set if JSON number is invalid") {
		static struct josh_ctx_t ctx;
		const char *json = "[1x]";

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "[0]", &len);

		ASSERT(!out);
		ASSERT(!len);
		ASSERT(ctx.error_id == JOSH_ERROR_NUMBER_INVALID);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 3);
		ASSERT(ctx.offset == 2);
	}
}
