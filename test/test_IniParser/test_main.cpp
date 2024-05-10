/**
 * @file test_main.cpp
 * @author Daniel Starke
 * @date 2024-04-21
 * @version 2024-05-05
 *
 * Copyright (c) 2024 Daniel Starke
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <unity.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "IniParser.hpp"


#ifndef ARRAY_SIZE
/**
 * Returns the size of a local array if known at compile time.
 *
 * @param x - array to check
 */
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof(*(x)))
#endif /* ARRAY_SIZE */



/**
 * Used by IniParser to iterate over all characters in a string.
 */
class StringProvider {
private:
	const char * ptr;
public:
	/**
	 * Constructor.
	 *
	 * @param[in] str - pointer to the input string
	 */
	explicit inline StringProvider(const char * str):
		ptr(str)
	{}
	/**
	 * Function operator which provides a character at a time.
	 *
	 * @return next character or -1 on end of input
	 */
	int operator() () {
		if (ptr == NULL || *ptr == 0) {
			return -1;
		}
		return int(*ptr++);
	}
};


namespace variants {
struct StrU32Pair {
	const char * str;
	uint32_t num;
};
struct StrI32Pair {
	const char * str;
	int32_t num;
};

static const char * strings[][2] = {
	/* test, expected */
		{"",             ""},
		{"a",            "a"},
		{"ab",           "ab"},
		{"abc",          "abc"},
		{"a c",          "a c"},
		{"a\tc",         "a\tc"},
		{"\"\"",         ""},
		{"\"a\"",        "a"},
		{"\"ab\"",       "ab"},
		{"\"a c\"",      "a c"},
		{"\"a#c\"",      "a#c"},
		{"\"a\tc\"",     "a\tc"},
		{"\" a c\"",     " a c"},
		{"\"\ta\tc\"",   "\ta\tc"},
		{"\" a c \"",    " a c "},
		{"\"\ta\tc\t\"", "\ta\tc\t"},
		{"\"a'c\"",      "a'c"},
		{"''",           ""},
		{"'a'",          "a"},
		{"'ab'",         "ab"},
		{"'a c'",        "a c"},
		{"'a#c'",        "a#c"},
		{"'a\tc'",       "a\tc"},
		{"' a c'",       " a c"},
		{"'\ta\tc'",     "\ta\tc"},
		{"' a c '",      " a c "},
		{"'\ta\tc\t'",   "\ta\tc\t"},
		{"'a\"c'",       "a\"c"}
};

static const StrU32Pair decNumbers[] = {
	{"0",                   0UL},
	{"00",                  0UL},
	{"000000000000000000",  0UL},
	{"1",                   1UL},
	{"10",                  10UL},
	{"100",                 100UL},
	{"1000",                1000UL},
	{"10000",               10000UL},
	{"100000",              100000UL},
	{"1000000",             1000000UL},
	{"10000000",            10000000UL},
	{"100000000",           100000000UL},
	{"1000000000",          1000000000UL},
	{"2147483647",          2147483647UL},
	{"4294967295",          4294967295UL},
	{"0000000004294967295", 4294967295UL}
};

static const StrU32Pair hexNumbers[] = {
	{"0",                   0x0UL},
	{"00",                  0x0UL},
	{"000000000000000000",  0x0UL},
	{"1",                   0x1UL},
	{"9",                   0x9UL},
	{"a",                   0xAUL},
	{"A",                   0xAUL},
	{"f",                   0xFUL},
	{"F",                   0xFUL},
	{"FF",                  0xFFUL},
	{"FFF",                 0xFFFUL},
	{"FFFF",                0xFFFFUL},
	{"FFFFF",               0xFFFFFUL},
	{"FFFFFF",              0xFFFFFFUL},
	{"FFFFFFF",             0xFFFFFFFUL},
	{"7FFFFFFF",            0x7FFFFFFFUL},
	{"FFFFFFFF",            0xFFFFFFFFUL},
	{"abcdef",              0xABCDEFUL},
	{"ABCDEF",              0xABCDEFUL},
	{"00000000000FFFFFFFF", 0xFFFFFFFFUL}
};

static const StrI32Pair decSignedNumbers[] = {
	{"0",                   0L},
	{"00",                  0L},
	{"000000000000000000",  0L},
	{"1",                   1L},
	{"10",                  10L},
	{"100",                 100L},
	{"1000",                1000L},
	{"10000",               10000L},
	{"100000",              100000L},
	{"1000000",             1000000L},
	{"10000000",            10000000L},
	{"100000000",           100000000L},
	{"1000000000",          1000000000L},
	{"2147483647",          2147483647L},
	{"0000000002147483647", 2147483647L}
};

static const StrI32Pair hexSignedNumbers[] = {
	{"0",                   0x0L},
	{"00",                  0x0L},
	{"000000000000000000",  0x0L},
	{"1",                   0x1L},
	{"9",                   0x9L},
	{"a",                   0xAL},
	{"A",                   0xAL},
	{"f",                   0xFL},
	{"F",                   0xFL},
	{"FF",                  0xFFL},
	{"FFF",                 0xFFFL},
	{"FFFF",                0xFFFFL},
	{"FFFFF",               0xFFFFFL},
	{"FFFFFF",              0xFFFFFFL},
	{"FFFFFFF",             0xFFFFFFFL},
	{"7FFFFFFF",            0x7FFFFFFFL},
	{"abcdef",              0xABCDEFL},
	{"ABCDEF",              0xABCDEFL},
	{"000000000007FFFFFFF", 0x7FFFFFFFL}
};

static const char * spaces[] = {
	"",
	" ",
	" ",
	"\t",
	"\t\t",
	" \t",
	"\t "
};

static const char * comments[] = {
	"",
	"#",
	"#comment",
	"# comment",
	"####",
	"# a = c",
	"#[a]"
};

static const char * eois[] = {
	"",
	"\n",
	"\n\r",
	"\r",
	"\r\n",
	"\n\n",
	"\r\r"
};
}


