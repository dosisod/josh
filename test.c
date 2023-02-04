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

static struct josh_ctx_t ctx;

int main(void) {
	TEST("simple array access") {
		const char *json = "[1]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 1);
		ASSERT(out == json + 1);
	}

	TEST("leading whitespace is skipped") {
		const char *json = "   [1]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 1);
		ASSERT(out == json + 4);
	}

	TEST("error set if non-array value found for array index key") {
		const char *json = "123";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_ARRAY);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("error set if JSON string is empty") {
		const char *out = josh_extract(&ctx, "", "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EMPTY_VALUE);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("multi digit numbers return correct length") {
		const char *json = "[123]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 3);
		ASSERT(out == json + 1);
	}

	TEST("string is able to be parsed") {
		const char *json = "[\"abc\"]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 5);
		ASSERT(out == json + 1);
	}

	TEST("error set if JSON string is never closed") {
		const char *json = "[\"abc";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_STRING_NOT_CLOSED);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 6);
		ASSERT(ctx.offset == 5);
	}

	TEST("error set if JSON number is invalid") {
		const char *json = "[1x]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_DIGIT_EXPECTED);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 3);
		ASSERT(ctx.offset == 2);
	}

	TEST("parse JSON true literal") {
		const char *json = "[true]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 4);
		ASSERT(out == json + 1);
	}

	TEST("parse JSON false literal") {
		const char *json = "[false]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 5);
		ASSERT(out == json + 1);
	}

	TEST("parse JSON null literal") {
		const char *json = "[null]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 4);
		ASSERT(out == json + 1);
	}

	TEST("set error when parsing unknown JSON literal") {
		const char *json = "[xyz]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_LITERAL);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("parse negative number") {
		const char *json = "[-123]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 4);
		ASSERT(out == json + 1);
	}

	TEST("set error when number contains multiple periods") {
		const char *json = "[1.2.3]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_DIGIT_EXPECTED);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 5);
		ASSERT(ctx.offset == 4);
	}

	TEST("set error when decimal doesnt have leading digit") {
		const char *json = "[-.1]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_DIGIT_EXPECTED);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 3);
		ASSERT(ctx.offset == 2);
	}

	TEST("set error when key number is invalid") {
		const char *json = "[123]";

		const char *out = josh_extract(&ctx, json, "[123xyz]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_KEY_NUMBER_INVALID);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("set error when array index is not found") {
		const char *json = "[]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_ARRAY_INDEX_NOT_FOUND);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("set error when array index is not found") {
		const char *json = "[]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_ARRAY_INDEX_NOT_FOUND);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("parse nth key from array") {
		const char *json = "[1, 2, 3]";

		const char *out = josh_extract(&ctx, json, "[2]");

		ASSERT(!ctx.error_id);
		ASSERT(ctx.key_count == 1);
		ASSERT(ctx.keys[0].type == JOSH_KEY_TYPE_ARRAY);
		ASSERT(ctx.keys[0].key.num == 2);
		ASSERT(ctx.len == 1);
		ASSERT(out == json + 7);
	}

	TEST("parse nested array") {
		const char *json = "[[1, 2, 3]]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 9);
		ASSERT(out == json + 1);
	}

	TEST("parse string with escape chars") {
		/* json = ["\" \\ \/ \b \f \n \r \t \u1234"] */
		const char *json = "[\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t \\u1234\"]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 32);
		ASSERT(out == json + 1);
	}

	TEST("set error when invalid escape char is found") {
		const char *json = "[\"\\z\"]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_INVALID_ESCAPE_CODE);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 4);
		ASSERT(ctx.offset == 3);
	}

	TEST("set error when invalid unicode escape is found") {
		const char *json = "[\"\\u123x\"]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_INVALID_UNICODE_ESCAPE_CODE);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 8);
		ASSERT(ctx.offset == 7);
	}

	TEST("set error for invalid object key") {
		const char *json = "";

		const char *out = josh_extract(&ctx, json, ".");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_INVALID_KEY_OBJECT);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("parse object key") {
		memset(&ctx, 0, sizeof ctx);
		const bool ok = josh_parse_key(&ctx, ".abc");

		ASSERT(ok);
		ASSERT(!ctx.error_id);
		ASSERT(ctx.key_count == 1);
		ASSERT(ctx.keys[0].type == JOSH_KEY_TYPE_OBJECT);
		ASSERT(ctx.keys[0].key.str);
		ASSERT(strcmp(ctx.keys[0].key.str, "abc") == 0);
	}

	TEST("parse empty object") {
		const char *json = "[{}]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 2);
		ASSERT(out == json + 1);
	}

	TEST("parse object with key") {
		const char *json = "[{\"abc\": 123}]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 12);
		ASSERT(out == json + 1);
	}

	TEST("parse object with multiple keys") {
		const char *json = "[{\"abc\": 123, \"def\": 456}]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 24);
		ASSERT(out == json + 1);
	}

	TEST("parse object using object key") {
		const char *json = "{\"abc\": 123, \"def\": 456}";

		const char *out = josh_extract(&ctx, json, ".def");

		ASSERT(ctx.len == 3);
		ASSERT(out == json + 20);
	}

	TEST("set error for object key missing colon") {
		const char *json = "{\"abc\" 123}";

		const char *out = josh_extract(&ctx, json, ".abc");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_COLON);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 8);
		ASSERT(ctx.offset == 7);
	}

	TEST("set error for object key using non-string key") {
		const char *json = "{123}";

		const char *out = josh_extract(&ctx, json, ".abc");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_STRING);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("set error for non existent object key") {
		const char *json = "{}";

		const char *out = josh_extract(&ctx, json, ".abc");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_OBJECT_KEY_NOT_FOUND);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("parse numbers with exponents") {
		const char *json = "[[1e3, 1E3, 1.2e3, 1.2E3, 1e+3, 1e-3]]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(ctx.len == 36);
		ASSERT(out == json + 1);
	}

	TEST("set error for number with leading zero") {
		const char *json = "[0123]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_NO_LEADING_ZERO);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 2);
		ASSERT(ctx.offset == 1);
	}

	TEST("set error with column and line info set") {
		const char *json = "[\n  x]";

		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.line == 2);
		ASSERT(ctx.column == 3);
		ASSERT(ctx.offset == 4);
	}

	TEST("parse string in array as dictionary key") {
		const char *json = "{\"abc\": 123}";

		const char *out = josh_extract(&ctx, json, "[\"abc\"]");

		ASSERT(ctx.len == 3);
		ASSERT(out == json + 8);
	}

	TEST("error set if using dot notation with invalid JS literal") {
		const char *out = josh_extract(&ctx, "{}", ".not_valid!");

		ASSERT(!out);
		ASSERT(!ctx.len);
		ASSERT(ctx.error_id == JOSH_ERROR_INVALID_KEY_OBJECT);
		ASSERT(ctx.line == 1);
		ASSERT(ctx.column == 1);
		ASSERT(ctx.offset == 0);
	}

	TEST("parse nested key") {
		memset(&ctx, 0, sizeof ctx);

		const bool ok = josh_parse_key(&ctx, "[1][2]");

		ASSERT(ok);
		ASSERT(!ctx.error_id);
		ASSERT(ctx.key_count == 2);
		ASSERT(ctx.keys[0].type == JOSH_KEY_TYPE_ARRAY);
		ASSERT(ctx.keys[0].key.num == 1);
		ASSERT(ctx.keys[1].type == JOSH_KEY_TYPE_ARRAY);
		ASSERT(ctx.keys[1].key.num == 2);
	}

	TEST("set error for key missing value") {
		memset(&ctx, 0, sizeof ctx);

		const bool ok = josh_parse_key(&ctx, "[");

		ASSERT(!ok);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_KEY_VALUE);
	}

	TEST("set error for number key missing closing square bracket") {
		memset(&ctx, 0, sizeof ctx);

		const bool ok = josh_parse_key(&ctx, "[1");

		ASSERT(!ok);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_KEY_CLOSING_BRACKET);
	}

	TEST("set error for string key missing closing square bracket") {
		memset(&ctx, 0, sizeof ctx);

		const bool ok = josh_parse_key(&ctx, "[\"abc\"");

		ASSERT(!ok);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_KEY_CLOSING_BRACKET);
	}

	TEST("set error for string key missing closing quote") {
		memset(&ctx, 0, sizeof ctx);

		const bool ok = josh_parse_key(&ctx, "[\"abc");

		ASSERT(!ok);
		ASSERT(ctx.error_id == JOSH_ERROR_EXPECTED_KEY_CLOSING_QUOTE);
	}

	TEST("extract nested value from array") {
		const char *json = "[[1, 2, 3]]";

		const char *out = josh_extract(&ctx, json, "[0][0]");

		ASSERT(ctx.len == 1);
		ASSERT(out == json + 2);
	}

	TEST("extract nested value from object") {
		const char *json = "{\"a\": {\"b\": 1, \"c\": 2}}";

		const char *out = josh_extract(&ctx, json, ".a.b");

		ASSERT(ctx.len == 1);
		ASSERT(out == json + 12);
	}

	TEST("parse key with nested dot notation") {
		memset(&ctx, 0, sizeof ctx);
		const bool ok = josh_parse_key(&ctx, ".a.b");

		ASSERT(ok);
		ASSERT(!ctx.error_id);
		ASSERT(ctx.key_count == 2);
		ASSERT(ctx.keys[0].type == JOSH_KEY_TYPE_OBJECT);
		ASSERT(ctx.keys[0].key.str);
		// TODO: use strcmp once strings are copied to context
		ASSERT(strncmp(ctx.keys[0].key.str, "a", 1) == 0);
		ASSERT(ctx.keys[1].type == JOSH_KEY_TYPE_OBJECT);
		ASSERT(ctx.keys[1].key.str);
		ASSERT(strncmp(ctx.keys[1].key.str, "b", 1) == 0);
	}

	TEST("set error if key max depth is reached") {
		memset(&ctx, 0, sizeof ctx);
		const bool ok = josh_parse_key(&ctx, "[1][2][3][4][5][6][7][8][9][10][11][12][13][14][15][16][17]");

		ASSERT(!ok);
		ASSERT(ctx.error_id == JOSH_ERROR_KEY_MAX_DEPTH_REACHED);
	}

	TEST("allocate memory increments counter") {
		memset(&ctx, 0, sizeof ctx);

		void *memory = josh_malloc(&ctx, 1);

		ASSERT(memory);
		ASSERT(ctx.allocated == 1);

		void *memory2 = josh_malloc(&ctx, 2);

		ASSERT(memory2);
		ASSERT(memory2 == memory + 1);
		ASSERT(ctx.allocated == 3);
	}

	TEST("set error if allocating more them max memory") {
		memset(&ctx, 0, sizeof ctx);

		void *mem = josh_malloc(&ctx, JOSH_CONFIG_MAX_MEMORY + 1);

		ASSERT(!mem);
		ASSERT(ctx.error_id == JOSH_ERROR_OUT_OF_MEMORY);
	}

	TEST("string keys are properly compared") {
		const char *json = "{\"a\": 1}";

		const char *out = josh_extract(&ctx, json, "[\"abc\"]");

		ASSERT(!out);

		out = josh_extract(&ctx, json, ".abc");

		ASSERT(!out);
	}

	TEST("all levels of key must match") {
		const char *json = "[[1]]";
		const char *out = josh_extract(&ctx, json, "[999][0]");

		ASSERT(!out);

		json = "{\"a\": {\"b\": 1}}";
		out = josh_extract(&ctx, json, ".x.b");

		ASSERT(!out);
	}

	TEST("duplicate object keys doesnt match incorrect element") {
		const char *json = "{\"a\": null, \"a\": [1]}";
		const char *out = josh_extract(&ctx, json, ".a[0]");

		ASSERT(out == json + 18);
		ASSERT(ctx.len == 1);
	}

	TEST("zero is a valid number") {
		const char *json = "[0]";
		const char *out = josh_extract(&ctx, json, "[0]");

		ASSERT(out == json + 1);
		ASSERT(ctx.len == 1);
	}
}
