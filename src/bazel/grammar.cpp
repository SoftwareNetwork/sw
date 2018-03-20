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
      case 28: // statements
        value.move< bazel::File > (that.value);
        break;

      case 29: // statement
      case 31: // function_call
        value.move< bazel::Function > (that.value);
        break;

      case 33: // parameter
      case 34: // variable_decl
        value.move< bazel::Parameter > (that.value);
        break;

      case 32: // parameters
        value.move< bazel::Parameters > (that.value);
        break;

      case 36: // expr
      case 37: // tuple
      case 42: // array
      case 43: // array_contents
      case 44: // array_content
        value.move< bazel::Values > (that.value);
        break;

      case 25: // INTEGER
        value.move< int > (that.value);
        break;

      case 22: // STRING
      case 23: // KEYWORD
      case 24: // ID
      case 48: // identifier
      case 49: // string
      case 50: // keyword
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
      case 28: // statements
        value.copy< bazel::File > (that.value);
        break;

      case 29: // statement
      case 31: // function_call
        value.copy< bazel::Function > (that.value);
        break;

      case 33: // parameter
      case 34: // variable_decl
        value.copy< bazel::Parameter > (that.value);
        break;

      case 32: // parameters
        value.copy< bazel::Parameters > (that.value);
        break;

      case 36: // expr
      case 37: // tuple
      case 42: // array
      case 43: // array_contents
      case 44: // array_content
        value.copy< bazel::Values > (that.value);
        break;

      case 25: // INTEGER
        value.copy< int > (that.value);
        break;

      case 22: // STRING
      case 23: // KEYWORD
      case 24: // ID
      case 48: // identifier
      case 49: // string
      case 50: // keyword
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
      case 28: // statements
        yylhs.value.build< bazel::File > ();
        break;

      case 29: // statement
      case 31: // function_call
        yylhs.value.build< bazel::Function > ();
        break;

      case 33: // parameter
      case 34: // variable_decl
        yylhs.value.build< bazel::Parameter > ();
        break;

      case 32: // parameters
        yylhs.value.build< bazel::Parameters > ();
        break;

      case 36: // expr
      case 37: // tuple
      case 42: // array
      case 43: // array_contents
      case 44: // array_content
        yylhs.value.build< bazel::Values > ();
        break;

      case 25: // INTEGER
        yylhs.value.build< int > ();
        break;

      case 22: // STRING
      case 23: // KEYWORD
      case 24: // ID
      case 48: // identifier
      case 49: // string
      case 50: // keyword
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
        // ?
        yylhs.value.as< bazel::Function > ().name = yystack_[0].value.as< bazel::Parameter > ().name;
        yylhs.value.as< bazel::Function > ().parameters.push_back(yystack_[0].value.as< bazel::Parameter > ());
        // global vars
        driver.bazel_file.parameters[yystack_[0].value.as< bazel::Parameter > ().name] = yystack_[0].value.as< bazel::Parameter > ();
    }

    break;

  case 6:

    { /*$$ = $1;*/ }

    break;

  case 7:

    { /*$$ = $1;*/ }

    break;

  case 9:

    {
		bazel::Function f;
		f.name = yystack_[3].value.as< std::string > ();
        f.parameters = yystack_[1].value.as< bazel::Parameters > ();
		yylhs.value.as< bazel::Function > () = f;
        driver.bazel_file.functions.push_back(f);
	}

    break;

  case 10:

    { yylhs.value.as< bazel::Parameters > ().push_back(yystack_[0].value.as< bazel::Parameter > ()); }

    break;

  case 11:

    { yylhs.value.as< bazel::Parameters > ().push_back(yystack_[1].value.as< bazel::Parameter > ()); }

    break;

  case 12:

    {
        yystack_[0].value.as< bazel::Parameters > ().push_back(yystack_[2].value.as< bazel::Parameter > ());
        yylhs.value.as< bazel::Parameters > () = std::move(yystack_[0].value.as< bazel::Parameters > ());
    }

    break;

  case 13:

    {}

    break;

  case 14:

    { yylhs.value.as< bazel::Parameter > () = yystack_[0].value.as< bazel::Parameter > (); }

    break;

  case 15:

    {
        bazel::Parameter p;
        p.values = yystack_[0].value.as< bazel::Values > ();
        yylhs.value.as< bazel::Parameter > () = p;
    }

    break;

  case 16:

    {
        bazel::Parameter p;
        p.name = "kv_map";
        yylhs.value.as< bazel::Parameter > () = p;
    }

    break;

  case 17:

    { yylhs.value.as< bazel::Parameter > () = bazel::Parameter{ yystack_[2].value.as< std::string > (), yystack_[0].value.as< bazel::Values > () }; }

    break;

  case 18:

    { yylhs.value.as< bazel::Parameter > () = bazel::Parameter{ yystack_[2].value.as< std::string > () }; }

    break;

  case 19:

    {}

    break;

  case 20:

    {}

    break;

  case 21:

    {
        bazel::Values v;
        v.insert(yystack_[0].value.as< std::string > ());
        yylhs.value.as< bazel::Values > () = v;
    }

    break;

  case 22:

    {
        bazel::Values v;
        v.insert(yystack_[2].value.as< std::string > ());
        yylhs.value.as< bazel::Values > () = v;
    }

    break;

  case 23:

    {
        bazel::Values v;
        v.insert(yystack_[0].value.as< std::string > ());
        yylhs.value.as< bazel::Values > () = v;
    }

    break;

  case 24:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ()); }

    break;

  case 25:

    {
        yystack_[2].value.as< bazel::Values > ().clear();
        yystack_[2].value.as< bazel::Values > ().insert(yystack_[0].value.as< bazel::Values > ().begin(), yystack_[0].value.as< bazel::Values > ().end());
        yylhs.value.as< bazel::Values > () = std::move(yystack_[2].value.as< bazel::Values > ());
    }

    break;

  case 26:

    {
        yystack_[2].value.as< bazel::Values > ().insert(yystack_[0].value.as< bazel::Values > ().begin(), yystack_[0].value.as< bazel::Values > ().end());
        yylhs.value.as< bazel::Values > () = std::move(yystack_[2].value.as< bazel::Values > ());
    }

    break;

  case 27:

    {
        bazel::Values v;
        //v.insert("fcall");
        yylhs.value.as< bazel::Values > () = v;
        //$$ = $1;
    }

    break;

  case 29:

    { /*$$ = $1;*/ }

    break;

  case 30:

    { /*$$ = $1;*/ }

    break;

  case 31:

    {}

    break;

  case 40:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[1].value.as< bazel::Values > ()); }

    break;

  case 42:

    {}

    break;

  case 43:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ()); }

    break;

  case 44:

    {
        yystack_[0].value.as< bazel::Values > ().insert(yystack_[2].value.as< bazel::Values > ().begin(), yystack_[2].value.as< bazel::Values > ().end());
        yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ());
    }

    break;

  case 45:

    { yylhs.value.as< bazel::Values > () = std::move(yystack_[0].value.as< bazel::Values > ()); }

    break;

  case 50:

    { yylhs.value.as< std::string > () = yystack_[0].value.as< std::string > (); }

    break;

  case 51:

    { yylhs.value.as< std::string > () = yystack_[1].value.as< std::string > (); }

    break;

  case 52:

    { yylhs.value.as< std::string > () = yystack_[0].value.as< std::string > (); }

    break;

  case 53:

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


  const signed char parser::yypact_ninf_ = -50;

  const signed char parser::yytable_ninf_ = -14;

  const signed char
  parser::yypact_[] =
  {
      89,   117,   117,   -18,   -50,   -50,    -9,     8,    56,   -50,
     -50,   -50,   -50,    27,   -50,   -50,     5,   -50,   117,     6,
      11,    31,    27,    13,    17,    16,    30,   117,   -50,    -9,
     -50,   -50,   -50,   117,   117,   -50,   -18,   103,   -18,   103,
      -4,   117,   -50,   -50,   117,   117,    51,   -50,    27,    27,
      28,    32,    47,    55,   -50,    27,   -50,   -50,    27,   -50,
     -50,   -50,   -50,   112,    27,   -50,   117,    41,    57,    62,
     -50,    80,   -50,    27,    27,   -50,    32,   117,   -50,   -50,
      27
  };

  const unsigned char
  parser::yydefact_[] =
  {
       0,    32,    42,     0,    52,    53,    50,     0,     0,     3,
       7,    27,     5,     6,    28,    24,    21,    23,     0,    33,
       0,    21,    45,     0,    43,     0,     0,     0,    51,    46,
       1,     2,     4,     0,     0,    41,     0,    13,     0,     0,
      29,    32,    31,    40,    42,     0,     0,    47,    25,    26,
       0,    36,     0,    10,    14,    15,    16,    22,    17,    18,
      30,    34,    44,     0,    19,    48,     0,     0,    37,     0,
       9,    11,     8,    20,    49,    35,    36,     0,    12,    38,
      39
  };

  const signed char
  parser::yypgoto_[] =
  {
     -50,   -50,   -50,    69,   -50,     1,    14,   -50,   -34,   -50,
      -1,   -50,    46,    49,    18,   -50,   -50,    48,   -50,    61,
     -50,   -50,    10,   -49,     9
  };

  const signed char
  parser::yydefgoto_[] =
  {
      -1,     7,     8,     9,    10,    11,    52,    53,    12,    63,
      13,    14,    20,    56,    67,    68,    15,    23,    24,    28,
      29,    35,    21,    17,    18
  };

  const signed char
  parser::yytable_[] =
  {
      19,    22,    69,    54,    25,    60,     6,    27,    30,    37,
      16,    33,    41,    26,    34,    38,    42,    40,    16,     5,
      39,    33,    36,    44,    34,    45,    46,    69,    36,     5,
      43,    36,    48,    49,    37,    37,    55,    54,    58,    57,
      19,    38,    33,    22,    64,    34,    50,    16,    26,    36,
       5,     5,    70,    75,     4,    36,    31,    36,    36,    66,
       1,    71,    73,    76,    36,    74,    33,    36,    65,    34,
      55,    77,     2,    36,     5,     3,    80,    32,     4,     5,
       6,    16,    36,    36,     1,    78,   -13,    61,    59,    36,
      47,    51,    62,     1,    79,     0,     2,     0,     0,     0,
       0,     0,     4,     5,     6,     2,     0,     1,     3,     0,
       0,     4,     5,     6,    51,     0,     1,     0,     0,     2,
       0,     1,     0,     0,     0,     4,     5,     6,     2,     0,
       0,     0,    72,     2,     4,     5,     6,     0,     0,     4,
       5,     6
  };

  const signed char
  parser::yycheck_[] =
  {
       1,     2,    51,    37,     3,     9,    24,    16,     0,     4,
       0,    15,     6,     3,    18,    10,     5,    18,     8,    23,
      15,    15,    13,     6,    18,     9,    27,    76,    19,    23,
      17,    22,    33,    34,     4,     4,    37,    71,    39,    38,
      41,    10,    15,    44,    45,    18,    36,    37,    38,    40,
      23,    23,     5,    12,    22,    46,     0,    48,    49,    50,
       4,     6,    63,     6,    55,    66,    15,    58,    17,    18,
      71,     9,    16,    64,    23,    19,    77,     8,    22,    23,
      24,    71,    73,    74,     4,    71,     6,    41,    39,    80,
      29,    11,    44,     4,    76,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    22,    23,    24,    16,    -1,     4,    19,    -1,
      -1,    22,    23,    24,    11,    -1,     4,    -1,    -1,    16,
      -1,     4,    -1,    -1,    -1,    22,    23,    24,    16,    -1,
      -1,    -1,    20,    16,    22,    23,    24,    -1,    -1,    22,
      23,    24
  };

  const unsigned char
  parser::yystos_[] =
  {
       0,     4,    16,    19,    22,    23,    24,    27,    28,    29,
      30,    31,    34,    36,    37,    42,    48,    49,    50,    36,
      38,    48,    36,    43,    44,    31,    48,    16,    45,    46,
       0,     0,    29,    15,    18,    47,    50,     4,    10,    15,
      36,     6,     5,    17,     6,     9,    36,    45,    36,    36,
      48,    11,    32,    33,    34,    36,    39,    31,    36,    39,
       9,    38,    43,    35,    36,    17,    50,    40,    41,    49,
       5,     6,    20,    36,    36,    12,     6,     9,    32,    40,
      36
  };

  const unsigned char
  parser::yyr1_[] =
  {
       0,    26,    27,    28,    28,    29,    29,    29,    30,    31,
      32,    32,    32,    33,    33,    33,    33,    34,    34,    35,
      35,    36,    36,    36,    36,    36,    36,    36,    36,    36,
      36,    37,    38,    38,    38,    39,    40,    40,    40,    41,
      42,    42,    43,    43,    43,    44,    45,    45,    46,    47,
      48,    48,    49,    50
  };

  const unsigned char
  parser::yyr2_[] =
  {
       0,     2,     2,     1,     2,     1,     1,     1,     5,     4,
       1,     2,     3,     0,     1,     1,     1,     3,     3,     1,
       2,     1,     3,     1,     1,     3,     3,     1,     1,     2,
       3,     3,     0,     1,     3,     3,     0,     1,     3,     3,
       3,     2,     0,     1,     3,     1,     1,     2,     3,     4,
       1,     2,     1,     1
  };



  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a yyntokens_, nonterminals.
  const char*
  const parser::yytname_[] =
  {
  "\"end of file\"", "error", "$undefined", "ERROR_SYMBOL", "L_BRACKET",
  "R_BRACKET", "COMMA", "QUOTE", "SEMICOLON", "COLON", "POINT",
  "L_CURLY_BRACKET", "R_CURLY_BRACKET", "SHARP", "R_ARROW", "EQUAL",
  "L_SQUARE_BRACKET", "R_SQUARE_BRACKET", "PLUS", "DEF", "END_OF_DEF",
  "CLASS", "STRING", "KEYWORD", "ID", "INTEGER", "$accept", "file",
  "statements", "statement", "function_def", "function_call", "parameters",
  "parameter", "variable_decl", "exprs", "expr", "tuple", "tuple_values",
  "kv_map", "kv_map_values", "kv_map_value", "array", "array_contents",
  "array_content", "array_subscripts", "array_subscript", "for_op",
  "identifier", "string", "keyword", YY_NULLPTR
  };

#if YY_BAZELDEBUG
  const unsigned short int
  parser::yyrline_[] =
  {
       0,    69,    69,    73,    75,    84,    92,    94,    98,   100,
     110,   112,   114,   122,   123,   125,   131,   139,   141,   145,
     147,   151,   157,   163,   169,   171,   177,   188,   195,   202,
     204,   208,   212,   213,   214,   217,   220,   221,   222,   225,
     228,   230,   234,   235,   237,   244,   248,   249,   252,   255,
     258,   260,   263,   266
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
