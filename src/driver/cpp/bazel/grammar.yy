%{
#include <assert.h>
#include <iostream>
#include <string>

#include "bazel/driver.h"

#define yylex(p) p.lex()
%}

////////////////////////////////////////

// general settings
%require "3.0"
%debug
%start file
%locations
%verbose
%no-lines
%error-verbose

////////////////////////////////////////

// c++ skeleton and options
%skeleton "lalr1.cc"

%define api.prefix {yy_bazel}
%define api.value.type variant
%define api.token.constructor // C++ style of handling variants
%define parse.assert // check C++ variant types

%code requires // forward decl of C++ driver (our parser) in HPP
{
#include "bazel/bazel.h"

class BazelParserDriver;
}

// param to yy_bazel::parser() constructor
// the parsing context
%param { BazelParserDriver &driver }

////////////////////////////////////////

// tokens and types
%token EOQ 0 "end of file"
%token ERROR_SYMBOL
%token L_BRACKET R_BRACKET COMMA QUOTE SEMICOLON COLON POINT
       L_CURLY_BRACKET R_CURLY_BRACKET SHARP R_ARROW EQUAL
       L_SQUARE_BRACKET R_SQUARE_BRACKET
	   PLUS DEF END_OF_DEF
%token CLASS

%token <std::string> STRING KEYWORD ID
%token <int> INTEGER

%type <std::string> identifier string keyword

%type <bazel::Function> function_call statement
%type <bazel::File> statements
%type <bazel::Parameter> parameter variable_decl
%type <bazel::Parameters> parameters
%type <bazel::Values> expr array array_contents array_content tuple

////////////////////////////////////////

%%

file: statements EOQ
    { /*driver.bazel_file = $1;*/ }
    ;

statements: statement
	{ /*$$.functions.push_back($1);*/ }
	| statements statement
	{
		/*$1.functions.push_back($2);*/
		$$ = std::move($1);
	}
	;

statement: /*function_call
	{ $$ = $1; }
	| */variable_decl
	{
        // ?
        $$.name = $1.name;
        $$.parameters.push_back($1);
        // global vars
        driver.bazel_file.parameters[$1.name] = $1;
    }
    | expr
	{ /*$$ = $1;*/ }
    | function_def
	{ /*$$ = $1;*/ }
	;

function_def: DEF function_call COLON exprs END_OF_DEF

function_call: identifier L_BRACKET parameters R_BRACKET
	{
		bazel::Function f;
		f.name = $1;
        f.parameters = $3;
		$$ = f;
        driver.bazel_file.functions.push_back(f);
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

parameter: /* empty */
    {}
    | variable_decl
    { $$ = $1; }
	| expr
    {
        bazel::Parameter p;
        p.values = $1;
        $$ = p;
    }
    | kv_map
    {
        bazel::Parameter p;
        p.name = "kv_map";
        $$ = p;
    }
	;

variable_decl: identifier EQUAL expr
	{ $$ = bazel::Parameter{ $1, $3 }; }
    | identifier EQUAL kv_map
	{ $$ = bazel::Parameter{ $1 }; }
	;

exprs: expr
	{}
	| exprs expr
	{}
	;

expr: identifier
    {
        bazel::Values v;
        v.insert($1);
        $$ = v;
    }
    | identifier POINT function_call
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
    | expr EQUAL expr
    {
        $1.clear();
        $1.insert($3.begin(), $3.end());
        $$ = std::move($1);
    }
	| expr PLUS expr
    {
        $1.insert($3.begin(), $3.end());
        $$ = std::move($1);
    }
    // "src/" + s for s in RELATIVE_WELL_KNOWN_PROTOS
	/*| expr PLUS expr keyword identifier keyword identifier
    {
        $1.insert($3.begin(), $3.end());
        $$ = std::move($1);
    }*/
	| function_call
    {
        bazel::Values v;
        //v.insert("fcall");
        $$ = v;
        //$$ = $1;
    }
    | tuple
    /*| kv_map
    {
        bazel::Values v;
        //v.insert("kv_map");
        $$ = v;
    }*/
    | keyword expr
	{ /*$$ = $1;*/ }
    | keyword expr COLON
	{ /*$$ = $1;*/ }
	;

tuple: L_BRACKET tuple_values R_BRACKET
    {}
    ;

tuple_values: /* empty */
    | expr
    | expr COMMA tuple_values
    ;

kv_map: L_CURLY_BRACKET kv_map_values R_CURLY_BRACKET
    ;

kv_map_values: /* empty */
    | kv_map_value
    | kv_map_value COMMA kv_map_values
    ;

kv_map_value: string COLON expr
    ;

array: L_SQUARE_BRACKET array_contents R_SQUARE_BRACKET
    { $$ = std::move($2); }
    | expr for_op
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

array_subscripts: array_subscript
    | array_subscript array_subscripts
    ;

array_subscript: L_SQUARE_BRACKET expr R_SQUARE_BRACKET
    ;

for_op: keyword identifier keyword expr
    ;

identifier: ID
	{ $$ = $1; }
    | ID array_subscripts
	{ $$ = $1; }
	;
string: STRING
	{ $$ = $1; }
	;
keyword: KEYWORD
	{ $$ = $1; }
    ;

%%

void yy_bazel::parser::error(const location_type& l, const std::string& m)
{
    driver.error(l, m);
}
