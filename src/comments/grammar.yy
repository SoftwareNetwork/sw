%{
#include <assert.h>
#include <iostream>
#include <string>

#include "comments/driver.h"

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

%define api.prefix {yy_comments}
%define api.value.type variant
%define api.token.constructor // C++ style of handling variants
%define parse.assert // check C++ variant types

%code requires // forward decl of C++ driver (our parser) in HPP
{
class CommentsParserDriver;
}

// param to yy_comments::parser() constructor
// the parsing context
%param { CommentsParserDriver &driver }

////////////////////////////////////////

// tokens and types
%token EOQ 0 "end of file"
%token ERROR_SYMBOL

%token <std::string> STRING
%type <std::string> comment err

////////////////////////////////////////

%%

file: comments EOQ
    ;

comments: comment
	{ driver.comments.push_back($1); }
	| comments comment
	{ driver.comments.push_back($2); }
	;

comment: STRING
	{ $$ = $1; }
    | err
	;
err: ERROR_SYMBOL
    { return 0; }
    ;

%%

void yy_comments::parser::error(const location_type& l, const std::string& m)
{
    driver.error(l, m);
}