/**
 * Maps the given key/group to a target variable.
 * This function is only a dummy to ignore all values.
 */
static bool ignoreAllValues(IniParser::Context & /* ctx */) {
	return true;
}


/**
 * Function called by Unity before test case execution.
 */
void setUp(void) {
	/* test setup */
}


/**
 * Function called by Unity after test case execution.
 */
void tearDown(void) {
	/* test clean-up */
}


/**
 * Test if `IniParser` handles semantically empty configurations correctly.
 */
void test_empty_ini() {
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString(" ", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("# only comments", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("# only comments\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("# only comments\r", ignoreAllValues));
}


/**
 * Test if `IniParser` reports invalid characters correctly.
 */
void test_invalid_characters() {
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("\x03", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[8A]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[A-]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[A:]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[A,]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("(A)", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("; not a comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("8A = b", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A-b = b", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A\x03 = b", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A = \x03", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A = b\x03", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A : b", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A = 'b\n'", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A = 'b\r'", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A = 'b'z", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("A = '\x7F'", ignoreAllValues));
}


/**
 * Test if `IniParser` handles various valid formats correctly.
 */
void test_invalid_formats() {
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[group", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[ group]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[\tgroup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[\rgroup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[\ngroup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[gr oup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[gr\toup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[gr\roup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[gr\noup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[group ]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[group\t]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[group\r]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[group\n]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("k ey = value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("k\tey = value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("k\rey = value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("k\ney = value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key \r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key \n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key #comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key\t\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key\t\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key\t#comment\n", ignoreAllValues));
}


/**
 * Test if `IniParser` handles various valid formats correctly.
 */
void test_valid_formats() {
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[gr_oup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[gr.oup]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[Group]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[Group]\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[Group]\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[Group]\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[Group]\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[GROUP]", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[GROUP]\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[GROUP]\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[GROUP]\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[GROUP]\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("k_ey =", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("k.ey =", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = ", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = #comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = #comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = #comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = #comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = #comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("Key = Value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("KEY = VALUE#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key= value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key =value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key  =value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t =value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key \t=value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = val ue#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key=value#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key\t=\tvalue#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = \"val'ue\"#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val\"ue'#comment\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'\r\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'#comment", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'#comment\n", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'#comment\n\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'#comment\r", ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'val#ue'#comment\r\n", ignoreAllValues));
}


/**
 * Test if `IniParser` handles string values correctly.
 */
void test_string_values() {
	using namespace variants;
	char str[8];
	char buf[64];
	bool type = false;
	const auto mapString = [&] (IniParser::Context & ctx) -> bool {
		if (ctx.group == "group" && ctx.key == "key") {
			if ( type ) {
				ctx.mapString(str);
			} else {
				ctx.mapString(str, sizeof(str));
			}
			type = !type;
		}
		return true;
	};
	for (size_t i = 0; i < ARRAY_SIZE(eois); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(comments); j++) {
			for (size_t k = 0; k < ARRAY_SIZE(spaces); k++) {
				for (size_t l = 0; l < ARRAY_SIZE(spaces); l++) {
					for (size_t m = 0; m < ARRAY_SIZE(strings); m++) {
						snprintf(buf, sizeof(buf), "[group]\nkey =%s%s%s%s%s", spaces[l], strings[m][0], spaces[k], comments[j], eois[i]);
						memset(str, 0, sizeof(str));
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapString), buf);
						TEST_ASSERT_EQUAL_STRING_MESSAGE(strings[m][1], str, buf);
						memset(str, 0xFF, sizeof(str));
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapString), buf);
						TEST_ASSERT_EQUAL_STRING_MESSAGE(strings[m][1], str, buf);
					}
				}
			}
		}
	}
	/* invalid characters */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 'abc'd", mapString));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 'ab\rc'", mapString));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 'ab\nc'", mapString));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 'ab", mapString));
}


