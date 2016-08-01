%{
#include <assert.h>
#include <iostream>
#include <string>

#include "driver.h"

#define yylex(p) p.lex()
%}

////////////////////////////////////////

// general settings
%require "3.0"
%debug
%start file
%locations
%verbose
//%no-lines
%error-verbose

////////////////////////////////////////

// c++ skeleton and options
%skeleton "lalr1.cc"

%define api.value.type variant
%define api.token.constructor // C++ style of handling variants
%define parse.assert // check C++ variant types

%code requires // forward decl of C++ driver (our parser) in HPP
{
#include "bazel.h"

class ParserDriver;
}

// param to yy::parser() constructor
// the parsing context
%param { ParserDriver &driver }

////////////////////////////////////////

// tokens and types
%token EOQ 0 "end of file"
%token ERROR_SYMBOL
%token L_BRACKET R_BRACKET COMMA QUOTE SEMICOLON COLON POINT
       L_CURLY_BRACKET R_CURLY_BRACKET SHARP R_ARROW EQUAL
       L_SQUARE_BRACKET R_SQUARE_BRACKET
	   PLUS
%token CLASS

%token <std::string> STRING KEYWORD ID
%token <int> INTEGER

%type <std::string> identifier string

%type <bazel::Function> function_call statement
%type <bazel::File> statements
%type <bazel::Parameter> parameter variable_decl
%type <bazel::Parameters> parameters
%type <bazel::Values> expr array array_contents array_content

////////////////////////////////////////

%%

file: statements EOQ
    { driver.bazel_file = $1; }
    ;

statements: statement
	{ $$.functions.push_back($1); }
	| statements statement
	{
		$1.functions.push_back($2);
		$$ = std::move($1);
	}
	;

statement: function_call
	{ $$ = $1; }
	| variable_decl
	{
        $$.name = $1.name;
        $$.parameters.push_back($1);
    }
	;

function_call: identifier L_BRACKET parameters R_BRACKET
	{
		bazel::Function f;
		f.name = $1;
        f.parameters = $3;
		$$ = f;
	}
	;

parameters: parameter
    { $$.push_back($1); }
	| parameter COMMA
    { $$.push_back($1); }
	| parameter COMMA parameters
    {
        $3.push_back($1);
        $$ = std::move($3);
    }
	;

parameter: variable_decl
    { $$ = $1; }
	| expr
    {
        bazel::Parameter p;
        p.values = $1;
        $$ = p;
    }
	;

variable_decl: identifier EQUAL expr
	{ $$ = bazel::Parameter{ $1, $3 }; }
	;

expr: identifier
    {
        bazel::Values v;
        v.insert($1);
        $$ = v;
    }
	| string
    {
        bazel::Values v;
        v.insert($1);
        $$ = v;
    }
	| array
    { $$ = std::move($1); }
	| expr PLUS expr
    {
        $1.insert($3.begin(), $3.end());
        $$ = std::move($1);
    }
	| function_call
    {
        bazel::Values v;
        v.insert("fcall");
        $$ = v;
    }
	;

array: L_SQUARE_BRACKET array_contents R_SQUARE_BRACKET
    { $$ = std::move($2); }
	;

array_contents: /* empty */
    {}
	| array_content
    { $$ = std::move($1); }
	| array_content COMMA array_contents
    {
        $3.insert($1.begin(), $1.end());
        $$ = std::move($3);
    }
	;

array_content: expr
    { $$ = std::move($1); }
	;

identifier: ID
	{ $$ = $1; }
	;
string: STRING
	{ $$ = $1; }
	;

%%

void yy::parser::error(const location_type& l, const std::string& m)
{
    driver.error(l, m);
}
