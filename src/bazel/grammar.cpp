// A Bison parser, made by GNU Bison 3.0.4.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// Take the name prefix into account.
#define yylex   yy_bazellex

// First part of user declarations.


#include <assert.h>
#include <iostream>
#include <string>

#include "bazel/driver.h"

#define yylex(p) p.lex()



# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

#include "grammar.hpp"

// User implementation prologue.




#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (/*CONSTCOND*/ false)
# endif


// Suppress unused-variable warnings by "using" E.
#define YYUSE(E) ((void) (E))

// Enable debugging if requested.
#if YY_BAZELDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << std::endl;                  \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yystack_print_ ();                \
  } while (false)

#else // !YY_BAZELDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YYUSE(Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void>(0)
# define YY_STACK_PRINT()                static_cast<void>(0)

#endif // !YY_BAZELDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)


namespace yy_bazel {


  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  parser::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr = "";
        char const *yyp = yystr;

        for (;;)
          switch (*++yyp)
            {
            case '\'':
            case ',':
              goto do_not_strip_quotes;

            case '\\':
              if (*++yyp != '\\')
                goto do_not_strip_quotes;
              // Fall through.
            default:
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }


  /// Build a parser object.
  parser::parser (BazelParserDriver &driver_yyarg)
    :
#if YY_BAZELDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      driver (driver_yyarg)
  {}

  parser::~parser ()
  {}


  /*---------------.
  | Symbol types.  |
  `---------------*/



  // by_state.
  inline
  parser::by_state::by_state ()
    : state (empty_state)
  {}

  inline
  parser::by_state::by_state (const by_state& other)
    : state (other.state)
  {}

  inline
  void
  parser::by_state::clear ()
  {
    state = empty_state;
  }

  inline
  void
  parser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  inline
  parser::by_state::by_state (state_type s)
    : state (s)
  {}

  inline
  parser::symbol_number_type
  parser::by_state::type_get () const
  {
    if (state == empty_state)
      return empty_symbol;
    else
      return yystos_[state];
  }

  inline
  parser::stack_symbol_type::stack_symbol_type ()
  {}


  inline
  parser::stack_symbol_type::stack_symbol_type (state_type s, symbol_type& that)
    : super_type (s, that.location)
  {
      switch (that.type_get ())
    {
      case 26: // statements
        value.move< bazel::File > (that.value);
        break;

      case 27: // statement
      case 28: // function_call
        value.move< bazel::Function > (that.value);
        break;

      case 30: // parameter
      case 31: // variable_decl
        value.move< bazel::Parameter > (that.value);
        break;

      case 29: // parameters
        value.move< bazel::Parameters > (that.value);
        break;

      case 32: // expr
      case 33: // tuple
      case 38: // array
      case 39: // array_contents
      case 40: // array_content
        value.move< bazel::Values > (that.value);
        break;

      case 23: // INTEGER
        value.move< int > (that.value);
        break;

      case 20: // STRING
      case 21: // KEYWORD
      case 22: // ID
      case 44: // identifier
      case 45: // string
      case 46: // keyword
        value.move< std::string > (that.value);
        break;

      default:
        break;
    }

    // that is emptied.
    that.type = empty_symbol;
  }

  inline
  parser::stack_symbol_type&
  parser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
      switch (that.type_get ())
    {
      case 26: // statements
        value.copy< bazel::File > (that.value);
        break;

      case 27: // statement
      case 28: // function_call
        value.copy< bazel::Function > (that.value);
        break;

      case 30: // parameter
      case 31: // variable_decl
        value.copy< bazel::Parameter > (that.value);
        break;

      case 29: // parameters
        value.copy< bazel::Parameters > (that.value);
        break;

      case 32: // expr
      case 33: // tuple
      case 38: // array
      case 39: // array_contents
      case 40: // array_content
        value.copy< bazel::Values > (that.value);
        break;

      case 23: // INTEGER
        value.copy< int > (that.value);
        break;

      case 20: // STRING
      case 21: // KEYWORD
      case 22: // ID
      case 44: // identifier
      case 45: // string
      case 46: // keyword
        value.copy< std::string > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    return *this;
  }