/**
 * Test if `IniParser` handles unsigned number values correctly.
 */
void test_unsigned_number_values() {
	using namespace variants;
	uint32_t num;
	char buf[64];
	uint32_t minNum = 0;
	uint32_t maxNum = 0xFFFFFFFFUL;
	const auto mapNumber = [&] (IniParser::Context & ctx) -> bool {
		if (ctx.group == "group" && ctx.key == "key") {
			ctx.mapNumber(num, minNum, maxNum);
		}
		return true;
	};
	const auto mapHexNumber = [&] (IniParser::Context & ctx) -> bool {
		if (ctx.group == "group" && ctx.key == "key") {
			ctx.mapHexNumber(num, minNum, maxNum);
		}
		return true;
	};
	/* decimal numbers */
	for (size_t i = 0; i < ARRAY_SIZE(eois); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(comments); j++) {
			for (size_t k = 0; k < ARRAY_SIZE(spaces); k++) {
				for (size_t l = 0; l < ARRAY_SIZE(spaces); l++) {
					for (size_t m = 0; m < ARRAY_SIZE(decNumbers); m++) {
						snprintf(buf, sizeof(buf), "[group]\nkey =%s%s%s%s%s", spaces[l], decNumbers[m].str, spaces[k], comments[j], eois[i]);
						num = 0;
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
						TEST_ASSERT_EQUAL_UINT32_MESSAGE(decNumbers[m].num, num, buf);
						num = 0xFFFFFFFFUL;
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
						TEST_ASSERT_EQUAL_UINT32_MESSAGE(decNumbers[m].num, num, buf);
					}
				}
			}
		}
	}
	/* hexadecimal numbers with 0x prefix */
	for (size_t i = 0; i < ARRAY_SIZE(eois); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(comments); j++) {
			for (size_t k = 0; k < ARRAY_SIZE(spaces); k++) {
				for (size_t l = 0; l < ARRAY_SIZE(spaces); l++) {
					for (size_t m = 0; m < ARRAY_SIZE(hexNumbers); m++) {
						snprintf(buf, sizeof(buf), "[group]\nkey =%s0x%s%s%s%s", spaces[l], hexNumbers[m].str, spaces[k], comments[j], eois[i]);
						num = 0;
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
						TEST_ASSERT_EQUAL_UINT32_MESSAGE(hexNumbers[m].num, num, buf);
						num = 0xFFFFFFFFUL;
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
						TEST_ASSERT_EQUAL_UINT32_MESSAGE(hexNumbers[m].num, num, buf);
					}
				}
			}
		}
	}
	/* hexadecimal numbers without 0x prefix */
	for (size_t i = 0; i < ARRAY_SIZE(eois); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(comments); j++) {
			for (size_t k = 0; k < ARRAY_SIZE(spaces); k++) {
				for (size_t l = 0; l < ARRAY_SIZE(spaces); l++) {
					for (size_t m = 0; m < ARRAY_SIZE(hexNumbers); m++) {
						snprintf(buf, sizeof(buf), "[group]\nkey =%s%s%s%s%s", spaces[l], hexNumbers[m].str, spaces[k], comments[j], eois[i]);
						num = 0;
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapHexNumber), buf);
						TEST_ASSERT_EQUAL_UINT32_MESSAGE(hexNumbers[m].num, num, buf);
						num = 0xFFFFFFFFUL;
						TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapHexNumber), buf);
						TEST_ASSERT_EQUAL_UINT32_MESSAGE(hexNumbers[m].num, num, buf);
					}
				}
			}
		}
	}
	/* number overflow */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 4294967296", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0x100000000", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 100000000", mapHexNumber));
	/* number range */
	maxNum = 11;
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 12", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0xC", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = C", mapHexNumber));
	minNum = 11;
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 10", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0xA", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = A", mapHexNumber));
	/* invalid numbers */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 00xB", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -11", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -0xB", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -B", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 10G", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0xBG", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = BG", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 1 1", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0x B", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0 B", mapHexNumber));
	/* empty value */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = ", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = ", mapHexNumber));
}


