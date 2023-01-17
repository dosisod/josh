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
		const char * out = josh_extract(&ctx, json, "0", &len);

		ASSERT(len == 1);
		ASSERT(out == json + 1);
	}

	TEST("leading whitespace is skipped") {
		const char *json = "   [1]";
		static struct josh_ctx_t ctx;

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "0", &len);

		ASSERT(len == 1);
		ASSERT(out == json + 4);
	}

	TEST("error set if non-array value found for array index key") {
		const char *json = "{}";
		static struct josh_ctx_t ctx;

		size_t len = 0;
		const char * out = josh_extract(&ctx, json, "0", &len);

		ASSERT(!out);
		ASSERT(!len);
		ASSERT(ctx.error_id == JOSH_EXPECTED_ARRAY);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}
}