  template <typename Base>
  inline
  void
  parser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YY_BAZELDEBUG
  template <typename Base>
  void
  parser::yy_print_ (std::ostream& yyo,
                                     const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YYUSE (yyoutput);
    symbol_number_type yytype = yysym.type_get ();
    // Avoid a (spurious) G++ 4.8 warning about "array subscript is
    // below array bounds".
    if (yysym.empty ())
      std::abort ();
    yyo << (yytype < yyntokens_ ? "token" : "nterm")
        << ' ' << yytname_[yytype] << " ("
        << yysym.location << ": ";
    YYUSE (yytype);
    yyo << ')';
  }
#endif

  inline
  void
  parser::yypush_ (const char* m, state_type s, symbol_type& sym)
  {
    stack_symbol_type t (s, sym);
    yypush_ (m, t);
  }

  inline
  void
  parser::yypush_ (const char* m, stack_symbol_type& s)
  {
    if (m)
      YY_SYMBOL_PRINT (m, s);
    yystack_.push (s);
  }

  inline
  void
  parser::yypop_ (unsigned int n)
  {
    yystack_.pop (n);
  }

#if YY_BAZELDEBUG
  std::ostream&
  parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  parser::debug_level_type
  parser::debug_level () const
  {
    return yydebug_;
  }

  void
  parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YY_BAZELDEBUG

