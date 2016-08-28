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
class ParserDriver;
}

// param to yy::parser() constructor
// the parsing context
%param { ParserDriver &driver }

////////////////////////////////////////

// tokens and types
%token EOQ 0 "end of file"
%token ERROR_SYMBOL

%token <std::string> STRING
%type <std::string> comment

////////////////////////////////////////

%%

file: comments EOQ
    ;

comments: comment
	{ driver.comments.push_back($1); }
	| comments comment
	{ driver.comments.push_back($2); }
    | comments ERROR_SYMBOL
    { return 0; }
	;

comment: STRING
	{ $$ = $1; }
	;

%%

void yy::parser::error(const location_type& l, const std::string& m)
{
    driver.error(l, m);
}
