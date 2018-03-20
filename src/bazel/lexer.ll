%{
#pragma warning(disable: 4005)
#include <string>

#include "bazel/grammar.hpp"

#define YY_USER_ACTION loc.columns(yyleng);

#define YY_DECL yy_bazel::parser::symbol_type yylex(yyscan_t yyscanner, yy_bazel::location &loc)

#define MAKE(x) yy_bazel::parser::make_ ## x(loc)
#define MAKE_VALUE(x, v) yy_bazel::parser::make_ ## x((v), loc)
%}

%option nounistd
%option yylineno
%option nounput
%option batch
%option never-interactive
%option reentrant
%option noyywrap
%option prefix="ll_bazel"

identifier_old_no_integer [_a-zA-Z][_a-zA-Z0-9]*

identifier      [_a-zA-Z0-9]+
quote1          "\'"[^'\\]*"\'"
quote2          "\""[^"\\]*"\""

%s IN_FUNCTION

%%

%{
    // Code run each time yylex is called.
    loc.step();
%}

#.*/\n                  // ignore comments

<IN_FUNCTION>\n\n       {
                            loc.lines(yyleng);
                            loc.step();
                            BEGIN(0);
                            return MAKE(END_OF_DEF);
                        }

[ \t]+                  loc.step();
\r                      loc.step();
\n                      {
                            loc.lines(yyleng);
                            loc.step();
                        }

";"                     return MAKE(SEMICOLON);
":"                     return MAKE(COLON);
"("                     return MAKE(L_BRACKET);
")"                     return MAKE(R_BRACKET);
"{"                     return MAKE(L_CURLY_BRACKET);
"}"                     return MAKE(R_CURLY_BRACKET);
"["                     return MAKE(L_SQUARE_BRACKET);
"]"                     return MAKE(R_SQUARE_BRACKET);
","                     return MAKE(COMMA);
"\."                    return MAKE(POINT);
"\+"                    return MAKE(PLUS);
"->"                    return MAKE(R_ARROW);
"="                     return MAKE(EQUAL);

"def"                   BEGIN(IN_FUNCTION); return MAKE(DEF);

and|elif|global|or|assert|else|if|except|pass|break|import|print|exec|in|raise|continue|finally|is|return|for|lambda|try|del|from|not|while return MAKE_VALUE(KEYWORD, yytext);
class					return MAKE(CLASS);

{identifier}			return MAKE_VALUE(ID, yytext);

{quote1}				|
{quote2}				return MAKE_VALUE(STRING, yytext);

.                       { /*driver.error(loc, "invalid character");*/ return MAKE(ERROR_SYMBOL); }
<<EOF>>                 return MAKE(EOQ);

%%