  inline parser::state_type
  parser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - yyntokens_] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - yyntokens_];
  }

  inline bool
  parser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  parser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  parser::parse ()
  {
    // State.
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

    // FIXME: This shoud be completely indented.  It is not yet to
    // avoid gratuitous conflicts when merging into the master branch.
    try
      {
    YYCDEBUG << "Starting parse" << std::endl;


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, yyla);

    // A new symbol was pushed on the stack.
  yynewstate:
    YYCDEBUG << "Entering state " << yystack_[0].state << std::endl;

    // Accept?
    if (yystack_[0].state == yyfinal_)
      goto yyacceptlab;

    goto yybackup;

    // Backup.
  yybackup:

    // Try to take a decision without lookahead.
    yyn = yypact_[yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token: ";
        try
          {
            symbol_type yylookahead (yylex (driver));
            yyla.move (yylookahead);
          }
        catch (const syntax_error& yyexc)
          {
            error (yyexc);
            goto yyerrlab1;
          }
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.type_get ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.type_get ())
      goto yydefault;

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", yyn, yyla);
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- Do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_(yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
        switch (yyr1_[yyn])
    {
      case 26: // statements
        yylhs.value.build< bazel::File > ();
        break;

      case 27: // statement
      case 28: // function_call
        yylhs.value.build< bazel::Function > ();
        break;

      case 30: // parameter
      case 31: // variable_decl
        yylhs.value.build< bazel::Parameter > ();
        break;

      case 29: // parameters
        yylhs.value.build< bazel::Parameters > ();
        break;

      case 32: // expr
      case 33: // tuple
      case 38: // array
      case 39: // array_contents
      case 40: // array_content
        yylhs.value.build< bazel::Values > ();
        break;

      case 23: // INTEGER
        yylhs.value.build< int > ();
        break;

      case 20: // STRING
      case 21: // KEYWORD
      case 22: // ID
      case 44: // identifier
      case 45: // string
      case 46: // keyword
        yylhs.value.build< std::string > ();
        break;

      default:
        break;
    }


      // Compute the default @$.
      {
        slice<stack_symbol_type, stack_type> slice (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, slice, yylen);
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
      try
        {
          switch (yyn)
            {
  case 2:

    { /*driver.bazel_file = $1;*/ }

    break;

  case 3:

    { /*$$.functions.push_back($1);*/ }

    break;

  case 4:

    {
		/*$1.functions.push_back($2);*/
		yylhs.value.as< bazel::File > () = std::move(yystack_[1].value.as< bazel::File > ());
	}

    break;

  case 5:

    {
        yylhs.value.as< bazel::Function > ().name = yystack_[0].value.as< bazel::Parameter > ().name;
        yylhs.value.as< bazel::Function > ().parameters.push_back(yystack_[0].value.as< bazel::Parameter > ());
    }

    break;

  case 6:

    { /*$$ = $1;*/ }

    break;

  case 7:

    {
		bazel::Function f;
		f.name = yystack_[3].value.as< std::string > ();
        f.parameters = yystack_[1].value.as< bazel::Parameters > ();
		yylhs.value.as< bazel::Function > () = f;
        driver.bazel_file.functions.push_back(f);
	}

    break;

  case 8:

    { yylhs.value.as< bazel::Parameters > ().push_back(yystack_[0].value.as< bazel::Parameter > ()); }

    break;

  case 9:

    { yylhs.value.as< bazel::Parameters > ().push_back(yystack_[1].value.as< bazel::Parameter > ()); }

    break;

  case 10:

    {
        yystack_[0].value.as< bazel::Parameters > ().push_back(yystack_[2].value.as< bazel::Parameter > ());
        yylhs.value.as< bazel::Parameters > () = std::move(yystack_[0].value.as< bazel::Parameters > ());
    }

    break;

  case 11:

    {}

    break;

  case 12:

    { yylhs.value.as< bazel::Parameter > () = yystack_[0].value.as< bazel::Parameter > (); }

    break;

  case 13:

    {
        bazel::Parameter p;
        p.values = yystack_[0].value.as< bazel::Values > ();
        yylhs.value.as< bazel::Parameter > () = p;
    }

    break;

  case 14:

    {
        bazel::Parameter p;
        p.name = "kv_map";
        yylhs.value.as< bazel::Parameter > () = p;
    }

    break;

  case 15:

    { yylhs.value.as< bazel::Parameter > () = bazel::Parameter{ yystack_[2].value.as< std::string > (), yystack_[0].value.as< bazel::Values > () }; }

    break;

  case 16:

    { yylhs.value.as< bazel::Parameter > () = bazel::Parameter{ yystack_[2].value.as< std::string > () }; }

    break;

  case 17:

    {
        bazel::Values v;
        v.insert(yystack_[0].value.as< std::string > ());
        yylhs.value.as< bazel::Values > () = v;
    }

    break;

  case 18:

    {
        bazel::Values v;
        v.insert(yystack_[2].value.as< std::string > ());
        yylhs.value.as< bazel::Values > () = v;
    }

    break;

  case 19:

    {
        bazel::Values v;
        v.insert(yystack_[0].value.as< std::string > ());
        yylhs.value.as< bazel::Values > () = v;
    }

    break;

  case 20:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ()); }

    break;

  case 21:

    {
        yystack_[2].value.as< bazel::Values > ().insert(yystack_[0].value.as< bazel::Values > ().begin(), yystack_[0].value.as< bazel::Values > ().end());
        yylhs.value.as< bazel::Values > () = std::move(yystack_[2].value.as< bazel::Values > ());
    }

    break;

  case 22:

    {
        bazel::Values v;
        //v.insert("fcall");
        yylhs.value.as< bazel::Values > () = v;
        //$$ = $1;
    }

    break;

  case 24:

    {}

    break;

  case 33:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[1].value.as< bazel::Values > ()); }

    break;

  case 35:

    {}

    break;

  case 36:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ()); }

    break;

  case 37:

    {
        yystack_[0].value.as< bazel::Values > ().insert(yystack_[2].value.as< bazel::Values > ().begin(), yystack_[2].value.as< bazel::Values > ().end());
        yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ());
    }

    break;

  case 38:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ()); }

    break;

  case 43:

    { yylhs.value.as< std::string > () = yystack_[0].value.as< std::string > (); }

    break;

  case 44:

    { yylhs.value.as< std::string > () = yystack_[1].value.as< std::string > (); }

    break;

  case 45:

    { yylhs.value.as< std::string > () = yystack_[0].value.as< std::string > (); }

    break;

  case 46:

    { yylhs.value.as< std::string > () = yystack_[0].value.as< std::string > (); }

    break;



            default:
              break;
            }
        }
      catch (const syntax_error& yyexc)
        {
          error (yyexc);
          YYERROR;
        }
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;
      YY_STACK_PRINT ();

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, yylhs);
    }
    goto yynewstate;

  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        error (yyla.location, yysyntax_error_ (yystack_[0].state, yyla));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.type_get () == yyeof_)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:

    /* Pacify compilers like GCC when the user code never invokes
       YYERROR and the label yyerrorlab therefore never appears in user
       code.  */
    if (false)
      goto yyerrorlab;
    yyerror_range[1].location = yystack_[yylen - 1].location;
    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    {
      stack_symbol_type error_token;
      for (;;)
        {
          yyn = yypact_[yystack_[0].state];
          if (!yy_pact_value_is_default_ (yyn))
            {
              yyn += yyterror_;
              if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
                {
                  yyn = yytable_[yyn];
                  if (0 < yyn)
                    break;
                }
            }

          // Pop the current state because it cannot handle the error token.
          if (yystack_.size () == 1)
            YYABORT;

          yyerror_range[1].location = yystack_[0].location;
          yy_destroy_ ("Error: popping", yystack_[0]);
          yypop_ ();
          YY_STACK_PRINT ();
        }

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = yyn;
      yypush_ ("Shifting", error_token);
    }
    goto yynewstate;

    // Accept.
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

    // Abort.
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack"
                 << std::endl;
        // Do not try to display the values of the reclaimed symbols,
        // as their printer might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
  }

  void
  parser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what());
  }

  // Generate an error message.
  std::string
  parser::yysyntax_error_ (state_type yystate, const symbol_type& yyla) const
  {
    // Number of reported tokens (one for the "unexpected", one per
    // "expected").
    size_t yycount = 0;
    // Its maximum.
    enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
    // Arguments of yyformat.
    char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];

    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state
         merging (from LALR or IELR) and default reductions corrupt the
         expected token list.  However, the list is correct for
         canonical LR with one exception: it will still contain any
         token that will not be accepted due to an error action in a
         later state.
    */
    if (!yyla.empty ())
      {
        int yytoken = yyla.type_get ();
        yyarg[yycount++] = yytname_[yytoken];
        int yyn = yypact_[yystate];
        if (!yy_pact_value_is_default_ (yyn))
          {
            /* Start YYX at -YYN if negative to avoid negative indexes in
               YYCHECK.  In other words, skip the first -YYN actions for
               this state because they are default actions.  */
            int yyxbegin = yyn < 0 ? -yyn : 0;
            // Stay within bounds of both yycheck and yytname.
            int yychecklim = yylast_ - yyn + 1;
            int yyxend = yychecklim < yyntokens_ ? yychecklim : yyntokens_;
            for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
              if (yycheck_[yyx + yyn] == yyx && yyx != yyterror_
                  && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
                {
                  if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                    {
                      yycount = 1;
                      break;
                    }
                  else
                    yyarg[yycount++] = yytname_[yyx];
                }
          }
      }

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
        YYCASE_(0, YY_("syntax error"));
        YYCASE_(1, YY_("syntax error, unexpected %s"));
        YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    size_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += yytnamerr_ (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char parser::yypact_ninf_ = -40;

  const signed char parser::yytable_ninf_ = -12;

  const signed char
  parser::yypact_[] =
  {
      49,    49,    49,   -40,    -6,    11,     5,   -40,   -40,   -40,
      -3,   -40,   -40,     2,   -40,    -2,    17,     4,    -3,     6,
      22,    49,   -40,    -6,   -40,   -40,   -40,    49,   -40,   -40,
      12,    44,    12,    44,    49,   -40,   -40,    49,    28,   -40,
      -3,     8,    15,    38,    35,   -40,    -3,   -40,   -40,    40,
      -3,   -40,   -40,   -40,   -40,    49,    39,    51,    41,   -40,
      36,    -3,   -40,    15,    49,   -40,   -40,    -3
  };

  const unsigned char
  parser::yydefact_[] =
  {
       0,    25,    35,    45,    43,     0,     0,     3,    22,     5,
       6,    23,    20,    17,    19,    26,     0,    17,    38,     0,
      36,     0,    44,    39,     1,     2,     4,     0,    46,    34,
       0,    11,     0,     0,    25,    24,    33,    35,     0,    40,
      21,     0,    29,     0,     8,    12,    13,    14,    18,     0,
      15,    16,    27,    37,    41,     0,     0,    30,     0,     7,
       9,    42,    28,    29,     0,    10,    31,    32
  };

  const signed char
  parser::yypgoto_[] =
  {
     -40,   -40,   -40,    55,    30,    10,   -40,   -29,    -1,   -40,
      34,    42,     9,   -40,   -40,    37,   -40,    50,   -40,   -40,
       7,   -39,    43
  };

  const signed char
  parser::yydefgoto_[] =
  {
      -1,     5,     6,     7,     8,    43,    44,     9,    10,    11,
      16,    47,    56,    57,    12,    19,    20,    22,    23,    29,
      17,    14,    30
  };

  const signed char
  parser::yytable_[] =
  {
      15,    18,    45,    58,    34,    25,    31,    13,    31,     1,
      21,    24,    32,    13,    32,    27,    27,    33,    28,    28,
      38,     2,    35,    36,    58,     3,    40,     4,    37,    28,
      46,    45,    50,    15,     4,     3,    18,    41,    13,    49,
       1,    60,   -11,    59,    31,    54,    27,    42,     1,    28,
      64,    62,     2,     1,    61,    42,     3,    63,     4,    46,
       2,    26,    48,    67,     3,     2,     4,    13,    52,     3,
      65,     4,    66,    39,    53,    51,     0,     0,     0,     0,
       0,     0,     0,     0,    55
  };

  const signed char
  parser::yycheck_[] =
  {
       1,     2,    31,    42,     6,     0,     4,     0,     4,     4,
      16,     0,    10,     6,    10,    18,    18,    15,    21,    21,
      21,    16,     5,    17,    63,    20,    27,    22,     6,    21,
      31,    60,    33,    34,    22,    20,    37,    30,    31,    32,
       4,     6,     6,     5,     4,    17,    18,    11,     4,    21,
       9,    12,    16,     4,    55,    11,    20,     6,    22,    60,
      16,     6,    32,    64,    20,    16,    22,    60,    34,    20,
      60,    22,    63,    23,    37,    33,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    41
  };

  const unsigned char
  parser::yystos_[] =
  {
       0,     4,    16,    20,    22,    25,    26,    27,    28,    31,
      32,    33,    38,    44,    45,    32,    34,    44,    32,    39,
      40,    16,    41,    42,     0,     0,    27,    18,    21,    43,
      46,     4,    10,    15,     6,     5,    17,     6,    32,    41,
      32,    44,    11,    29,    30,    31,    32,    35,    28,    44,
      32,    35,    34,    39,    17,    46,    36,    37,    45,     5,
       6,    32,    12,     6,     9,    29,    36,    32
  };

  const unsigned char
  parser::yyr1_[] =
  {
       0,    24,    25,    26,    26,    27,    27,    28,    29,    29,
      29,    30,    30,    30,    30,    31,    31,    32,    32,    32,
      32,    32,    32,    32,    33,    34,    34,    34,    35,    36,
      36,    36,    37,    38,    38,    39,    39,    39,    40,    41,
      41,    42,    43,    44,    44,    45,    46
  };

  const unsigned char
  parser::yyr2_[] =
  {
       0,     2,     2,     1,     2,     1,     1,     4,     1,     2,
       3,     0,     1,     1,     1,     3,     3,     1,     3,     1,
       1,     3,     1,     1,     3,     0,     1,     3,     3,     0,
       1,     3,     3,     3,     2,     0,     1,     3,     1,     1,
       2,     3,     4,     1,     2,     1,     1
  };



  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a yyntokens_, nonterminals.
  const char*
  const parser::yytname_[] =
  {
  "\"end of file\"", "error", "$undefined", "ERROR_SYMBOL", "L_BRACKET",
  "R_BRACKET", "COMMA", "QUOTE", "SEMICOLON", "COLON", "POINT",
  "L_CURLY_BRACKET", "R_CURLY_BRACKET", "SHARP", "R_ARROW", "EQUAL",
  "L_SQUARE_BRACKET", "R_SQUARE_BRACKET", "PLUS", "CLASS", "STRING",
  "KEYWORD", "ID", "INTEGER", "$accept", "file", "statements", "statement",
  "function_call", "parameters", "parameter", "variable_decl", "expr",
  "tuple", "tuple_values", "kv_map", "kv_map_values", "kv_map_value",
  "array", "array_contents", "array_content", "array_subscripts",
  "array_subscript", "for_op", "identifier", "string", "keyword", YY_NULLPTR
  };

#if YY_BAZELDEBUG
  const unsigned char
  parser::yyrline_[] =
  {
       0,    69,    69,    73,    75,    84,    89,    93,   103,   105,
     107,   115,   116,   118,   124,   132,   134,   138,   144,   150,
     156,   158,   169,   176,   185,   189,   190,   191,   194,   197,
     198,   199,   202,   205,   207,   211,   212,   214,   221,   225,
     226,   229,   232,   235,   237,   240,   243
  };

  // Print the state stack on the debug stream.
  void
  parser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << i->state;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  parser::yy_reduce_print_ (int yyrule)
  {
    unsigned int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):" << std::endl;
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YY_BAZELDEBUG



} // yy_bazel




void yy_bazel::parser::error(const location_type& l, const std::string& m)
{
    driver.error(l, m);
}
