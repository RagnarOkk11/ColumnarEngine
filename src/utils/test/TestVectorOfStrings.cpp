#include "utils/VectorOfStrings.h"

#include <catch.hpp>
#include <vector>
#include <string>

TEST_CASE("VectorOfStrings: Basic operations", "[VectorOfStrings]") {
    VectorOfStrings vec;
    REQUIRE(vec.Size() == 0);
    REQUIRE(vec.DataSize() == 0);

    vec.PushBack("hello");
    vec.PushBack("world");
    
    REQUIRE(vec.Size() == 2);
    REQUIRE(vec.GetString(0) == "hello");
    REQUIRE(vec.GetString(1) == "world");
}

TEST_CASE("VectorOfStrings2D: Basic operations", "[VectorOfStrings2D]") {
    VectorOfStrings2D vec;
    vec.StartAddString();
    vec.ContinueAddString("hello");
    vec.EndAddString();

    vec.StartAddString();
    vec.ContinueAddString("world");
    vec.EndAddString();
    
    vec.StartNewLine();

    vec.StartAddString();
    vec.ContinueAddString("foo");
    vec.EndAddString();

    vec.StartAddString();
    vec.ContinueAddString("bar");
    vec.EndAddString();
    
    vec.StartNewLine();

    REQUIRE(vec.Height() == 2);
    REQUIRE(vec.Width() == 2);
    REQUIRE(vec.GetString2D(0, 0) == "hello");
    REQUIRE(vec.GetString2D(0, 1) == "world");
    REQUIRE(vec.GetString2D(1, 0) == "foo");
    REQUIRE(vec.GetString2D(1, 1) == "bar");
}

TEST_CASE("VectorOfStrings2D supports 1D vector<string> mode", "[vector_of_strings]") {
	VectorOfStrings2D values;

	values.AddString("alpha");
	values.AddString("beta");
	values.AddString("");

	REQUIRE(values.Height() == 1);
	REQUIRE(values.GetString(0) == std::string_view("alpha"));
	REQUIRE(values.GetString(1) == std::string_view("beta"));
	REQUIRE(values.GetString(2) == std::string_view(""));
}

TEST_CASE("VectorOfStrings2D validates incremental add protocol", "[vector_of_strings]") {
	VectorOfStrings2D values;

	REQUIRE_THROWS_AS(values.ContinueAddString('x'), std::runtime_error);

	values.StartAddString();
	REQUIRE_THROWS_AS(values.StartAddString(), std::runtime_error);
	values.ContinueAddString('o');
	values.ContinueAddString('k');
	values.EndAddString();

	REQUIRE(values.GetString(0) == std::string_view("ok"));
	REQUIRE_THROWS_AS(values.EndAddString(), std::runtime_error);

	values.StartAddString();
	REQUIRE_THROWS_AS(values.StartNewLine(), std::runtime_error);
	REQUIRE_THROWS_AS(values.AddString("mixed"), std::runtime_error);
}

TEST_CASE("VectorOfStrings2D supports rectangular 2D table access", "[vector_of_strings]") {
	VectorOfStrings2D table;

	table.AddString("r0c0");
	table.AddString("r0c1");
	table.StartNewLine();

	table.AddString("r1c0");
	table.AddString("r1c1");

	REQUIRE(table.Height() == 2);
	REQUIRE(table.Width() == 2);
	REQUIRE(table.GetString2D(0, 0) == std::string_view("r0c0"));
	REQUIRE(table.GetString2D(0, 1) == std::string_view("r0c1"));
	REQUIRE(table.GetString2D(1, 0) == std::string_view("r1c0"));
	REQUIRE(table.GetString2D(1, 1) == std::string_view("r1c1"));

	REQUIRE_THROWS_AS(table.GetString(0), std::runtime_error);
}

TEST_CASE("VectorOfStrings2D enforces equal column count across rows", "[vector_of_strings]") {
	VectorOfStrings2D table;

	table.AddString("r0c0");
	table.AddString("r0c1");
	table.StartNewLine();

	table.AddString("r1c0");
	REQUIRE_THROWS_AS(table.StartNewLine(), std::runtime_error);
}

TEST_CASE("VectorOfStrings2D validates indexes", "[vector_of_strings]") {
	VectorOfStrings2D values;
	values.AddString("a");

	REQUIRE_THROWS_AS(values.GetString(1), std::runtime_error);

	VectorOfStrings2D table;
	table.AddString("x");
	table.StartNewLine();
	table.AddString("y");

	REQUIRE_THROWS_AS(table.GetString2D(2, 0), std::runtime_error);
	REQUIRE_THROWS_AS(table.GetString2D(0, 1), std::runtime_error);
}

TEST_CASE("VectorOfStrings2D supports BackLastString and PopLastChar", "[vector_of_strings]") {
	VectorOfStrings2D values;

	values.StartAddString();
	REQUIRE(values.EmptyLastString());
	REQUIRE_THROWS_AS(values.BackLastString(), std::runtime_error);
	REQUIRE_THROWS_AS(values.PopLastChar(), std::runtime_error);

	values.ContinueAddString('a');
	values.ContinueAddString('b');
	values.ContinueAddString('c');
	REQUIRE(values.BackLastString() == 'c');

	values.PopLastChar();
	REQUIRE(values.BackLastString() == 'b');
	values.EndAddString();

	REQUIRE(values.GetString(0) == std::string_view("ab"));
	REQUIRE_THROWS_AS(values.BackLastString(), std::runtime_error);
	REQUIRE_THROWS_AS(values.PopLastChar(), std::runtime_error);
}

TEST_CASE("VectorOfStrings2D reads non square 2D table correctly", "[vector_of_strings]") {
	VectorOfStrings2D table;

	table.AddString("r0c0");
	table.AddString("r0c1");
	table.AddString("r0c2");
	table.StartNewLine();
	table.AddString("r1c0");
	table.AddString("r1c1");
	table.AddString("r1c2");

	REQUIRE(table.Width() == 3);
	REQUIRE(table.Height() == 2);
	REQUIRE(table.GetString2D(1, 0) == std::string_view("r1c0"));
	REQUIRE(table.GetString2D(1, 1) == std::string_view("r1c1"));
	REQUIRE(table.GetString2D(1, 2) == std::string_view("r1c2"));
}