/**
 * Test if `IniParser` handles signed number values correctly.
 */
void test_signed_number_values() {
	using namespace variants;
	int32_t num;
	char buf[64];
	int32_t minNum = -0x80000000L;
	int32_t maxNum = 0x7FFFFFFFL;
	const auto mapNumber = [&] (IniParser::Context & ctx) -> bool {
		if (ctx.group == "group" && ctx.key == "key") {
			ctx.mapNumber(num, minNum, maxNum);
		}
		return true;
	};
	const auto mapHexNumber = [&] (IniParser::Context & ctx) -> bool {
		if (ctx.group == "group" && ctx.key == "key") {
			ctx.mapHexNumber(num, minNum, maxNum);
		}
		return true;
	};
	/* decimal numbers */
	for (size_t i = 0; i < ARRAY_SIZE(eois); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(comments); j++) {
			for (size_t k = 0; k < ARRAY_SIZE(spaces); k++) {
				for (size_t l = 0; l < ARRAY_SIZE(spaces); l++) {
					for (size_t m = 0; m < ARRAY_SIZE(decSignedNumbers); m++) {
						for (int32_t s = 1; s > -2; s -= 2) {
							snprintf(buf, sizeof(buf), "[group]\nkey =%s%s%s%s%s%s", spaces[l], (s < 0) ? "-" : "", decSignedNumbers[m].str, spaces[k], comments[j], eois[i]);
							num = 0;
							TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
							TEST_ASSERT_EQUAL_INT32_MESSAGE(s * decSignedNumbers[m].num, num, buf);
							num = -1;
							TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
							TEST_ASSERT_EQUAL_INT32_MESSAGE(s * decSignedNumbers[m].num, num, buf);
						}
					}
					snprintf(buf, sizeof(buf), "[group]\nkey =%s-2147483648%s%s%s", spaces[l], spaces[k], comments[j], eois[i]);
					num = 0;
					TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
					TEST_ASSERT_EQUAL_INT32_MESSAGE(-2147483648L, num, buf);
					num = -1;
					TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
					TEST_ASSERT_EQUAL_INT32_MESSAGE(-2147483648L, num, buf);
				}
			}
		}
	}
	/* hexadecimal numbers with 0x prefix */
	for (size_t i = 0; i < ARRAY_SIZE(eois); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(comments); j++) {
			for (size_t k = 0; k < ARRAY_SIZE(spaces); k++) {
				for (size_t l = 0; l < ARRAY_SIZE(spaces); l++) {
					for (size_t m = 0; m < ARRAY_SIZE(hexSignedNumbers); m++) {
						for (int32_t s = 1; s > -2; s -= 2) {
							snprintf(buf, sizeof(buf), "[group]\nkey =%s%s0x%s%s%s%s", spaces[l], (s < 0) ? "-" : "", hexSignedNumbers[m].str, spaces[k], comments[j], eois[i]);
							num = 0;
							TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
							TEST_ASSERT_EQUAL_INT32_MESSAGE(s * hexSignedNumbers[m].num, num, buf);
							num = -1;
							TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
							TEST_ASSERT_EQUAL_INT32_MESSAGE(s * hexSignedNumbers[m].num, num, buf);
						}
					}
					snprintf(buf, sizeof(buf), "[group]\nkey =%s-0x80000000%s%s%s", spaces[l], spaces[k], comments[j], eois[i]);
					num = 0;
					TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
					TEST_ASSERT_EQUAL_INT32_MESSAGE(-2147483648L, num, buf);
					num = -1;
					TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapNumber), buf);
					TEST_ASSERT_EQUAL_INT32_MESSAGE(-2147483648L, num, buf);
				}
			}
		}
	}
	/* hexadecimal numbers without 0x prefix */
	for (size_t i = 0; i < ARRAY_SIZE(eois); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(comments); j++) {
			for (size_t k = 0; k < ARRAY_SIZE(spaces); k++) {
				for (size_t l = 0; l < ARRAY_SIZE(spaces); l++) {
					for (size_t m = 0; m < ARRAY_SIZE(hexSignedNumbers); m++) {
						for (int32_t s = 1; s > -2; s -= 2) {
							snprintf(buf, sizeof(buf), "[group]\nkey =%s%s%s%s%s%s", spaces[l], (s < 0) ? "-" : "", hexSignedNumbers[m].str, spaces[k], comments[j], eois[i]);
							num = 0;
							TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapHexNumber), buf);
							TEST_ASSERT_EQUAL_INT32_MESSAGE(s * hexSignedNumbers[m].num, num, buf);
							num = -1;
							TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapHexNumber), buf);
							TEST_ASSERT_EQUAL_INT32_MESSAGE(s * hexSignedNumbers[m].num, num, buf);
						}
					}
					snprintf(buf, sizeof(buf), "[group]\nkey =%s-80000000%s%s%s", spaces[l], spaces[k], comments[j], eois[i]);
					num = 0;
					TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapHexNumber), buf);
					TEST_ASSERT_EQUAL_INT32_MESSAGE(-2147483648L, num, buf);
					num = -1;
					TEST_ASSERT_EQUAL_size_t_MESSAGE(0, IniParser::parseString(buf, mapHexNumber), buf);
					TEST_ASSERT_EQUAL_INT32_MESSAGE(-2147483648L, num, buf);
				}
			}
		}
	}
	/* number overflow */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 4294967296", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0x100000000", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 100000000", mapHexNumber));
	/* number range */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -2147483649", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 2147483648", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -0x80000001", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0x80000001", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -80000001", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 80000001", mapHexNumber));
	minNum = -11;
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -12", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -0xC", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -C", mapHexNumber));
	maxNum = -11;
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -10", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -0xA", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -A", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 10", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0xA", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = A", mapHexNumber));
	maxNum = 11;
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 12", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0xC", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = C", mapHexNumber));
	minNum = 11;
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 10", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0xA", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = A", mapHexNumber));
	/* invalid numbers */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = - 0", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = - 0", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -00xB", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -10G", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 10G", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -0xBG", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = 0xBG", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -BG", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = BG", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -1 1", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -0x B", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -0 B", mapHexNumber));
	/* empty value */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = ", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -", mapNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = ", mapHexNumber));
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = -", mapHexNumber));
}


/**
 * Test if `IniParser` handles size limits correctly.
 */
void test_size_limits() {
	char str[8];
	const auto mapString = [&] (IniParser::Context & ctx) -> bool {
		ctx.mapString(str);
		return true;
	};
	/* group string length limit */
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[g123456]", ignoreAllValues, 8));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[g1234567]", ignoreAllValues, 8));
	/* key string length limit */
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("k123456 =", ignoreAllValues, 8));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("k1234567 =", ignoreAllValues, 8));
	/* value string length limit */
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = v123456", mapString, 8));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key = v1234567", mapString, 8));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("key = 'v123456'", mapString, 8));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("key = 'v1234567'", mapString, 8));
}


/**
 * Test if `IniParser` handles data providers correctly.
 */
void test_data_function() {
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseFn(StringProvider("[gr"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[group]"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[gr_oup]"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[gr.oup]"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[group]\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[group]\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[Group]"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[Group]\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[Group]\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[GROUP]"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[GROUP]\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("[GROUP]\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key ="), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("k_ey ="), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("k.ey ="), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = "), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = #comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = #comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = #comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = value"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = value\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = value\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = value#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = value#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = value#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("Key = Value"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("Key = Value\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("Key = Value\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("Key = Value#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("Key = Value#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("Key = Value#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("KEY = VALUE"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("KEY = VALUE\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("KEY = VALUE\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("KEY = VALUE#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("KEY = VALUE#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("KEY = VALUE#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key= value"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key= value\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key= value\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key= value#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key= value#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key= value#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key =value"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key =value\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key =value\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key =value#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key =value#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key =value#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = val ue"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = val ue\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = val ue\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = val ue#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = val ue#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = val ue#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key=value"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key=value\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key=value\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key=value#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key=value#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key=value#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key\t=\tvalue"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key\t=\tvalue\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key\t=\tvalue\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key\t=\tvalue#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key\t=\tvalue#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key\t=\tvalue#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \"val'ue\""), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \"val'ue\"\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \"val'ue\"\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \"val'ue\"#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \"val'ue\"#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = \"val'ue\"#comment\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = 'val\"ue'"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = 'val\"ue'\n"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = 'val\"ue'\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = 'val\"ue'#comment"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = 'val\"ue'#comment\r"), ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseFn(StringProvider("key = 'val\"ue'#comment\n"), ignoreAllValues));
}


/**
 * Test if `IniParser` handles strings with size limit correctly.
 */
void test_sized_string_input() {
	/* heap allocated */
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[gr", 3, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]", 7, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]x", 7, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, IniParser::parseString("[group]x", 8, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]", 8, ignoreAllValues));
	/* stack allocated */
	TEST_ASSERT_EQUAL_size_t(1, iniParseString<16>("[gr", 3, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<16>("[group]", 7, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<16>("[group]x", 7, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, iniParseString<16>("[group]x", 8, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<16>("[group]", 8, ignoreAllValues));
}


/**
 * Test if `IniParser` handles special error cases correctly.
 */
void test_special_errors() {
	char str[8];
	const auto mapString = [&] (IniParser::Context & ctx) -> bool {
		ctx.mapString(str);
		return true;
	};
	const auto abort = [&] (IniParser::Context &) -> bool {
		return false;
	};
	const auto breakState = [&] (IniParser::Context & ctx) -> bool {
		reinterpret_cast<uint32_t &>(ctx) = 0;
		return true;
	};
	/* null character in handled string */
	{
		IniParser ini(mapString);
		const char * ptr = "[group]\nkey = abc";
		while (*ptr != 0) {
			TEST_ASSERT_TRUE(ini.parse(int(*ptr)));
			ptr++;
		}
		TEST_ASSERT_FALSE(ini.parse(0));
		TEST_ASSERT_EQUAL_size_t(2, ini.getLine());
	}
	/* null character in unhandled string */
	{
		IniParser ini(ignoreAllValues);
		const char * ptr = "[group]\nkey = abc";
		while (*ptr != 0) {
			TEST_ASSERT_TRUE(ini.parse(int(*ptr)));
			ptr++;
		}
		TEST_ASSERT_FALSE(ini.parse(0));
		TEST_ASSERT_EQUAL_size_t(2, ini.getLine());
	}
	/* parse beyond error */
	{
		IniParser ini(ignoreAllValues);
		const char * ptr = "[gr oup]";
		while (*ptr != 0) {
			ini.parse(int(*ptr));
			ptr++;
		}
		TEST_ASSERT_FALSE(ini.parse(' '));
		TEST_ASSERT_EQUAL_size_t(1, ini.getLine());
	}
	/* abort by mapping provider */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = abc", abort));
	/* mapping provider breaks parsing state (for coverage testing only) */
	TEST_ASSERT_EQUAL_size_t(2, IniParser::parseString("[group]\nkey = abc", breakState));
}


/**
 * Test `IniParser::StringHelper` functions.
 */
void test_string_helper() {
	const auto mapString = [&] (IniParser::Context & ctx) -> bool {
		TEST_ASSERT_TRUE(ctx.group == "group");
		TEST_ASSERT_TRUE(ctx.group != "GROUP");
		TEST_ASSERT_FALSE(ctx.group == "GROUP");
		TEST_ASSERT_TRUE(ctx.key == "key");
		TEST_ASSERT_FALSE(ctx.key == "KEY");
		TEST_ASSERT_TRUE(ctx.key != "KEY");
		TEST_ASSERT_TRUE(ctx.group < "grp");
		TEST_ASSERT_TRUE(ctx.group > "gr");
		TEST_ASSERT_FALSE(ctx.group <= "gr");
		TEST_ASSERT_FALSE(ctx.group >= "grp");
		TEST_ASSERT_TRUE(ctx.group.startsWith("group"));
		TEST_ASSERT_TRUE(ctx.group.startsWith("grou"));
		TEST_ASSERT_TRUE(ctx.group.startsWith("gro"));
		TEST_ASSERT_TRUE(ctx.group.startsWith("gr"));
		TEST_ASSERT_TRUE(ctx.group.startsWith("g"));
		TEST_ASSERT_FALSE(ctx.group.startsWith("G"));
		TEST_ASSERT_TRUE(strcmp(ctx.group, "group") == 0);
		TEST_ASSERT_TRUE(ctx.group[0] == 'g');
		return true;
	};
	TEST_ASSERT_EQUAL_size_t(0, IniParser::parseString("[group]\nkey = abc", mapString));
}


/**
 * Test `IniParserTpl` and related functions.
 */
void test_template_sized_parser() {
	static const char * iniStrOk = "[group]\nkey = 'abc'";
	static const char * iniStrNok = "[group]\nkey = 'a\nbc'";
	char str[8];
	const char * ptr;
	const auto mapString = [&] (IniParser::Context & ctx) -> bool {
		ctx.mapString(str);
		return true;
	};
	const auto dataProvider = [&] () -> int {
		const int ch = *ptr;
		if (ch > 0) {
			ptr++;
			return ch;
		}
		return -1;
	};
	/* from string */
	memset(str, 0, sizeof(str));
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<8>(iniStrOk, mapString));
	TEST_ASSERT_EQUAL_STRING("abc", str);
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniStrNok, mapString));
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniStrNok, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(1, iniParseString<5>(iniStrOk, ignoreAllValues));
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<6>(iniStrOk, ignoreAllValues));
	/* from data provider */
	memset(str, 0, sizeof(str));
	ptr = iniStrOk;
	TEST_ASSERT_EQUAL_size_t(0, iniParseFn<8>(dataProvider, mapString));
	TEST_ASSERT_EQUAL_STRING("abc", str);
	ptr = iniStrNok;
	TEST_ASSERT_EQUAL_size_t(2, iniParseFn<8>(dataProvider, mapString));
	ptr = iniStrNok;
	TEST_ASSERT_EQUAL_size_t(2, iniParseFn<8>(dataProvider, ignoreAllValues));
	ptr = iniStrOk;
	TEST_ASSERT_EQUAL_size_t(1, iniParseFn<5>(dataProvider, ignoreAllValues));
	ptr = iniStrOk;
	TEST_ASSERT_EQUAL_size_t(0, iniParseFn<6>(dataProvider, ignoreAllValues));
}


class MapNum123 {
private:
	uint32_t num;
public:
	inline MapNum123():
		num{0}
	{}
	inline bool operator() (IniParser::Context & ctx, const bool parsed) {
		if ( ! parsed ) {
			ctx.mapNumber(this->num);
		} else if (this->num != 123) {
			return false;
		}
		return true;
	}
};


class MapSNum123 {
private:
	int32_t num;
public:
	inline MapSNum123():
		num{0}
	{}
	inline bool operator() (IniParser::Context & ctx, const bool parsed) {
		if ( ! parsed ) {
			ctx.mapNumber(this->num);
		} else if (this->num != -123) {
			return false;
		}
		return true;
	}
};


class MapStringAbc {
private:
	char str[8];
public:
	inline MapStringAbc():
		str{0}
	{}
	inline bool operator() (IniParser::Context & ctx, const bool parsed) {
		if ( ! parsed ) {
			ctx.mapString(this->str);
		} else if (strcmp(this->str, "abc") != 0) {
			return false;
		}
		return true;
	}
};


/**
 * Test mapping provider with two arguments for custom value verification.
 */
void test_custom_value_verification() {
	/* unsigned number */
	static const char * iniNumOk = "[group]\nkey = 123";
	static const char * iniNumNok = "[group]\nkey = 1234";
	uint32_t num;
	const auto mapNumber123 = [&] (IniParser::Context & ctx, const bool parsed) -> bool {
		if ( ! parsed ) {
			ctx.mapNumber(num);
		} else if (num != 123) {
			return false;
		}
		return true;
	};
	/* lambda */
	num = 0;
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<8>(iniNumOk, mapNumber123));
	num = 0;
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniNumNok, mapNumber123));
	/* functor */
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<8>(iniNumOk, MapNum123()));
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniNumNok, MapNum123()));
	/* signed number */
	static const char * iniSNumOk = "[group]\nkey = -123";
	static const char * iniSNumNok = "[group]\nkey = -1234";
	int32_t sNum;
	const auto mapSNumber123 = [&] (IniParser::Context & ctx, const bool parsed) -> bool {
		if ( ! parsed ) {
			ctx.mapNumber(sNum);
		} else if (sNum != -123) {
			return false;
		}
		return true;
	};
	/* lambda */
	sNum = 0;
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<8>(iniSNumOk, mapSNumber123));
	sNum = 0;
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniSNumNok, mapSNumber123));
	/* functor */
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<8>(iniSNumOk, MapSNum123()));
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniSNumNok, MapSNum123()));
	/* string */
	static const char * iniStrOk = "[group]\nkey = 'abc'";
	static const char * iniStrNok = "[group]\nkey = 'abcd'";
	char str[8];
	const auto mapStringAbc = [&] (IniParser::Context & ctx, const bool parsed) -> bool {
		if ( ! parsed ) {
			ctx.mapString(str);
		} else if (strcmp(str, "abc") != 0) {
			return false;
		}
		return true;
	};
	/* lambda */
	memset(str, 0, sizeof(str));
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<8>(iniStrOk, mapStringAbc));
	memset(str, 0, sizeof(str));
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniStrNok, mapStringAbc));
	/* functor */
	TEST_ASSERT_EQUAL_size_t(0, iniParseString<8>(iniStrOk, MapStringAbc()));
	TEST_ASSERT_EQUAL_size_t(2, iniParseString<8>(iniStrNok, MapStringAbc()));
}


/**
 * Main entry point for all unit tests.
 */
int main() {
	UNITY_BEGIN();

	RUN_TEST(test_empty_ini);
	RUN_TEST(test_invalid_characters);
	RUN_TEST(test_invalid_formats);
	RUN_TEST(test_valid_formats);
	RUN_TEST(test_string_values);
	RUN_TEST(test_unsigned_number_values);
	RUN_TEST(test_signed_number_values);
	RUN_TEST(test_size_limits);
	RUN_TEST(test_data_function);
	RUN_TEST(test_sized_string_input);
	RUN_TEST(test_special_errors);
	RUN_TEST(test_string_helper);
	RUN_TEST(test_template_sized_parser);
	RUN_TEST(test_custom_value_verification);

	UNITY_END();
	return 0;
}
