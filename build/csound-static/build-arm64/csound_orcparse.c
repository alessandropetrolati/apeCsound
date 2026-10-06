/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         csound_orcparse
#define yylex           csound_orclex
#define yyerror         csound_orcerror
#define yydebug         csound_orcdebug
#define yynerrs         csound_orcnerrs

/* First part of user prologue.  */
#line 141 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"

/* #define YYSTYPE ORCTOKEN* */
/* JPff thinks that line must be wrong and is trying this! */
#define YYSTYPE TREE*

#ifndef NULL
#define NULL 0L
#endif
#include "csoundCore.h"
#include <ctype.h>
#include <string.h>
#include "namedins.h"

#include "csound_orc.h"
#include "parse_param.h"

#ifdef PARCS
#include "cs_par_base.h"
#include "cs_par_orc_semantics.h"
#else
#define csp_orc_sa_instr_add(a,b)
#define csp_orc_sa_instr_add_tree(a,b)
#define csp_orc_sa_instr_finalize(a)
#define csp_orc_sa_global_read_write_add_list(a,b,c)
#define csp_orc_sa_globals_find(a,b)
#define csp_orc_sa_global_read_write_add_list1(a,b,c)
#define csp_orc_sa_interlocks(a, b)
#define csp_orc_sa_global_read_add_list(a,b)
#define csp_orc_sa_global_write_add_list(a,b);
#endif

#define namedInstrFlag csound->parserNamedInstrFlag

    extern TREE* parser_append(CSOUND * csound, TREE *first, TREE *newlast);
    extern int csound_orclex(TREE**, CSOUND *, void *);
    extern void print_tree(CSOUND *, char *msg, TREE *);
    extern TREE* constant_fold(CSOUND *, TREE *);
    extern void csound_orcerror(PARSE_PARM *, void *, CSOUND *,
                                TREE**, const char*);
    extern ORCTOKEN *lookup_token(CSOUND*,char*,void*);
#define LINE csound_orcget_lineno(scanner)
#define LOCN csound_orcget_locn(scanner)
    extern uint64_t csound_orcget_locn(void *);
    extern int csound_orcget_lineno(void *);
    extern ORCTOKEN *make_string(CSOUND *, char *);
    extern char* UNARY_PLUS;
    extern TREE* make_opcall_from_func_start(CSOUND*, int32_t, uint64_t, int32_t, TREE*, TREE*);
    extern void add_instr_variable(CSOUND *csound,  TREE *x);

#line 126 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "csound_orcparse.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_NEWLINE = 3,                    /* NEWLINE  */
  YYSYMBOL_S_NEQ = 4,                      /* S_NEQ  */
  YYSYMBOL_S_AND = 5,                      /* S_AND  */
  YYSYMBOL_S_OR = 6,                       /* S_OR  */
  YYSYMBOL_S_LT = 7,                       /* S_LT  */
  YYSYMBOL_S_LE = 8,                       /* S_LE  */
  YYSYMBOL_S_EQ = 9,                       /* S_EQ  */
  YYSYMBOL_S_ADDIN = 10,                   /* S_ADDIN  */
  YYSYMBOL_S_SUBIN = 11,                   /* S_SUBIN  */
  YYSYMBOL_S_MULIN = 12,                   /* S_MULIN  */
  YYSYMBOL_S_DIVIN = 13,                   /* S_DIVIN  */
  YYSYMBOL_S_GT = 14,                      /* S_GT  */
  YYSYMBOL_S_GE = 15,                      /* S_GE  */
  YYSYMBOL_S_BITSHIFT_LEFT = 16,           /* S_BITSHIFT_LEFT  */
  YYSYMBOL_S_BITSHIFT_RIGHT = 17,          /* S_BITSHIFT_RIGHT  */
  YYSYMBOL_LABEL_TOKEN = 18,               /* LABEL_TOKEN  */
  YYSYMBOL_IF_TOKEN = 19,                  /* IF_TOKEN  */
  YYSYMBOL_DECLARE_TOKEN = 20,             /* DECLARE_TOKEN  */
  YYSYMBOL_UDO_TOKEN = 21,                 /* UDO_TOKEN  */
  YYSYMBOL_UDOSTART_DEFINITION = 22,       /* UDOSTART_DEFINITION  */
  YYSYMBOL_UDOEND_TOKEN = 23,              /* UDOEND_TOKEN  */
  YYSYMBOL_UDO_ANS_TOKEN = 24,             /* UDO_ANS_TOKEN  */
  YYSYMBOL_UDO_ARGS_TOKEN = 25,            /* UDO_ARGS_TOKEN  */
  YYSYMBOL_UDO_IDENT = 26,                 /* UDO_IDENT  */
  YYSYMBOL_VOID_TOKEN = 27,                /* VOID_TOKEN  */
  YYSYMBOL_ERROR_TOKEN = 28,               /* ERROR_TOKEN  */
  YYSYMBOL_T_OPCALL = 29,                  /* T_OPCALL  */
  YYSYMBOL_T_FUNCTION = 30,                /* T_FUNCTION  */
  YYSYMBOL_T_ASSIGNMENT = 31,              /* T_ASSIGNMENT  */
  YYSYMBOL_STRUCT_TOKEN = 32,              /* STRUCT_TOKEN  */
  YYSYMBOL_INSTR_TOKEN = 33,               /* INSTR_TOKEN  */
  YYSYMBOL_ENDIN_TOKEN = 34,               /* ENDIN_TOKEN  */
  YYSYMBOL_GOTO_TOKEN = 35,                /* GOTO_TOKEN  */
  YYSYMBOL_KGOTO_TOKEN = 36,               /* KGOTO_TOKEN  */
  YYSYMBOL_IGOTO_TOKEN = 37,               /* IGOTO_TOKEN  */
  YYSYMBOL_STRING_TOKEN = 38,              /* STRING_TOKEN  */
  YYSYMBOL_T_IDENT = 39,                   /* T_IDENT  */
  YYSYMBOL_T_IDENTB = 40,                  /* T_IDENTB  */
  YYSYMBOL_T_TYPED_IDENT = 41,             /* T_TYPED_IDENT  */
  YYSYMBOL_T_TYPED_IDENTB = 42,            /* T_TYPED_IDENTB  */
  YYSYMBOL_T_MEMBER_IDENT = 43,            /* T_MEMBER_IDENT  */
  YYSYMBOL_T_PLUS_IDENT = 44,              /* T_PLUS_IDENT  */
  YYSYMBOL_INTEGER_TOKEN = 45,             /* INTEGER_TOKEN  */
  YYSYMBOL_NUMBER_TOKEN = 46,              /* NUMBER_TOKEN  */
  YYSYMBOL_THEN_TOKEN = 47,                /* THEN_TOKEN  */
  YYSYMBOL_ITHEN_TOKEN = 48,               /* ITHEN_TOKEN  */
  YYSYMBOL_KTHEN_TOKEN = 49,               /* KTHEN_TOKEN  */
  YYSYMBOL_ELSEIF_TOKEN = 50,              /* ELSEIF_TOKEN  */
  YYSYMBOL_ELSE_TOKEN = 51,                /* ELSE_TOKEN  */
  YYSYMBOL_ENDIF_TOKEN = 52,               /* ENDIF_TOKEN  */
  YYSYMBOL_UNTIL_TOKEN = 53,               /* UNTIL_TOKEN  */
  YYSYMBOL_WHILE_TOKEN = 54,               /* WHILE_TOKEN  */
  YYSYMBOL_DO_TOKEN = 55,                  /* DO_TOKEN  */
  YYSYMBOL_OD_TOKEN = 56,                  /* OD_TOKEN  */
  YYSYMBOL_BREAK_TOKEN = 57,               /* BREAK_TOKEN  */
  YYSYMBOL_CONTINUE_TOKEN = 58,            /* CONTINUE_TOKEN  */
  YYSYMBOL_SWITCH_TOKEN = 59,              /* SWITCH_TOKEN  */
  YYSYMBOL_CASE_TOKEN = 60,                /* CASE_TOKEN  */
  YYSYMBOL_DEFAULT_TOKEN = 61,             /* DEFAULT_TOKEN  */
  YYSYMBOL_ENDSW_TOKEN = 62,               /* ENDSW_TOKEN  */
  YYSYMBOL_FOR_TOKEN = 63,                 /* FOR_TOKEN  */
  YYSYMBOL_IN_TOKEN = 64,                  /* IN_TOKEN  */
  YYSYMBOL_TRUE_TOKEN = 65,                /* TRUE_TOKEN  */
  YYSYMBOL_FALSE_TOKEN = 66,               /* FALSE_TOKEN  */
  YYSYMBOL_TRUEK_TOKEN = 67,               /* TRUEK_TOKEN  */
  YYSYMBOL_FALSEK_TOKEN = 68,              /* FALSEK_TOKEN  */
  YYSYMBOL_S_ELIPSIS = 69,                 /* S_ELIPSIS  */
  YYSYMBOL_S_ELIPSIS2 = 70,                /* S_ELIPSIS2  */
  YYSYMBOL_T_ARRAY = 71,                   /* T_ARRAY  */
  YYSYMBOL_T_ARRAY_IDENT = 72,             /* T_ARRAY_IDENT  */
  YYSYMBOL_T_DECLARE = 73,                 /* T_DECLARE  */
  YYSYMBOL_STRUCT_EXPR = 74,               /* STRUCT_EXPR  */
  YYSYMBOL_T_MAPI = 75,                    /* T_MAPI  */
  YYSYMBOL_T_MAPK = 76,                    /* T_MAPK  */
  YYSYMBOL_77_ = 77,                       /* '?'  */
  YYSYMBOL_78_ = 78,                       /* '='  */
  YYSYMBOL_79_ = 79,                       /* '|'  */
  YYSYMBOL_80_ = 80,                       /* '#'  */
  YYSYMBOL_81_ = 81,                       /* '&'  */
  YYSYMBOL_82_ = 82,                       /* '+'  */
  YYSYMBOL_83_ = 83,                       /* '-'  */
  YYSYMBOL_84_ = 84,                       /* '*'  */
  YYSYMBOL_85_ = 85,                       /* '/'  */
  YYSYMBOL_86_ = 86,                       /* '%'  */
  YYSYMBOL_87_ = 87,                       /* '^'  */
  YYSYMBOL_S_UNOT = 88,                    /* S_UNOT  */
  YYSYMBOL_S_UMINUS = 89,                  /* S_UMINUS  */
  YYSYMBOL_S_UPLUS = 90,                   /* S_UPLUS  */
  YYSYMBOL_S_GOTO = 91,                    /* S_GOTO  */
  YYSYMBOL_T_HIGHEST = 92,                 /* T_HIGHEST  */
  YYSYMBOL_93_ = 93,                       /* ','  */
  YYSYMBOL_94_ = 94,                       /* ':'  */
  YYSYMBOL_95_ = 95,                       /* '('  */
  YYSYMBOL_96_ = 96,                       /* ')'  */
  YYSYMBOL_97_ = 97,                       /* '['  */
  YYSYMBOL_98_ = 98,                       /* ']'  */
  YYSYMBOL_99_ = 99,                       /* '.'  */
  YYSYMBOL_100_ = 100,                     /* '~'  */
  YYSYMBOL_101_ = 101,                     /* '!'  */
  YYSYMBOL_YYACCEPT = 102,                 /* $accept  */
  YYSYMBOL_orcfile = 103,                  /* orcfile  */
  YYSYMBOL_root_statement_list = 104,      /* root_statement_list  */
  YYSYMBOL_root_statement = 105,           /* root_statement  */
  YYSYMBOL_struct_definition = 106,        /* struct_definition  */
  YYSYMBOL_struct_arg_list = 107,          /* struct_arg_list  */
  YYSYMBOL_struct_arg = 108,               /* struct_arg  */
  YYSYMBOL_instr_definition = 109,         /* instr_definition  */
  YYSYMBOL_110_1 = 110,                    /* $@1  */
  YYSYMBOL_instr_id_list = 111,            /* instr_id_list  */
  YYSYMBOL_instr_id = 112,                 /* instr_id  */
  YYSYMBOL_udo_definition = 113,           /* udo_definition  */
  YYSYMBOL_udo_arg_list = 114,             /* udo_arg_list  */
  YYSYMBOL_udo_out_arg_list = 115,         /* udo_out_arg_list  */
  YYSYMBOL_out_type_list = 116,            /* out_type_list  */
  YYSYMBOL_out_type = 117,                 /* out_type  */
  YYSYMBOL_opcall = 118,                   /* opcall  */
  YYSYMBOL_function_call = 119,            /* function_call  */
  YYSYMBOL_statement_list = 120,           /* statement_list  */
  YYSYMBOL_statement = 121,                /* statement  */
  YYSYMBOL_if_goto = 122,                  /* if_goto  */
  YYSYMBOL_if_then = 123,                  /* if_then  */
  YYSYMBOL_if_then_base = 124,             /* if_then_base  */
  YYSYMBOL_elseif_list = 125,              /* elseif_list  */
  YYSYMBOL_elseif = 126,                   /* elseif  */
  YYSYMBOL_until = 127,                    /* until  */
  YYSYMBOL_while = 128,                    /* while  */
  YYSYMBOL_case = 129,                     /* case  */
  YYSYMBOL_case_list = 130,                /* case_list  */
  YYSYMBOL_switch = 131,                   /* switch  */
  YYSYMBOL_for_in = 132,                   /* for_in  */
  YYSYMBOL_declare_definition = 133,       /* declare_definition  */
  YYSYMBOL_expr_list = 134,                /* expr_list  */
  YYSYMBOL_expr = 135,                     /* expr  */
  YYSYMBOL_gen_array = 136,                /* gen_array  */
  YYSYMBOL_slice_array = 137,              /* slice_array  */
  YYSYMBOL_static_array = 138,             /* static_array  */
  YYSYMBOL_array_expr = 139,               /* array_expr  */
  YYSYMBOL_struct_expr = 140,              /* struct_expr  */
  YYSYMBOL_ternary_expr = 141,             /* ternary_expr  */
  YYSYMBOL_unary_expr = 142,               /* unary_expr  */
  YYSYMBOL_binary_expr = 143,              /* binary_expr  */
  YYSYMBOL_out_arg_list = 144,             /* out_arg_list  */
  YYSYMBOL_out_arg = 145,                  /* out_arg  */
  YYSYMBOL_out_arg_list_array = 146,       /* out_arg_list_array  */
  YYSYMBOL_array_identifier = 147,         /* array_identifier  */
  YYSYMBOL_assignment = 148,               /* assignment  */
  YYSYMBOL_assignment_array = 149,         /* assignment_array  */
  YYSYMBOL_in = 150,                       /* in  */
  YYSYMBOL_then = 151,                     /* then  */
  YYSYMBOL_goto = 152,                     /* goto  */
  YYSYMBOL_optnewline = 153,               /* optnewline  */
  YYSYMBOL_string = 154,                   /* string  */
  YYSYMBOL_false_const = 155,              /* false_const  */
  YYSYMBOL_true_const = 156,               /* true_const  */
  YYSYMBOL_number = 157,                   /* number  */
  YYSYMBOL_integer = 158,                  /* integer  */
  YYSYMBOL_plus_identifier = 159,          /* plus_identifier  */
  YYSYMBOL_typed_identifier = 160,         /* typed_identifier  */
  YYSYMBOL_typed_identifierb = 161,        /* typed_identifierb  */
  YYSYMBOL_identifier = 162,               /* identifier  */
  YYSYMBOL_identifierb = 163               /* identifierb  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   4095

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  102
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  62
/* YYNRULES -- Number of rules.  */
#define YYNRULES  235
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  487

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   336


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   101,     2,    80,     2,    86,    81,     2,
      95,    96,    84,    82,    93,    83,    99,    85,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    94,     2,
       2,    78,     2,    77,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    97,     2,    98,    87,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    79,     2,   100,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    88,    89,    90,    91,    92
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   200,   200,   214,   217,   220,   221,   222,   223,   224,
     229,   233,   235,   237,   240,   241,   242,   245,   244,   252,
     260,   262,   267,   268,   269,   273,   298,   309,   321,   335,
     337,   339,   343,   345,   347,   349,   352,   354,   357,   358,
     367,   372,   377,   382,   389,   401,   406,   411,   418,   430,
     432,   434,   436,   438,   440,   442,   444,   446,   448,   452,
     456,   459,   463,   468,   472,   473,   483,   498,   513,   514,
     521,   522,   523,   524,   525,   526,   527,   529,   531,   533,
     539,   548,   550,   553,   556,   570,   575,   583,   586,   591,
     597,   603,   609,   616,   623,   624,   630,   638,   644,   651,
     659,   669,   679,   689,   698,   700,   702,   705,   706,   708,
     709,   710,   711,   712,   713,   714,   715,   716,   717,   718,
     719,   720,   721,   722,   723,   727,   734,   741,   750,   756,
     763,   772,   779,   787,   792,   798,   804,   817,   831,   836,
     850,   854,   867,   870,   871,   872,   875,   877,   878,   880,
     881,   885,   886,   908,   911,   912,   913,   914,   915,   916,
     917,   918,   919,   920,   922,   923,   924,   925,   926,   927,
     928,   929,   930,   931,   932,   933,   934,   935,   936,   937,
     938,   939,   940,   941,   942,   943,   944,   945,   946,   947,
     948,   950,   951,   953,   957,   959,   962,   963,   964,   965,
     968,   970,   973,   978,   983,  1007,  1009,  1011,  1013,  1015,
    1020,  1022,  1026,  1030,  1034,  1040,  1043,  1045,  1047,  1051,
    1053,  1055,  1059,  1060,  1063,  1067,  1070,  1076,  1079,  1085,
    1089,  1097,  1103,  1107,  1111,  1115
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "NEWLINE", "S_NEQ",
  "S_AND", "S_OR", "S_LT", "S_LE", "S_EQ", "S_ADDIN", "S_SUBIN", "S_MULIN",
  "S_DIVIN", "S_GT", "S_GE", "S_BITSHIFT_LEFT", "S_BITSHIFT_RIGHT",
  "LABEL_TOKEN", "IF_TOKEN", "DECLARE_TOKEN", "UDO_TOKEN",
  "UDOSTART_DEFINITION", "UDOEND_TOKEN", "UDO_ANS_TOKEN", "UDO_ARGS_TOKEN",
  "UDO_IDENT", "VOID_TOKEN", "ERROR_TOKEN", "T_OPCALL", "T_FUNCTION",
  "T_ASSIGNMENT", "STRUCT_TOKEN", "INSTR_TOKEN", "ENDIN_TOKEN",
  "GOTO_TOKEN", "KGOTO_TOKEN", "IGOTO_TOKEN", "STRING_TOKEN", "T_IDENT",
  "T_IDENTB", "T_TYPED_IDENT", "T_TYPED_IDENTB", "T_MEMBER_IDENT",
  "T_PLUS_IDENT", "INTEGER_TOKEN", "NUMBER_TOKEN", "THEN_TOKEN",
  "ITHEN_TOKEN", "KTHEN_TOKEN", "ELSEIF_TOKEN", "ELSE_TOKEN",
  "ENDIF_TOKEN", "UNTIL_TOKEN", "WHILE_TOKEN", "DO_TOKEN", "OD_TOKEN",
  "BREAK_TOKEN", "CONTINUE_TOKEN", "SWITCH_TOKEN", "CASE_TOKEN",
  "DEFAULT_TOKEN", "ENDSW_TOKEN", "FOR_TOKEN", "IN_TOKEN", "TRUE_TOKEN",
  "FALSE_TOKEN", "TRUEK_TOKEN", "FALSEK_TOKEN", "S_ELIPSIS", "S_ELIPSIS2",
  "T_ARRAY", "T_ARRAY_IDENT", "T_DECLARE", "STRUCT_EXPR", "T_MAPI",
  "T_MAPK", "'?'", "'='", "'|'", "'#'", "'&'", "'+'", "'-'", "'*'", "'/'",
  "'%'", "'^'", "S_UNOT", "S_UMINUS", "S_UPLUS", "S_GOTO", "T_HIGHEST",
  "','", "':'", "'('", "')'", "'['", "']'", "'.'", "'~'", "'!'", "$accept",
  "orcfile", "root_statement_list", "root_statement", "struct_definition",
  "struct_arg_list", "struct_arg", "instr_definition", "$@1",
  "instr_id_list", "instr_id", "udo_definition", "udo_arg_list",
  "udo_out_arg_list", "out_type_list", "out_type", "opcall",
  "function_call", "statement_list", "statement", "if_goto", "if_then",
  "if_then_base", "elseif_list", "elseif", "until", "while", "case",
  "case_list", "switch", "for_in", "declare_definition", "expr_list",
  "expr", "gen_array", "slice_array", "static_array", "array_expr",
  "struct_expr", "ternary_expr", "unary_expr", "binary_expr",
  "out_arg_list", "out_arg", "out_arg_list_array", "array_identifier",
  "assignment", "assignment_array", "in", "then", "goto", "optnewline",
  "string", "false_const", "true_const", "number", "integer",
  "plus_identifier", "typed_identifier", "typed_identifierb", "identifier",
  "identifierb", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-379)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-224)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -379,    31,   630,  -379,  -379,  -379,  3541,    12,    12,    12,
     130,  -379,  -379,  -379,  -379,  -379,  -379,  -379,  3541,  3541,
    -379,  -379,  3541,    90,  -379,  -379,  -379,  -379,  -379,   124,
    -379,  -379,  -379,   209,  -379,  -379,  -379,  -379,  -379,    77,
     172,  3236,  -379,  3282,    -8,    12,     9,  3328,     8,  3367,
    -379,  -379,  -379,  -379,  -379,  -379,  -379,  1882,  1928,  1974,
    3541,  2020,  2066,    85,  2714,  -379,  -379,  -379,    77,   172,
    -379,  -379,  -379,  -379,  -379,  -379,  -379,  -379,   178,     0,
     -17,    90,   140,   155,    10,  -379,  -379,  -379,  -379,  2808,
    2844,  2492,   -40,   -20,  -379,  3541,  3541,  3541,  3541,  3541,
    3541,  3541,  3541,  3541,  3541,  3541,  4032,   210,   233,  -379,
    3541,    12,  3541,   159,  -379,  -379,  -379,  -379,  -379,   116,
      18,    15,  3158,  3541,  2205,  -379,  -379,  -379,  -379,  -379,
     159,   490,    17,  3541,  2251,   141,   221,   146,  -379,   -41,
    -379,  3376,   159,  -379,   121,  -379,   143,  -379,   143,  -379,
     949,   -39,  2865,  -379,   128,  -379,   128,   790,   976,  1046,
    1092,  1139,  1185,  1232,  1278,  1325,  1371,  -379,  -379,  -379,
    2112,  1418,  1464,  1511,  1557,  1604,  1650,  1697,  1743,  1790,
    1836,  3415,   229,   199,  3454,   145,   163,   238,    -1,   161,
    -379,    -8,     9,   171,  -379,  -379,  -379,   134,  4032,  4032,
      20,  -379,    90,  3541,    90,  3541,    19,    23,    27,    29,
      30,    32,    33,    37,    39,  2532,  2760,  3616,  -379,  -379,
    4032,   277,  -379,  2576,  -379,  2617,    85,   189,   197,    77,
    -379,   194,   294,  -379,  2298,    42,  -379,  3454,    44,   189,
     197,   295,  -379,    46,  -379,    58,  -379,  -379,  -379,  -379,
    -379,  2661,   189,   197,  -379,  -379,  -379,  -379,  3541,  -379,
    -379,  3541,  -379,  3541,  -379,  3541,  -379,  3541,  -379,  3541,
    -379,  3541,  -379,  3541,  -379,  3541,  -379,  3541,  -379,  3541,
    -379,  2353,  -379,  3541,  -379,  3541,  -379,  3541,  -379,  3541,
    -379,  3541,  -379,  3541,  -379,  3541,  -379,  3541,  -379,  3541,
    -379,  3541,  3541,  2902,  4032,   298,  3541,  2437,  -379,   126,
     127,    97,   213,   214,     7,    26,   212,  4032,  -379,  1062,
    3641,    20,  3541,   304,  -379,   228,   247,   247,  2949,   247,
     247,  2986,  -379,  -379,  -379,  -379,  -379,  -379,  -379,  -379,
    -379,  -379,   313,   316,  -379,  3683,  -379,  -379,  -379,  3541,
    -379,  3541,  3158,  -379,  2396,  -379,  -379,  -379,  -379,  -379,
     -27,    81,  3186,  3186,    81,    81,    81,    81,    81,   165,
     165,  -379,  2158,    81,   250,   279,   341,   143,   143,   -59,
     -59,   -59,   128,   -24,  3493,  4032,  -379,   -21,  3532,  -379,
    -379,  -379,   -14,   317,  -379,    -8,     9,   171,   297,    97,
      97,   322,    90,  -379,  3713,  -379,  -379,   129,    59,  4032,
    -379,  -379,  3541,  3541,  4032,  3541,  3541,  4032,  4032,  -379,
     323,  3158,  -379,  -379,  3158,  -379,  -379,    -6,  -379,  -379,
      79,  -379,   160,  -379,  -379,   324,   325,   326,  4032,  -379,
     335,  4032,  4032,  3007,  3043,  3742,  3101,  3137,  3771,  4032,
    -379,  -379,  -379,    90,  -379,  4032,  4032,  4032,  3800,  -379,
    4032,  4032,  4032,  -379,  4032,  4032,  -379,  -379,  3829,  3858,
    3887,   336,  3916,  3945,  3974,  4003,   337,   338,   342,  -379,
    -379,  -379,  -379,  -379,  -379,  -379,  -379
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       4,     0,     2,     1,    79,    78,     0,     0,     0,     0,
       0,   219,   220,   221,   234,   235,   232,   233,     0,     0,
      76,    77,     0,     0,     3,     8,     6,     7,    68,     0,
       5,    70,    71,     0,    72,    73,    74,    75,     9,   201,
     199,     0,   195,     0,   198,     0,   197,     0,   196,     0,
     224,   230,   229,   227,   225,   228,   226,     0,     0,     0,
       0,     0,     0,   107,     0,   119,   121,   120,   118,   122,
     111,   112,   113,   117,   124,   123,   116,   115,   114,     0,
       0,     0,     0,     0,     0,    21,    22,    24,    23,     0,
       0,     0,     0,     0,    49,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    65,     0,     0,    87,
       0,     0,     0,     0,   206,   207,   209,   208,   205,     0,
       0,     0,   106,     0,   114,   211,   212,   214,   213,   210,
       0,     0,     0,     0,   114,     0,     0,     0,    60,     0,
      40,     0,     0,    62,     0,   153,   152,   151,   150,   110,
       0,     0,   106,   147,   146,   149,   148,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   216,   218,   217,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    10,
      13,    16,    15,    14,    19,   231,    17,     0,    65,    65,
       0,   215,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    64,    81,
      65,     0,    86,     0,   138,     0,     0,   137,   136,     0,
     194,   196,     0,    41,     0,     0,    44,     0,     0,   200,
       0,     0,    45,     0,    48,     0,   202,    69,   204,    59,
     203,     0,   139,   141,    61,   109,   108,   132,     0,   163,
     222,     0,   173,     0,   175,     0,   171,     0,   159,     0,
     167,     0,   169,     0,   161,     0,   191,     0,   193,     0,
     145,     0,   165,     0,   185,     0,   189,     0,   187,     0,
     155,     0,   157,     0,   177,     0,   179,     0,   183,     0,
     181,     0,     0,     0,    65,     0,     0,     0,    31,     0,
       0,     0,     0,     0,     0,     0,     0,    65,    20,     0,
       0,     0,     0,     0,    94,     0,     0,     0,     0,     0,
       0,     0,    56,    58,    57,    50,    51,    52,    53,    55,
      54,   135,     0,     0,    63,     0,    83,   133,   140,     0,
      42,     0,   104,    66,   106,    43,    46,    67,    47,   134,
       0,   162,   172,   174,   170,   158,   166,   168,   160,   190,
     192,   144,     0,   164,   184,   188,   186,   154,   156,   176,
     178,   182,   180,     0,     0,    85,    80,     0,     0,    29,
      30,    34,     0,     0,    35,    39,     0,    38,     0,     0,
       0,     0,     0,    11,     0,    89,    90,    95,     0,    65,
      96,    93,     0,     0,    65,     0,     0,    65,    65,    82,
       0,   105,   125,   143,   142,   130,   131,     0,   127,   128,
       0,    33,     0,    37,   103,     0,     0,     0,    65,    12,
       0,    65,    92,     0,     0,     0,     0,     0,     0,    88,
      84,   129,   126,     0,    32,    65,    65,    65,     0,    18,
      91,    65,    65,    99,    65,    65,    97,    36,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    26,
     101,   100,   102,    98,    25,    28,    27
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -379,  -379,  -379,  -379,  -379,  -379,  -311,  -379,  -379,  -379,
     147,  -379,   266,  -246,  -379,  -378,  -379,   157,  -183,    48,
    -379,  -379,  -379,  -379,   240,  -379,  -379,  -308,    38,  -379,
    -379,  -379,    47,   711,  -379,  -379,  -379,   290,   462,  -379,
    -379,  -379,   166,   231,   167,   -78,  -379,  -379,   -84,   138,
     306,   990,  -379,  -379,  -379,  -379,    -9,  -379,   -11,  -379,
      -2,  -379
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,    24,    25,   189,   190,    26,   317,    84,
      85,    27,   186,   393,   432,   394,    28,    63,   217,   218,
      31,    32,    33,   108,   109,    34,    35,   324,   325,    36,
      37,    38,   151,   122,    65,    66,    67,    68,    69,    70,
      71,    72,    41,    42,    43,    44,   123,   133,   203,   182,
      45,   261,    73,    74,    75,    76,    77,    87,    46,    47,
      78,    49
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      48,    86,   313,   191,   403,    79,    80,    81,    88,   205,
     400,   140,    92,   196,   433,   319,   320,   411,   233,   149,
     242,    93,   332,   321,   201,    14,   333,    16,   180,   402,
     334,     3,   335,   336,   391,   337,   338,   345,   181,   124,
     339,   134,   340,   136,   201,   353,    14,   355,    16,   357,
      30,    14,   234,   202,   234,   249,    50,    14,    15,   257,
      17,   358,   441,    51,    52,    14,   234,    16,   401,   234,
     192,   422,   234,   204,   425,   467,   187,   428,   185,   193,
     322,   323,   431,    53,    54,    55,    56,   234,   121,   135,
     132,   439,   451,   314,   139,   185,   144,   165,   166,   411,
      57,    58,   392,   197,    48,   141,   137,   142,   234,   224,
     234,   228,   234,    59,   232,    60,   234,   231,    61,    62,
     234,   385,   234,   234,   391,   234,   234,    94,   240,    14,
     234,    16,   234,    82,   404,   234,    14,   234,    16,   234,
     253,   194,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   234,   234,   436,   437,    14,    15,    16,    17,    29,
     172,   173,   174,   175,   176,   177,   178,   179,   180,    14,
     235,   238,   234,    14,   110,    51,   111,   452,   181,    51,
     243,   245,   104,   231,    14,    15,    16,    17,    86,   322,
     323,   326,   392,   329,   195,    88,    48,    48,    14,    15,
     327,    17,   330,    95,    96,    97,    98,    99,   100,   101,
     102,   103,    83,   219,   234,    48,    83,   254,    48,   119,
     130,   104,   389,   390,   247,   181,   442,   177,   178,   179,
     180,   445,   304,   395,   448,   449,   395,   191,   305,   246,
     181,   308,   412,   413,   248,   415,   416,   175,   176,   177,
     178,   179,   180,   453,   315,   458,   454,   311,   460,   105,
     106,   107,   181,    29,   312,   344,   165,   166,   316,   112,
     226,   113,   468,   469,   470,   184,   226,   142,   472,   473,
     346,   474,   475,   105,   220,   221,   110,   226,   322,   323,
     410,   141,    39,   142,   349,   165,   166,   350,   356,   226,
     396,   386,    48,   396,   192,   360,   398,   409,   399,   397,
     250,   201,   397,   193,   395,    48,   418,    48,    48,   419,
     434,   395,   395,   435,   191,   438,   450,   455,   456,   457,
     173,   174,   175,   176,   177,   178,   179,   180,   459,   479,
     484,   485,   226,    48,   318,   486,   188,   181,   222,   383,
     230,   309,   310,   387,   342,    29,    29,   165,   166,   407,
     174,   175,   176,   177,   178,   179,   180,   344,   344,   408,
     183,     0,     0,     0,    29,   395,   181,    29,     0,     0,
       0,   396,     0,    48,     0,     0,     0,     0,   396,   396,
     397,   192,     0,   344,     0,     0,    39,   397,   397,     0,
     193,     0,    48,   227,     0,     0,     0,    48,     0,   229,
       0,     0,    48,     0,     0,    48,    48,     0,     0,     0,
     239,     0,     0,   175,   176,   177,   178,   179,   180,     0,
       0,   427,   252,   344,     0,   430,    48,     0,   181,    48,
      48,     0,   396,    48,     0,     0,    48,    48,     0,     0,
       0,   397,   344,    48,    48,    48,    48,     0,    48,    48,
      48,    29,    48,    48,    40,     0,    48,    48,    48,     0,
      48,    48,    48,    48,    29,    39,    29,    29,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    39,    39,
     344,   149,     0,   344,     0,     0,   344,   344,     0,     0,
       0,     0,    29,     0,     0,     0,   344,    39,   344,     0,
      39,     0,     0,     0,     0,     0,   344,   344,   344,     0,
     344,   344,   344,   344,     0,     0,     0,     0,    50,    14,
      15,     0,    17,     0,     0,    51,    52,     0,     0,     0,
       0,     0,    29,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,     0,
       0,    29,     0,     0,     0,     0,    29,     0,    40,     0,
       0,    29,    57,    58,    29,    29,     0,     0,     0,     0,
       0,    40,     0,     0,     0,    59,   241,    60,     0,     0,
      61,    62,     0,     0,    39,    29,     0,     0,    29,    29,
       0,     0,    29,     0,     0,    29,    29,    39,     0,    39,
      39,     0,    29,    29,    29,    29,     0,    29,    29,    29,
       0,    29,    29,     0,     0,    29,    29,    29,     0,    29,
      29,    29,    29,     4,     0,    39,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    40,     5,     6,
       7,     0,     8,     0,     0,     0,     0,     0,     0,     0,
      40,    40,     9,    10,     0,    11,    12,    13,     0,    14,
      15,    16,    17,     0,     0,    39,     0,     0,     0,    40,
       0,     0,    40,    18,    19,     0,     0,    20,    21,    22,
       0,     0,     0,    23,    39,     0,     0,     0,     0,    39,
       0,     0,     0,     0,    39,     0,     0,    39,    39,     0,
       0,     0,     0,     0,     0,     0,     0,    64,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    39,    89,
      90,    39,    39,    91,     0,    39,     0,     0,    39,    39,
       0,     0,     0,     0,     0,    39,    39,    39,    39,     0,
      39,    39,    39,     0,    39,    39,     0,     0,    39,    39,
      39,     0,    39,    39,    39,    39,    40,     0,   146,   148,
     150,   152,   154,   156,     0,     0,     0,     0,     0,    40,
       0,    40,    40,     0,     0,     0,     0,     0,     0,     0,
       0,   259,     0,   260,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    40,     0,     0,
       0,     0,     0,     0,     0,   215,   216,     0,     0,     0,
       0,   223,     0,   225,     0,     0,     0,     0,  -223,  -223,
    -223,   150,  -223,     0,     0,  -223,  -223,     0,     0,     0,
       0,     0,   150,     0,     0,     0,     0,    40,     0,     0,
       0,     0,   251,     0,     0,  -223,  -223,  -223,  -223,     0,
       0,     0,     0,     0,     0,     0,    40,     0,     0,     0,
       0,    40,  -223,  -223,     0,     0,    40,     0,     0,    40,
      40,   281,     0,     0,     0,  -223,     0,  -223,     0,     0,
    -223,  -223,   303,     0,     0,   307,     0,     0,     0,     0,
      40,     0,     0,    40,    40,     0,     0,    40,     0,     0,
      40,    40,     0,     0,   328,     0,   331,    40,    40,    40,
      40,     0,    40,    40,    40,     0,    40,    40,     0,     0,
      40,    40,    40,     0,    40,    40,    40,    40,     0,     0,
       0,     0,     0,     0,     0,   352,     0,     0,   354,     0,
     255,     0,     0,   157,   158,   159,   160,   161,   162,     0,
       0,     0,     0,   163,   164,   165,   166,     0,     0,     0,
       0,     0,   361,     0,   362,     0,   363,   262,   364,   260,
     365,     0,   366,     0,   367,     0,   368,     0,   369,     0,
     370,     0,     0,     0,   373,     0,   374,     0,   375,     0,
     376,     0,   377,     0,   378,     0,   379,     0,   380,     0,
     381,     0,   382,     0,  -223,  -223,  -223,     0,  -223,     0,
       0,  -223,  -223,     0,     0,     0,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,     0,     0,
       0,  -223,  -223,  -223,  -223,   256,   181,   264,     0,   260,
       0,     0,     0,     0,     0,     0,     0,     0,  -223,  -223,
     251,     0,   421,     0,     0,     4,     0,     0,     0,     0,
       0,  -223,     0,  -223,     0,     0,  -223,  -223,     0,     0,
       5,     6,     0,   424,  -223,  -223,  -223,     0,  -223,     0,
       0,  -223,  -223,   266,     0,   260,     0,    11,    12,    13,
       0,    14,    15,    16,    17,     0,     0,     0,     0,     0,
       0,  -223,  -223,  -223,  -223,    18,    19,     0,   405,    20,
      21,    22,     0,   443,   444,    23,   446,   447,  -223,  -223,
    -223,  -223,  -223,     0,  -223,     0,     0,  -223,  -223,     0,
     268,  -223,   260,  -223,     0,     0,  -223,  -223,   263,   265,
     267,   269,   271,   273,   275,   277,   279,  -223,  -223,  -223,
    -223,   283,   285,   287,   289,   291,   293,   295,   297,   299,
     301,     0,     0,     0,  -223,  -223,     0,  -223,  -223,  -223,
       0,  -223,     0,     0,  -223,  -223,   270,  -223,   260,  -223,
       0,     0,  -223,  -223,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -223,  -223,  -223,  -223,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -223,  -223,  -223,  -223,  -223,     0,  -223,     0,     0,
    -223,  -223,     0,   272,  -223,   260,  -223,     0,     0,  -223,
    -223,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -223,  -223,  -223,  -223,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -223,  -223,     0,
    -223,  -223,  -223,     0,  -223,     0,     0,  -223,  -223,   274,
    -223,   260,  -223,     0,     0,  -223,  -223,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -223,  -223,  -223,
    -223,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -223,  -223,  -223,  -223,  -223,     0,
    -223,     0,     0,  -223,  -223,     0,   276,  -223,   260,  -223,
       0,     0,  -223,  -223,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -223,  -223,  -223,  -223,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -223,  -223,     0,  -223,  -223,  -223,     0,  -223,     0,     0,
    -223,  -223,   278,  -223,   260,  -223,     0,     0,  -223,  -223,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -223,  -223,  -223,  -223,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -223,  -223,  -223,
    -223,  -223,     0,  -223,     0,     0,  -223,  -223,     0,   282,
    -223,   260,  -223,     0,     0,  -223,  -223,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -223,  -223,  -223,  -223,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -223,  -223,     0,  -223,  -223,  -223,     0,
    -223,     0,     0,  -223,  -223,   284,  -223,   260,  -223,     0,
       0,  -223,  -223,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -223,  -223,  -223,  -223,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -223,  -223,  -223,  -223,  -223,     0,  -223,     0,     0,  -223,
    -223,     0,   286,  -223,   260,  -223,     0,     0,  -223,  -223,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -223,
    -223,  -223,  -223,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -223,  -223,     0,  -223,
    -223,  -223,     0,  -223,     0,     0,  -223,  -223,   288,  -223,
     260,  -223,     0,     0,  -223,  -223,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -223,  -223,  -223,  -223,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -223,  -223,  -223,  -223,  -223,     0,  -223,
       0,     0,  -223,  -223,     0,   290,  -223,   260,  -223,     0,
       0,  -223,  -223,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -223,  -223,  -223,  -223,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -223,
    -223,     0,  -223,  -223,  -223,     0,  -223,     0,     0,  -223,
    -223,   292,  -223,   260,  -223,     0,     0,  -223,  -223,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -223,
    -223,  -223,  -223,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -223,  -223,  -223,  -223,
    -223,     0,  -223,     0,     0,  -223,  -223,     0,   294,  -223,
     260,  -223,     0,     0,  -223,  -223,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -223,  -223,  -223,  -223,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -223,  -223,     0,  -223,  -223,  -223,     0,  -223,
       0,     0,  -223,  -223,   296,  -223,   260,  -223,     0,     0,
    -223,  -223,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -223,  -223,  -223,  -223,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -223,
    -223,  -223,  -223,  -223,     0,  -223,     0,     0,  -223,  -223,
       0,   298,  -223,   260,  -223,     0,     0,  -223,  -223,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -223,  -223,
    -223,  -223,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -223,  -223,     0,  -223,  -223,
    -223,     0,  -223,     0,     0,  -223,  -223,   300,  -223,   260,
    -223,     0,     0,  -223,  -223,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -223,  -223,  -223,  -223,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -223,  -223,  -223,  -223,  -223,     0,  -223,     0,
       0,  -223,  -223,   145,     0,  -223,     0,  -223,     0,     0,
    -223,  -223,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -223,  -223,  -223,  -223,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -223,  -223,
      50,    14,    15,     0,    17,     0,     0,    51,    52,   147,
       0,  -223,     0,  -223,     0,     0,  -223,  -223,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    57,    58,    50,    14,    15,     0,
      17,     0,     0,    51,    52,   149,     0,    59,     0,    60,
       0,     0,    61,    62,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,    50,    14,    15,     0,    17,     0,     0,    51,
      52,   153,     0,    59,     0,    60,     0,     0,    61,    62,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    57,    58,    50,    14,
      15,     0,    17,     0,     0,    51,    52,   155,     0,    59,
       0,    60,     0,     0,    61,    62,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    57,    58,    50,    14,    15,     0,    17,     0,
       0,    51,    52,   280,     0,    59,     0,    60,     0,     0,
      61,    62,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    57,    58,
      50,    14,    15,     0,    17,     0,     0,    51,    52,   423,
       0,    59,     0,    60,     0,     0,    61,    62,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    57,    58,    50,    14,    15,     0,
      17,     0,     0,    51,    52,     0,     0,    59,   236,    60,
       0,     0,    61,    62,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,    56,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,     0,    50,    14,    15,     0,    17,     0,     0,
      51,    52,     0,    59,   244,    60,     0,     0,    61,    62,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    57,    58,    50,
      14,    15,     0,    17,     0,     0,    51,    52,     0,     0,
      59,   351,   237,     0,   142,    61,    62,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,    54,    55,    56,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    57,    58,     0,    50,    14,    15,     0,
      17,     0,     0,    51,    52,     0,    59,     0,   237,     0,
     142,    61,    62,     0,   371,     0,     0,   157,   158,   159,
     160,   161,   162,    53,    54,    55,    56,   163,   164,   165,
     166,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      57,    58,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    59,     0,    60,     0,     0,    61,    62,
     157,   158,   159,   160,   161,   162,     0,     0,     0,     0,
     163,   164,   165,   166,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   157,   158,   159,   160,   161,   162,   372,     0,     0,
     181,   163,   164,   165,   166,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   258,     0,     0,     0,
       0,     0,     0,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,     0,     0,     0,     0,     0,
     388,     0,     0,   181,   359,   200,   157,   158,   159,   160,
     161,   162,     0,     0,     0,     0,   163,   164,   165,   166,
       0,     0,     0,     0,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,     0,     0,     0,     0,
       0,   388,     0,     0,   181,   359,   157,   158,   159,   160,
     161,   162,     0,     0,     0,     0,   163,   164,   165,   166,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   170,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     157,   158,   159,   160,   161,   162,     0,     0,     0,   181,
     163,   164,   165,   166,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   170,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
       0,   157,   158,   159,   160,   161,   162,     0,     0,   181,
     341,   163,   164,   165,   166,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,   157,   158,   159,   160,   161,
     162,     0,     0,   181,   347,   163,   164,   165,   166,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   181,   348,     0,     0,   157,   158,
     159,   160,   161,   162,     0,     0,     0,     0,   163,   164,
     165,   166,     0,     0,     0,     0,     0,     0,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,    11,
      12,    13,     0,     0,     0,     0,     0,     0,   181,   359,
       0,   167,   168,   169,   157,   158,   159,   160,   161,   162,
       0,     0,     0,     0,   163,   164,   165,   166,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,     0,     0,     0,     0,   167,   168,   169,
       0,   181,   157,   158,   159,   160,   161,   162,     0,     0,
       0,     0,   163,   164,   165,   166,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   157,   158,
     159,   160,   161,   162,     0,     0,     0,   181,   163,   164,
     165,   166,     0,   198,     0,     0,     0,     0,     0,   157,
     158,   159,   160,   161,   162,     0,     0,     0,     0,   163,
     164,   165,   166,     0,     0,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,     0,     0,   199,
       0,     0,     0,     0,     0,   181,   157,   158,   159,   160,
     161,   162,     0,     0,     0,     0,   163,   164,   165,   166,
       0,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,     0,     0,   258,     0,     0,     0,     0,
       0,   181,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   157,   158,   159,   160,   161,   162,     0,
       0,     0,   181,   163,   164,   165,   166,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   170,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     157,   158,   159,   160,   161,   162,   384,     0,     0,   181,
     163,   164,   165,   166,   414,     0,     0,     0,     0,     0,
       0,   157,   158,   159,   160,   161,   162,     0,     0,     0,
       0,   163,   164,   165,   166,     0,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,     0,     0,
       0,   417,     0,     0,     0,     0,   181,   157,   158,   159,
     160,   161,   162,     0,     0,     0,     0,   163,   164,   165,
     166,     0,   461,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   181,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,     0,     0,   462,     0,
       0,     0,     0,     0,   181,   157,   158,   159,   160,   161,
     162,     0,     0,     0,     0,   163,   164,   165,   166,     0,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     181,   157,   158,   159,   160,   161,   162,     0,     0,     0,
       0,   163,   164,   165,   166,     0,   464,     0,     0,     0,
       0,     0,   157,   158,   159,   160,   161,   162,     0,     0,
       0,     0,   163,   164,   165,   166,     0,     0,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
     157,     0,   465,   160,   161,   162,     0,     0,   181,     0,
     163,   164,   165,   166,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   181,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   114,   115,   116,   117,
       0,     0,     0,     0,     0,   181,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,    50,    14,    15,     0,    17,     0,
       0,    51,    52,   181,     0,     0,     0,     0,     0,     0,
       0,     0,   125,   126,   127,   128,     0,     0,     0,     0,
       0,    53,    54,    55,    56,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   118,     0,     0,     0,    57,    58,
      50,    14,    15,     0,    17,     0,     0,    51,    52,   119,
       0,   120,     0,    60,     0,     0,    61,    62,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     129,     0,     0,     0,    57,    58,    50,    14,    15,     0,
      17,     0,     0,    51,    52,   130,     0,   131,     0,    60,
       0,     0,    61,    62,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,    56,     0,     0,     0,
       0,     0,     0,     0,     0,    50,    14,    15,     0,    17,
      57,    58,    51,    52,    50,    14,    15,     0,    17,     0,
       0,    51,    52,    59,   138,    60,     0,     0,    61,    62,
       0,     0,    53,    54,    55,    56,     0,     0,     0,     0,
       0,    53,    54,    55,    56,     0,     0,     0,     0,    57,
      58,     0,     0,    50,    14,    15,     0,    17,    57,    58,
      51,    52,    59,   143,    60,     0,     0,    61,    62,     0,
       0,    59,     0,    60,   250,     0,    61,    62,     0,     0,
      53,    54,    55,    56,     0,     0,     0,     0,     0,     0,
       0,     0,    50,    14,    15,     0,    17,    57,    58,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,   302,
      59,     0,    60,     0,     0,    61,    62,     0,     0,    53,
      54,    55,    56,     0,     0,     0,     0,     0,     0,     0,
       0,    50,    14,    15,     0,    17,    57,    58,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,   306,    59,
       0,    60,     0,     0,    61,    62,     0,     0,    53,    54,
      55,    56,     0,     0,     0,     0,     0,     0,     0,     0,
      50,    14,    15,     0,    17,    57,    58,    51,    52,    50,
      14,    15,     0,    17,     0,     0,    51,    52,    59,     0,
      60,   426,     0,    61,    62,     0,     0,    53,    54,    55,
      56,     0,     0,     0,     0,     0,    53,    54,    55,    56,
       0,     0,     0,     0,    57,    58,     0,     0,     0,     4,
       0,     0,     0,    57,    58,     0,     0,    59,     0,    60,
     429,     0,    61,    62,     5,     6,    59,     0,    60,     0,
       0,    61,    62,     0,     4,     0,     0,     0,     0,     0,
       0,    11,    12,    13,     0,    14,    15,    16,    17,     5,
       6,     0,     0,     0,     0,     0,     0,     0,   343,    18,
      19,     0,     0,    20,    21,    22,    11,    12,    13,    23,
      14,    15,    16,    17,     0,     0,     4,     0,     0,     0,
       0,     0,     0,     0,    18,    19,     0,   406,    20,    21,
      22,     5,     6,     0,    23,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     4,     0,    11,    12,
      13,     0,    14,    15,    16,    17,     0,     0,     0,     0,
       0,     5,     6,     0,     0,   420,    18,    19,     0,     0,
      20,    21,    22,     0,     0,     4,    23,   440,    11,    12,
      13,     0,    14,    15,    16,    17,     0,     0,     0,     0,
       5,     6,     0,     0,     0,     0,    18,    19,     0,     0,
      20,    21,    22,     0,     4,     0,    23,    11,    12,    13,
       0,    14,    15,    16,    17,     0,     0,     0,     0,     5,
       6,     0,     0,     0,     0,    18,    19,     0,   463,    20,
      21,    22,     0,     4,     0,    23,    11,    12,    13,     0,
      14,    15,    16,    17,     0,     0,     0,     0,     5,     6,
       0,     0,     0,   471,    18,    19,     0,   466,    20,    21,
      22,     0,     4,     0,    23,    11,    12,    13,     0,    14,
      15,    16,    17,     0,     0,     0,     0,     5,     6,     0,
       0,     0,   476,    18,    19,     0,     0,    20,    21,    22,
       0,     4,     0,    23,    11,    12,    13,     0,    14,    15,
      16,    17,     0,     0,     0,     0,     5,     6,     0,     0,
       0,   477,    18,    19,     0,     0,    20,    21,    22,     0,
       4,     0,    23,    11,    12,    13,     0,    14,    15,    16,
      17,     0,     0,     0,     0,     5,     6,     0,     0,     0,
     478,    18,    19,     0,     0,    20,    21,    22,     0,     4,
       0,    23,    11,    12,    13,     0,    14,    15,    16,    17,
       0,     0,     0,     0,     5,     6,     0,     0,     0,     0,
      18,    19,     0,     0,    20,    21,    22,     0,     4,     0,
      23,    11,    12,    13,     0,    14,    15,    16,    17,     0,
       0,     0,     0,     5,     6,     0,     0,     0,     0,    18,
      19,     0,   480,    20,    21,    22,     0,     4,     0,    23,
      11,    12,    13,     0,    14,    15,    16,    17,     0,     0,
       0,     0,     5,     6,     0,     0,     0,     0,    18,    19,
       0,   481,    20,    21,    22,     0,     4,     0,    23,    11,
      12,    13,     0,    14,    15,    16,    17,     0,     0,     0,
       0,     5,     6,     0,     0,     0,     0,    18,    19,     0,
     482,    20,    21,    22,     0,     4,     0,    23,    11,    12,
      13,     0,    14,    15,    16,    17,     0,     0,     0,     0,
       5,     6,     0,     0,     0,     0,    18,    19,     0,   483,
      20,    21,    22,     0,     0,     0,    23,    11,    12,    13,
       0,    14,    15,    16,    17,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    18,    19,     0,     0,    20,
      21,    22,     0,     0,     0,    23
};

static const yytype_int16 yycheck[] =
{
       2,    10,     3,    81,   315,     7,     8,     9,    10,    93,
       3,     3,    23,     3,   392,   198,   199,   325,     3,     1,
       3,    23,     3,     3,    64,    39,     3,    41,    87,     3,
       3,     0,     3,     3,    27,     3,     3,   220,    97,    41,
       3,    43,     3,    45,    64,     3,    39,     3,    41,     3,
       2,    39,    93,    93,    93,    96,    38,    39,    40,    98,
      42,     3,     3,    45,    46,    39,    93,    41,   314,    93,
      81,    98,    93,    93,    98,   453,    93,    98,    95,    81,
      60,    61,    96,    65,    66,    67,    68,    93,    41,    97,
      43,   402,    98,    94,    47,    95,    49,    16,    17,   407,
      82,    83,    95,    93,   106,    97,    97,    99,    93,   111,
      93,   113,    93,    95,    96,    97,    93,   119,   100,   101,
      93,   304,    93,    93,    27,    93,    93,     3,   130,    39,
      93,    41,    93,     3,   317,    93,    39,    93,    41,    93,
     142,     1,    95,    96,    97,    98,    99,   100,   101,   102,
     103,    93,    93,   399,   400,    39,    40,    41,    42,     2,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    39,
     123,   124,    93,    39,    97,    45,    99,    98,    97,    45,
     133,   134,    97,   185,    39,    40,    41,    42,   197,    60,
      61,   202,    95,   204,    39,   197,   198,   199,    39,    40,
     202,    42,   204,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    82,     3,    93,   217,    82,    96,   220,    93,
      93,    97,    96,    96,     3,    97,   409,    84,    85,    86,
      87,   414,     3,   311,   417,   418,   314,   315,    39,    98,
      97,    96,   326,   327,    98,   329,   330,    82,    83,    84,
      85,    86,    87,    93,    93,   438,    96,    94,   441,    50,
      51,    52,    97,   106,    26,   217,    16,    17,    97,    97,
     113,    99,   455,   456,   457,    97,   119,    99,   461,   462,
       3,   464,   465,    50,    51,    52,    97,   130,    60,    61,
      62,    97,     2,    99,    97,    16,    17,     3,     3,   142,
     311,     3,   304,   314,   315,   258,    93,     3,    94,   311,
      98,    64,   314,   315,   392,   317,     3,   319,   320,     3,
       3,   399,   400,    26,   402,     3,     3,     3,     3,     3,
      80,    81,    82,    83,    84,    85,    86,    87,     3,     3,
       3,     3,   185,   345,   197,     3,    80,    97,   108,   302,
     119,   185,   185,   306,   216,   198,   199,    16,    17,   321,
      81,    82,    83,    84,    85,    86,    87,   319,   320,   322,
      64,    -1,    -1,    -1,   217,   453,    97,   220,    -1,    -1,
      -1,   392,    -1,   385,    -1,    -1,    -1,    -1,   399,   400,
     392,   402,    -1,   345,    -1,    -1,   106,   399,   400,    -1,
     402,    -1,   404,   113,    -1,    -1,    -1,   409,    -1,   119,
      -1,    -1,   414,    -1,    -1,   417,   418,    -1,    -1,    -1,
     130,    -1,    -1,    82,    83,    84,    85,    86,    87,    -1,
      -1,   384,   142,   385,    -1,   388,   438,    -1,    97,   441,
     442,    -1,   453,   445,    -1,    -1,   448,   449,    -1,    -1,
      -1,   453,   404,   455,   456,   457,   458,    -1,   460,   461,
     462,   304,   464,   465,     2,    -1,   468,   469,   470,    -1,
     472,   473,   474,   475,   317,   185,   319,   320,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   198,   199,
     442,     1,    -1,   445,    -1,    -1,   448,   449,    -1,    -1,
      -1,    -1,   345,    -1,    -1,    -1,   458,   217,   460,    -1,
     220,    -1,    -1,    -1,    -1,    -1,   468,   469,   470,    -1,
     472,   473,   474,   475,    -1,    -1,    -1,    -1,    38,    39,
      40,    -1,    42,    -1,    -1,    45,    46,    -1,    -1,    -1,
      -1,    -1,   385,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,    -1,
      -1,   404,    -1,    -1,    -1,    -1,   409,    -1,   106,    -1,
      -1,   414,    82,    83,   417,   418,    -1,    -1,    -1,    -1,
      -1,   119,    -1,    -1,    -1,    95,    96,    97,    -1,    -1,
     100,   101,    -1,    -1,   304,   438,    -1,    -1,   441,   442,
      -1,    -1,   445,    -1,    -1,   448,   449,   317,    -1,   319,
     320,    -1,   455,   456,   457,   458,    -1,   460,   461,   462,
      -1,   464,   465,    -1,    -1,   468,   469,   470,    -1,   472,
     473,   474,   475,     3,    -1,   345,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   185,    18,    19,
      20,    -1,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     198,   199,    32,    33,    -1,    35,    36,    37,    -1,    39,
      40,    41,    42,    -1,    -1,   385,    -1,    -1,    -1,   217,
      -1,    -1,   220,    53,    54,    -1,    -1,    57,    58,    59,
      -1,    -1,    -1,    63,   404,    -1,    -1,    -1,    -1,   409,
      -1,    -1,    -1,    -1,   414,    -1,    -1,   417,   418,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     6,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   438,    18,
      19,   441,   442,    22,    -1,   445,    -1,    -1,   448,   449,
      -1,    -1,    -1,    -1,    -1,   455,   456,   457,   458,    -1,
     460,   461,   462,    -1,   464,   465,    -1,    -1,   468,   469,
     470,    -1,   472,   473,   474,   475,   304,    -1,    57,    58,
      59,    60,    61,    62,    -1,    -1,    -1,    -1,    -1,   317,
      -1,   319,   320,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     1,    -1,     3,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   345,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   104,   105,    -1,    -1,    -1,
      -1,   110,    -1,   112,    -1,    -1,    -1,    -1,    38,    39,
      40,   120,    42,    -1,    -1,    45,    46,    -1,    -1,    -1,
      -1,    -1,   131,    -1,    -1,    -1,    -1,   385,    -1,    -1,
      -1,    -1,   141,    -1,    -1,    65,    66,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   404,    -1,    -1,    -1,
      -1,   409,    82,    83,    -1,    -1,   414,    -1,    -1,   417,
     418,   170,    -1,    -1,    -1,    95,    -1,    97,    -1,    -1,
     100,   101,   181,    -1,    -1,   184,    -1,    -1,    -1,    -1,
     438,    -1,    -1,   441,   442,    -1,    -1,   445,    -1,    -1,
     448,   449,    -1,    -1,   203,    -1,   205,   455,   456,   457,
     458,    -1,   460,   461,   462,    -1,   464,   465,    -1,    -1,
     468,   469,   470,    -1,   472,   473,   474,   475,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   234,    -1,    -1,   237,    -1,
       1,    -1,    -1,     4,     5,     6,     7,     8,     9,    -1,
      -1,    -1,    -1,    14,    15,    16,    17,    -1,    -1,    -1,
      -1,    -1,   261,    -1,   263,    -1,   265,     1,   267,     3,
     269,    -1,   271,    -1,   273,    -1,   275,    -1,   277,    -1,
     279,    -1,    -1,    -1,   283,    -1,   285,    -1,   287,    -1,
     289,    -1,   291,    -1,   293,    -1,   295,    -1,   297,    -1,
     299,    -1,   301,    -1,    38,    39,    40,    -1,    42,    -1,
      -1,    45,    46,    -1,    -1,    -1,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    -1,    -1,    -1,
      -1,    65,    66,    67,    68,    96,    97,     1,    -1,     3,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,
     349,    -1,   351,    -1,    -1,     3,    -1,    -1,    -1,    -1,
      -1,    95,    -1,    97,    -1,    -1,   100,   101,    -1,    -1,
      18,    19,    -1,   372,    38,    39,    40,    -1,    42,    -1,
      -1,    45,    46,     1,    -1,     3,    -1,    35,    36,    37,
      -1,    39,    40,    41,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    65,    66,    67,    68,    53,    54,    -1,    56,    57,
      58,    59,    -1,   412,   413,    63,   415,   416,    82,    83,
      38,    39,    40,    -1,    42,    -1,    -1,    45,    46,    -1,
       1,    95,     3,    97,    -1,    -1,   100,   101,   158,   159,
     160,   161,   162,   163,   164,   165,   166,    65,    66,    67,
      68,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,    -1,    -1,    -1,    82,    83,    -1,    38,    39,    40,
      -1,    42,    -1,    -1,    45,    46,     1,    95,     3,    97,
      -1,    -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    83,    38,    39,    40,    -1,    42,    -1,    -1,
      45,    46,    -1,     1,    95,     3,    97,    -1,    -1,   100,
     101,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,    -1,
      38,    39,    40,    -1,    42,    -1,    -1,    45,    46,     1,
      95,     3,    97,    -1,    -1,   100,   101,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    83,    38,    39,    40,    -1,
      42,    -1,    -1,    45,    46,    -1,     1,    95,     3,    97,
      -1,    -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    83,    -1,    38,    39,    40,    -1,    42,    -1,    -1,
      45,    46,     1,    95,     3,    97,    -1,    -1,   100,   101,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,    38,
      39,    40,    -1,    42,    -1,    -1,    45,    46,    -1,     1,
      95,     3,    97,    -1,    -1,   100,   101,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    83,    -1,    38,    39,    40,    -1,
      42,    -1,    -1,    45,    46,     1,    95,     3,    97,    -1,
      -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    83,    38,    39,    40,    -1,    42,    -1,    -1,    45,
      46,    -1,     1,    95,     3,    97,    -1,    -1,   100,   101,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    83,    -1,    38,
      39,    40,    -1,    42,    -1,    -1,    45,    46,     1,    95,
       3,    97,    -1,    -1,   100,   101,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    83,    38,    39,    40,    -1,    42,
      -1,    -1,    45,    46,    -1,     1,    95,     3,    97,    -1,
      -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      83,    -1,    38,    39,    40,    -1,    42,    -1,    -1,    45,
      46,     1,    95,     3,    97,    -1,    -1,   100,   101,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    83,    38,    39,
      40,    -1,    42,    -1,    -1,    45,    46,    -1,     1,    95,
       3,    97,    -1,    -1,   100,   101,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    83,    -1,    38,    39,    40,    -1,    42,
      -1,    -1,    45,    46,     1,    95,     3,    97,    -1,    -1,
     100,   101,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      83,    38,    39,    40,    -1,    42,    -1,    -1,    45,    46,
      -1,     1,    95,     3,    97,    -1,    -1,   100,   101,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    83,    -1,    38,    39,
      40,    -1,    42,    -1,    -1,    45,    46,     1,    95,     3,
      97,    -1,    -1,   100,   101,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    83,    38,    39,    40,    -1,    42,    -1,
      -1,    45,    46,     1,    -1,    95,    -1,    97,    -1,    -1,
     100,   101,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,
      38,    39,    40,    -1,    42,    -1,    -1,    45,    46,     1,
      -1,    95,    -1,    97,    -1,    -1,   100,   101,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    83,    38,    39,    40,    -1,
      42,    -1,    -1,    45,    46,     1,    -1,    95,    -1,    97,
      -1,    -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    83,    38,    39,    40,    -1,    42,    -1,    -1,    45,
      46,     1,    -1,    95,    -1,    97,    -1,    -1,   100,   101,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    83,    38,    39,
      40,    -1,    42,    -1,    -1,    45,    46,     1,    -1,    95,
      -1,    97,    -1,    -1,   100,   101,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    83,    38,    39,    40,    -1,    42,    -1,
      -1,    45,    46,     1,    -1,    95,    -1,    97,    -1,    -1,
     100,   101,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,
      38,    39,    40,    -1,    42,    -1,    -1,    45,    46,     1,
      -1,    95,    -1,    97,    -1,    -1,   100,   101,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    83,    38,    39,    40,    -1,
      42,    -1,    -1,    45,    46,    -1,    -1,    95,     3,    97,
      -1,    -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    83,    -1,    38,    39,    40,    -1,    42,    -1,    -1,
      45,    46,    -1,    95,     3,    97,    -1,    -1,   100,   101,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,    38,
      39,    40,    -1,    42,    -1,    -1,    45,    46,    -1,    -1,
      95,     3,    97,    -1,    99,   100,   101,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    83,    -1,    38,    39,    40,    -1,
      42,    -1,    -1,    45,    46,    -1,    95,    -1,    97,    -1,
      99,   100,   101,    -1,     1,    -1,    -1,     4,     5,     6,
       7,     8,     9,    65,    66,    67,    68,    14,    15,    16,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    83,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    -1,    -1,   100,   101,
       4,     5,     6,     7,     8,     9,    -1,    -1,    -1,    -1,
      14,    15,    16,    17,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,     4,     5,     6,     7,     8,     9,    94,    -1,    -1,
      97,    14,    15,    16,    17,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    70,    -1,    -1,    -1,
      -1,    -1,    -1,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,    -1,
      94,    -1,    -1,    97,    98,     3,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    14,    15,    16,    17,
      -1,    -1,    -1,    -1,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,
      -1,    94,    -1,    -1,    97,    98,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    14,    15,    16,    17,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
       4,     5,     6,     7,     8,     9,    -1,    -1,    -1,    97,
      14,    15,    16,    17,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      -1,     4,     5,     6,     7,     8,     9,    -1,    -1,    97,
      98,    14,    15,    16,    17,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    -1,     4,     5,     6,     7,     8,
       9,    -1,    -1,    97,    98,    14,    15,    16,    17,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    97,    98,    -1,    -1,     4,     5,
       6,     7,     8,     9,    -1,    -1,    -1,    -1,    14,    15,
      16,    17,    -1,    -1,    -1,    -1,    -1,    -1,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    -1,    97,    98,
      -1,    47,    48,    49,     4,     5,     6,     7,     8,     9,
      -1,    -1,    -1,    -1,    14,    15,    16,    17,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    -1,    -1,    -1,    -1,    -1,    47,    48,    49,
      -1,    97,     4,     5,     6,     7,     8,     9,    -1,    -1,
      -1,    -1,    14,    15,    16,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,     4,     5,
       6,     7,     8,     9,    -1,    -1,    -1,    97,    14,    15,
      16,    17,    -1,    55,    -1,    -1,    -1,    -1,    -1,     4,
       5,     6,     7,     8,     9,    -1,    -1,    -1,    -1,    14,
      15,    16,    17,    -1,    -1,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    -1,    -1,    -1,    55,
      -1,    -1,    -1,    -1,    -1,    97,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    14,    15,    16,    17,
      -1,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    -1,    -1,    -1,    70,    -1,    -1,    -1,    -1,
      -1,    97,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,     4,     5,     6,     7,     8,     9,    -1,
      -1,    -1,    97,    14,    15,    16,    17,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
       4,     5,     6,     7,     8,     9,    94,    -1,    -1,    97,
      14,    15,    16,    17,    55,    -1,    -1,    -1,    -1,    -1,
      -1,     4,     5,     6,     7,     8,     9,    -1,    -1,    -1,
      -1,    14,    15,    16,    17,    -1,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    -1,    -1,    -1,
      -1,    55,    -1,    -1,    -1,    -1,    97,     4,     5,     6,
       7,     8,     9,    -1,    -1,    -1,    -1,    14,    15,    16,
      17,    -1,    55,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    97,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    -1,    -1,    -1,    55,    -1,
      -1,    -1,    -1,    -1,    97,     4,     5,     6,     7,     8,
       9,    -1,    -1,    -1,    -1,    14,    15,    16,    17,    -1,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      97,     4,     5,     6,     7,     8,     9,    -1,    -1,    -1,
      -1,    14,    15,    16,    17,    -1,    55,    -1,    -1,    -1,
      -1,    -1,     4,     5,     6,     7,     8,     9,    -1,    -1,
      -1,    -1,    14,    15,    16,    17,    -1,    -1,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    -1,
       4,    -1,    55,     7,     8,     9,    -1,    -1,    97,    -1,
      14,    15,    16,    17,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    97,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    10,    11,    12,    13,
      -1,    -1,    -1,    -1,    -1,    97,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    38,    39,    40,    -1,    42,    -1,
      -1,    45,    46,    97,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    10,    11,    12,    13,    -1,    -1,    -1,    -1,
      -1,    65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    78,    -1,    -1,    -1,    82,    83,
      38,    39,    40,    -1,    42,    -1,    -1,    45,    46,    93,
      -1,    95,    -1,    97,    -1,    -1,   100,   101,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      78,    -1,    -1,    -1,    82,    83,    38,    39,    40,    -1,
      42,    -1,    -1,    45,    46,    93,    -1,    95,    -1,    97,
      -1,    -1,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    38,    39,    40,    -1,    42,
      82,    83,    45,    46,    38,    39,    40,    -1,    42,    -1,
      -1,    45,    46,    95,    96,    97,    -1,    -1,   100,   101,
      -1,    -1,    65,    66,    67,    68,    -1,    -1,    -1,    -1,
      -1,    65,    66,    67,    68,    -1,    -1,    -1,    -1,    82,
      83,    -1,    -1,    38,    39,    40,    -1,    42,    82,    83,
      45,    46,    95,    96,    97,    -1,    -1,   100,   101,    -1,
      -1,    95,    -1,    97,    98,    -1,   100,   101,    -1,    -1,
      65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    38,    39,    40,    -1,    42,    82,    83,    45,
      46,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,
      95,    -1,    97,    -1,    -1,   100,   101,    -1,    -1,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    38,    39,    40,    -1,    42,    82,    83,    45,    46,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,    95,
      -1,    97,    -1,    -1,   100,   101,    -1,    -1,    65,    66,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      38,    39,    40,    -1,    42,    82,    83,    45,    46,    38,
      39,    40,    -1,    42,    -1,    -1,    45,    46,    95,    -1,
      97,    98,    -1,   100,   101,    -1,    -1,    65,    66,    67,
      68,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,    68,
      -1,    -1,    -1,    -1,    82,    83,    -1,    -1,    -1,     3,
      -1,    -1,    -1,    82,    83,    -1,    -1,    95,    -1,    97,
      98,    -1,   100,   101,    18,    19,    95,    -1,    97,    -1,
      -1,   100,   101,    -1,     3,    -1,    -1,    -1,    -1,    -1,
      -1,    35,    36,    37,    -1,    39,    40,    41,    42,    18,
      19,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,    53,
      54,    -1,    -1,    57,    58,    59,    35,    36,    37,    63,
      39,    40,    41,    42,    -1,    -1,     3,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    53,    54,    -1,    56,    57,    58,
      59,    18,    19,    -1,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,    -1,    35,    36,
      37,    -1,    39,    40,    41,    42,    -1,    -1,    -1,    -1,
      -1,    18,    19,    -1,    -1,    52,    53,    54,    -1,    -1,
      57,    58,    59,    -1,    -1,     3,    63,    34,    35,    36,
      37,    -1,    39,    40,    41,    42,    -1,    -1,    -1,    -1,
      18,    19,    -1,    -1,    -1,    -1,    53,    54,    -1,    -1,
      57,    58,    59,    -1,     3,    -1,    63,    35,    36,    37,
      -1,    39,    40,    41,    42,    -1,    -1,    -1,    -1,    18,
      19,    -1,    -1,    -1,    -1,    53,    54,    -1,    56,    57,
      58,    59,    -1,     3,    -1,    63,    35,    36,    37,    -1,
      39,    40,    41,    42,    -1,    -1,    -1,    -1,    18,    19,
      -1,    -1,    -1,    23,    53,    54,    -1,    56,    57,    58,
      59,    -1,     3,    -1,    63,    35,    36,    37,    -1,    39,
      40,    41,    42,    -1,    -1,    -1,    -1,    18,    19,    -1,
      -1,    -1,    23,    53,    54,    -1,    -1,    57,    58,    59,
      -1,     3,    -1,    63,    35,    36,    37,    -1,    39,    40,
      41,    42,    -1,    -1,    -1,    -1,    18,    19,    -1,    -1,
      -1,    23,    53,    54,    -1,    -1,    57,    58,    59,    -1,
       3,    -1,    63,    35,    36,    37,    -1,    39,    40,    41,
      42,    -1,    -1,    -1,    -1,    18,    19,    -1,    -1,    -1,
      23,    53,    54,    -1,    -1,    57,    58,    59,    -1,     3,
      -1,    63,    35,    36,    37,    -1,    39,    40,    41,    42,
      -1,    -1,    -1,    -1,    18,    19,    -1,    -1,    -1,    -1,
      53,    54,    -1,    -1,    57,    58,    59,    -1,     3,    -1,
      63,    35,    36,    37,    -1,    39,    40,    41,    42,    -1,
      -1,    -1,    -1,    18,    19,    -1,    -1,    -1,    -1,    53,
      54,    -1,    56,    57,    58,    59,    -1,     3,    -1,    63,
      35,    36,    37,    -1,    39,    40,    41,    42,    -1,    -1,
      -1,    -1,    18,    19,    -1,    -1,    -1,    -1,    53,    54,
      -1,    56,    57,    58,    59,    -1,     3,    -1,    63,    35,
      36,    37,    -1,    39,    40,    41,    42,    -1,    -1,    -1,
      -1,    18,    19,    -1,    -1,    -1,    -1,    53,    54,    -1,
      56,    57,    58,    59,    -1,     3,    -1,    63,    35,    36,
      37,    -1,    39,    40,    41,    42,    -1,    -1,    -1,    -1,
      18,    19,    -1,    -1,    -1,    -1,    53,    54,    -1,    56,
      57,    58,    59,    -1,    -1,    -1,    63,    35,    36,    37,
      -1,    39,    40,    41,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    53,    54,    -1,    -1,    57,
      58,    59,    -1,    -1,    -1,    63
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   103,   104,     0,     3,    18,    19,    20,    22,    32,
      33,    35,    36,    37,    39,    40,    41,    42,    53,    54,
      57,    58,    59,    63,   105,   106,   109,   113,   118,   119,
     121,   122,   123,   124,   127,   128,   131,   132,   133,   139,
     140,   144,   145,   146,   147,   152,   160,   161,   162,   163,
      38,    45,    46,    65,    66,    67,    68,    82,    83,    95,
      97,   100,   101,   119,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   154,   155,   156,   157,   158,   162,   162,
     162,   162,     3,    82,   111,   112,   158,   159,   162,   135,
     135,   135,   160,   162,     3,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    97,    50,    51,    52,   125,   126,
      97,    99,    97,    99,    10,    11,    12,    13,    78,    93,
      95,   134,   135,   148,   162,    10,    11,    12,    13,    78,
      93,    95,   134,   149,   162,    97,   162,    97,    96,   134,
       3,    97,    99,    96,   134,     1,   135,     1,   135,     1,
     135,   134,   135,     1,   135,     1,   135,     4,     5,     6,
       7,     8,     9,    14,    15,    16,    17,    47,    48,    49,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    97,   151,   152,    97,    95,   114,    93,   114,   107,
     108,   147,   160,   162,     1,    39,     3,    93,    55,    55,
       3,    64,    93,   150,    93,   150,   134,   134,   134,   134,
     134,   134,   134,   134,   134,   135,   135,   120,   121,     3,
      51,    52,   126,   135,   162,   135,   119,   139,   162,   139,
     145,   162,    96,     3,    93,   134,     3,    97,   134,   139,
     162,    96,     3,   134,     3,   134,    98,     3,    98,    96,
      98,   135,   139,   162,    96,     1,    96,    98,    70,     1,
       3,   153,     1,   153,     1,   153,     1,   153,     1,   153,
       1,   153,     1,   153,     1,   153,     1,   153,     1,   153,
       1,   135,     1,   153,     1,   153,     1,   153,     1,   153,
       1,   153,     1,   153,     1,   153,     1,   153,     1,   153,
       1,   153,    94,   135,     3,    39,    94,   135,    96,   144,
     146,    94,    26,     3,    94,    93,    97,   110,   112,   120,
     120,     3,    60,    61,   129,   130,   160,   162,   135,   160,
     162,   135,     3,     3,     3,     3,     3,     3,     3,     3,
       3,    98,   151,    52,   121,   120,     3,    98,    98,    97,
       3,     3,   135,     3,   135,     3,     3,     3,     3,    98,
     134,   135,   135,   135,   135,   135,   135,   135,   135,   135,
     135,     1,    94,   135,   135,   135,   135,   135,   135,   135,
     135,   135,   135,   134,    94,   120,     3,   134,    94,    96,
      96,    27,    95,   115,   117,   147,   160,   162,    93,    94,
       3,   115,     3,   108,   120,    56,    56,   130,   134,     3,
      62,   129,   150,   150,    55,   150,   150,    55,     3,     3,
      52,   135,    98,     1,   135,    98,    98,   134,    98,    98,
     134,    96,   116,   117,     3,    26,   115,   115,     3,   108,
      34,     3,   120,   135,   135,   120,   135,   135,   120,   120,
       3,    98,    98,    93,    96,     3,     3,     3,   120,     3,
     120,    55,    55,    56,    55,    55,    56,   117,   120,   120,
     120,    23,   120,   120,   120,   120,    23,    23,    23,     3,
      56,    56,    56,    56,     3,     3,     3
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,   102,   103,   104,   104,   105,   105,   105,   105,   105,
     106,   107,   107,   107,   108,   108,   108,   110,   109,   109,
     111,   111,   112,   112,   112,   113,   113,   113,   113,   114,
     114,   114,   115,   115,   115,   115,   116,   116,   117,   117,
     118,   118,   118,   118,   118,   118,   118,   118,   118,   118,
     118,   118,   118,   118,   118,   118,   118,   118,   118,   119,
     119,   119,   119,   120,   120,   120,   121,   121,   121,   121,
     121,   121,   121,   121,   121,   121,   121,   121,   121,   121,
     122,   123,   123,   123,   123,   124,   125,   125,   126,   127,
     128,   129,   129,   130,   130,   130,   131,   132,   132,   132,
     132,   132,   132,   133,   134,   134,   134,   135,   135,   135,
     135,   135,   135,   135,   135,   135,   135,   135,   135,   135,
     135,   135,   135,   135,   135,   136,   137,   137,   137,   137,
     137,   137,   138,   139,   139,   139,   140,   140,   140,   140,
     140,   140,   141,   141,   141,   141,   142,   142,   142,   142,
     142,   142,   142,   142,   143,   143,   143,   143,   143,   143,
     143,   143,   143,   143,   143,   143,   143,   143,   143,   143,
     143,   143,   143,   143,   143,   143,   143,   143,   143,   143,
     143,   143,   143,   143,   143,   143,   143,   143,   143,   143,
     143,   143,   143,   143,   144,   144,   145,   145,   145,   145,
     146,   146,   147,   147,   147,   148,   148,   148,   148,   148,
     149,   149,   149,   149,   149,   150,   151,   151,   151,   152,
     152,   152,   153,   153,   154,   155,   155,   156,   156,   157,
     158,   159,   160,   161,   162,   163
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     2,     0,     1,     1,     1,     1,     1,
       3,     3,     4,     1,     1,     1,     1,     0,     7,     3,
       3,     1,     1,     1,     1,    10,     9,    10,    10,     3,
       3,     2,     3,     2,     1,     1,     3,     1,     1,     1,
       2,     3,     4,     4,     3,     3,     4,     4,     3,     2,
       4,     4,     4,     4,     4,     4,     4,     4,     4,     3,
       2,     3,     2,     2,     1,     0,     4,     4,     1,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       5,     3,     5,     4,     6,     5,     2,     1,     5,     5,
       5,     4,     3,     2,     1,     2,     5,     7,     9,     7,
       9,     9,     9,     6,     3,     4,     1,     1,     3,     3,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     5,     6,     5,     5,     6,
       5,     5,     3,     4,     4,     4,     3,     3,     3,     3,
       4,     3,     5,     5,     4,     3,     2,     2,     2,     2,
       2,     2,     2,     2,     4,     3,     4,     3,     4,     3,
       4,     3,     4,     3,     4,     3,     4,     3,     4,     3,
       4,     3,     4,     3,     4,     3,     4,     3,     4,     3,
       4,     3,     4,     3,     4,     3,     4,     3,     4,     3,
       4,     3,     4,     3,     3,     1,     1,     1,     1,     1,
       3,     1,     3,     3,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     0,     1,     1,     1,     1,     1,     1,
       1,     2,     1,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (parm, scanner, csound, astTree, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, parm, scanner, csound, astTree); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, PARSE_PARM *parm, void *scanner, CSOUND * csound, TREE ** astTree)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (parm);
  YY_USE (scanner);
  YY_USE (csound);
  YY_USE (astTree);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, PARSE_PARM *parm, void *scanner, CSOUND * csound, TREE ** astTree)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, parm, scanner, csound, astTree);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, PARSE_PARM *parm, void *scanner, CSOUND * csound, TREE ** astTree)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], parm, scanner, csound, astTree);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, parm, scanner, csound, astTree); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
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
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, PARSE_PARM *parm, void *scanner, CSOUND * csound, TREE ** astTree)
{
  YY_USE (yyvaluep);
  YY_USE (parm);
  YY_USE (scanner);
  YY_USE (csound);
  YY_USE (astTree);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (PARSE_PARM *parm, void *scanner, CSOUND * csound, TREE ** astTree)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, csound, scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* orcfile: root_statement_list  */
#line 201 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          {
              if (yyvsp[0] != NULL)
                *astTree = ((TREE *)yyvsp[0]);
              else
                *astTree = NULL;
              csound->synterrcnt = csound_orcnerrs;
              if (csoundGetDebug(csound) & DEBUG_PARSER ||
		  csoundGetDebug(csound) & DEBUG_TREE)
                print_tree(csound, "ALL:\n", yyvsp[0]);
          }
#line 2637 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 3: /* root_statement_list: root_statement_list root_statement  */
#line 215 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                      { yyval = parser_append(csound, yyvsp[-1], yyvsp[0]); }
#line 2643 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 4: /* root_statement_list: %empty  */
#line 217 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                      { yyval = NULL; }
#line 2649 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 10: /* struct_definition: STRUCT_TOKEN identifier struct_arg_list  */
#line 230 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                  { yyval = make_node(csound,LINE,LOCN, STRUCT_TOKEN, yyvsp[-1], yyvsp[0]); }
#line 2655 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 11: /* struct_arg_list: struct_arg_list ',' struct_arg  */
#line 234 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = parser_append(csound, yyvsp[-2], yyvsp[0]); }
#line 2661 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 12: /* struct_arg_list: struct_arg_list ',' NEWLINE struct_arg  */
#line 236 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                 { yyval = parser_append(csound, yyvsp[-3], yyvsp[0]); }
#line 2667 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 17: /* $@1: %empty  */
#line 245 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                    { csound_orcput_ilocn(scanner, LINE, LOCN); }
#line 2673 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 18: /* instr_definition: INSTR_TOKEN instr_id_list NEWLINE $@1 statement_list ENDIN_TOKEN NEWLINE  */
#line 247 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                  {  yyval = make_node(csound, (int32_t) csound_orcget_iline(scanner),
                                  csound_orcget_ilocn(scanner), INSTR_TOKEN,
                                  yyvsp[-5], yyvsp[-2]);
                    csp_orc_sa_instr_finalize(csound);
                 }
#line 2683 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 19: /* instr_definition: INSTR_TOKEN NEWLINE error  */
#line 253 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                 { csound->ErrorMsg(csound, Str("No number following instr\n"));
                  csp_orc_sa_instr_finalize(csound);
                  yyval = NULL;
                 }
#line 2692 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 20: /* instr_id_list: instr_id_list ',' instr_id  */
#line 261 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                  { yyval = parser_append(csound, yyvsp[-2], yyvsp[0]); }
#line 2698 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 21: /* instr_id_list: instr_id  */
#line 262 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                          { csp_orc_sa_instr_add_tree(csound, yyvsp[0]);
                    add_instr_variable(csound, yyvsp[0]);
                }
#line 2706 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 25: /* udo_definition: UDOSTART_DEFINITION identifier ',' UDO_IDENT ',' UDO_IDENT NEWLINE statement_list UDOEND_TOKEN NEWLINE  */
#line 275 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              {
                TREE *udoTop = make_leaf(csound, LINE,LOCN, UDO_TOKEN,
                                         (ORCTOKEN *)NULL);
                TREE *ident = yyvsp[-8];
                TREE *udoAns = make_leaf(csound, LINE,LOCN, UDO_ANS_TOKEN,
                                         (ORCTOKEN *)yyvsp[-6]);
                TREE *udoArgs = make_leaf(csound, LINE,LOCN, UDO_ARGS_TOKEN,
                                          (ORCTOKEN *)yyvsp[-4]);
                if (UNLIKELY(PARSER_DEBUG))
                  csound->Message(csound, "UDO COMPLETE\n");

                udoTop->left = ident;
                ident->left = udoAns;
                ident->right = udoArgs;

                udoTop->right = (TREE *)yyvsp[-2];

                yyval = udoTop;

                if (UNLIKELY(PARSER_DEBUG))
                  print_tree(csound, "UDO\n", (TREE *)yyval);

              }
#line 2734 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 26: /* udo_definition: UDOSTART_DEFINITION identifier udo_arg_list ':' udo_out_arg_list NEWLINE statement_list UDOEND_TOKEN NEWLINE  */
#line 300 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              {
                TREE *udoTop = make_leaf(csound, LINE, LOCN, UDO_TOKEN,
                                        (ORCTOKEN*)NULL);
                yyval = udoTop;
                udoTop->left = yyvsp[-7];
                yyvsp[-7]->left = yyvsp[-4];
                yyvsp[-7]->right = yyvsp[-6];
                yyval->right = yyvsp[-2];
              }
#line 2748 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 27: /* udo_definition: UDOSTART_DEFINITION identifier udo_arg_list ':' NEWLINE udo_out_arg_list NEWLINE statement_list UDOEND_TOKEN NEWLINE  */
#line 311 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              {
                TREE *udoTop = make_leaf(csound, LINE, LOCN, UDO_TOKEN,
                                        (ORCTOKEN*)NULL);
                yyval = udoTop;
                udoTop->left = yyvsp[-8];
                yyvsp[-8]->left = yyvsp[-4];
                yyvsp[-8]->right = yyvsp[-7];
                yyval->right = yyvsp[-2];
              }
#line 2762 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 28: /* udo_definition: UDOSTART_DEFINITION identifier udo_arg_list NEWLINE ':' udo_out_arg_list NEWLINE statement_list UDOEND_TOKEN NEWLINE  */
#line 323 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              {
                TREE *udoTop = make_leaf(csound, LINE, LOCN, UDO_TOKEN,
                                        (ORCTOKEN*)NULL);
                yyval = udoTop;
                udoTop->left = yyvsp[-8];
                yyvsp[-8]->left = yyvsp[-4];
                yyvsp[-8]->right = yyvsp[-7];
                yyval->right = yyvsp[-2];
              }
#line 2776 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 29: /* udo_arg_list: '(' out_arg_list ')'  */
#line 336 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
             { yyval = yyvsp[-1];  }
#line 2782 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 30: /* udo_arg_list: '(' out_arg_list_array ')'  */
#line 338 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
             { yyval = yyvsp[-1];  }
#line 2788 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 31: /* udo_arg_list: '(' ')'  */
#line 340 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
             { yyval = make_leaf(csound, LINE, LOCN, T_IDENT, make_token(csound, "0", NULL)); }
#line 2794 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 32: /* udo_out_arg_list: '(' out_type_list ')'  */
#line 344 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
             { yyval = yyvsp[-1]; }
#line 2800 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 33: /* udo_out_arg_list: '(' ')'  */
#line 346 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
             { yyval = make_leaf(csound, LINE, LOCN, T_IDENT, make_token(csound, "0", NULL)); }
#line 2806 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 34: /* udo_out_arg_list: VOID_TOKEN  */
#line 348 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
             { yyval = make_leaf(csound, LINE, LOCN, T_IDENT, make_token(csound, "0", NULL)); }
#line 2812 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 36: /* out_type_list: out_type_list ',' out_type  */
#line 353 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = parser_append(csound, yyvsp[-2], yyvsp[0]); }
#line 2818 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 40: /* opcall: identifier NEWLINE  */
#line 368 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            yyval->left = yyvsp[-1];
          }
#line 2826 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 41: /* opcall: out_arg_list expr_list NEWLINE  */
#line 373 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            yyval->left = yyvsp[-2];
            yyval->right = yyvsp[-1];
          }
#line 2835 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 42: /* opcall: out_arg_list '(' ')' NEWLINE  */
#line 378 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            yyval->left = yyvsp[-3];
            /*$$->right = $2; */
          }
#line 2844 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 43: /* opcall: out_arg_list identifier expr_list NEWLINE  */
#line 383 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            yyval->left = yyvsp[-2];
            yyvsp[-2]->type = T_OPCALL;
            yyvsp[-2]->left = yyvsp[-3];
            yyvsp[-2]->right = yyvsp[-1];
          }
#line 2855 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 44: /* opcall: out_arg_list identifier NEWLINE  */
#line 390 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            if (yyvsp[-1]->value != NULL && yyvsp[-1]->value->lexeme != NULL &&
                strcmp(yyvsp[-1]->value->lexeme, "init") == 0) {
              yyval->left = yyvsp[-1];
              yyvsp[-1]->type = T_OPCALL;
              yyvsp[-1]->left = yyvsp[-2];
            } else {
              yyval->left = yyvsp[-2];
              yyval->right = yyvsp[-1];
            }
          }
#line 2871 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 45: /* opcall: out_arg_list_array expr_list NEWLINE  */
#line 402 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            yyval->left = yyvsp[-2];
            yyval->right = yyvsp[-1];
          }
#line 2880 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 46: /* opcall: out_arg_list_array '(' ')' NEWLINE  */
#line 407 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            yyval->left = yyvsp[-3];
            /*$$->right = $2; */
          }
#line 2889 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 47: /* opcall: out_arg_list_array identifier expr_list NEWLINE  */
#line 412 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            yyval->left = yyvsp[-2];
            yyvsp[-2]->type = T_OPCALL;
            yyvsp[-2]->left = yyvsp[-3];
            yyvsp[-2]->right = yyvsp[-1];
          }
#line 2900 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 48: /* opcall: out_arg_list_array identifier NEWLINE  */
#line 419 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_leaf(csound, LINE,LOCN, T_OPCALL, NULL);
            if (yyvsp[-1]->value != NULL && yyvsp[-1]->value->lexeme != NULL &&
                strcmp(yyvsp[-1]->value->lexeme, "init") == 0) {
              yyval->left = yyvsp[-1];
              yyvsp[-1]->type = T_OPCALL;
              yyvsp[-1]->left = yyvsp[-2];
            } else {
              yyval->left = yyvsp[-2];
              yyval->right = yyvsp[-1];
            }
          }
#line 2916 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 49: /* opcall: function_call NEWLINE  */
#line 431 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = yyvsp[-1]; }
#line 2922 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 50: /* opcall: function_call '+' expr_list NEWLINE  */
#line 433 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '+', yyvsp[-3], yyvsp[-1]);  }
#line 2928 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 51: /* opcall: function_call '-' expr_list NEWLINE  */
#line 435 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '-', yyvsp[-3], yyvsp[-1]); }
#line 2934 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 52: /* opcall: function_call '*' expr_list NEWLINE  */
#line 437 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '*', yyvsp[-3], yyvsp[-1]); }
#line 2940 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 53: /* opcall: function_call '/' expr_list NEWLINE  */
#line 439 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '/', yyvsp[-3], yyvsp[-1]); }
#line 2946 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 54: /* opcall: function_call '^' expr_list NEWLINE  */
#line 441 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '^', yyvsp[-3], yyvsp[-1]); }
#line 2952 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 55: /* opcall: function_call '%' expr_list NEWLINE  */
#line 443 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '%', yyvsp[-3], yyvsp[-1]); }
#line 2958 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 56: /* opcall: function_call '|' expr_list NEWLINE  */
#line 445 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '|', yyvsp[-3], yyvsp[-1]); }
#line 2964 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 57: /* opcall: function_call '&' expr_list NEWLINE  */
#line 447 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '&', yyvsp[-3], yyvsp[-1]); }
#line 2970 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 58: /* opcall: function_call '#' expr_list NEWLINE  */
#line 449 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = make_opcall_from_func_start(csound, LINE, LOCN, '#', yyvsp[-3], yyvsp[-1]); }
#line 2976 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 59: /* function_call: typed_identifierb expr_list ')'  */
#line 453 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = yyvsp[-2];
                yyvsp[-2]->type = T_FUNCTION;
                yyvsp[-2]->right = yyvsp[-1]; }
#line 2984 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 60: /* function_call: typed_identifierb ')'  */
#line 457 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = yyvsp[-1];
                yyvsp[-1]->type = T_FUNCTION; }
#line 2991 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 61: /* function_call: identifierb expr_list ')'  */
#line 460 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = yyvsp[-2];
                yyvsp[-2]->type = T_FUNCTION;
                yyvsp[-2]->right = yyvsp[-1]; }
#line 2999 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 62: /* function_call: identifierb ')'  */
#line 464 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = yyvsp[-1];
                yyvsp[-1]->type = T_FUNCTION; }
#line 3006 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 63: /* statement_list: statement_list statement  */
#line 469 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                {
                    yyval = parser_append(csound, (TREE *)yyvsp[-1], (TREE *)yyvsp[0]);
                }
#line 3014 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 65: /* statement_list: %empty  */
#line 473 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                  {
                    /* This rule allows for empty statement lists, but
                    in turn causes a lot of shift/reduce errors to be
                    reported.  The parser works with this, but we should
                    perhaps look at expanding the other rules to work
                    without statement_list in them. */
                    yyval = NULL;
                  }
#line 3027 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 66: /* statement: out_arg_list assignment expr_list NEWLINE  */
#line 484 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                {
                  yyval = (TREE *)yyvsp[-2];
                  yyval->left = (TREE *)yyvsp[-3];

                  if(yyvsp[-2]->right != NULL) {
                    TREE* op = yyvsp[-2]->right;
                    yyvsp[-2]->right = NULL;
                    op->right = (TREE *)yyvsp[-1];
                    op->left = copy_node(csound, yyvsp[-3]);
                    yyval->right = op;
                  } else {
                    yyval->right = (TREE *)yyvsp[-1];
                  }
                }
#line 3046 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 67: /* statement: out_arg_list_array assignment_array expr_list NEWLINE  */
#line 499 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                {
                  yyval = (TREE *)yyvsp[-2];
                  yyval->left = (TREE *)yyvsp[-3];

                  if(yyvsp[-2]->right != NULL) {
                    TREE* op = yyvsp[-2]->right;
                    yyvsp[-2]->right = NULL;
                    op->right = (TREE *)yyvsp[-1];
                    op->left = copy_node(csound, yyvsp[-3]);
                    yyval->right = op;
                  } else {
                    yyval->right = (TREE *)yyvsp[-1];
                  }
                }
#line 3065 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 69: /* statement: goto identifier NEWLINE  */
#line 515 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                {
                    yyvsp[-2]->left = NULL;
                    yyvsp[-2]->right = yyvsp[-1];
                    yyval = yyvsp[-2];
                }
#line 3075 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 76: /* statement: BREAK_TOKEN  */
#line 528 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound, LINE, LOCN, BREAK_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 3081 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 77: /* statement: CONTINUE_TOKEN  */
#line 530 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound, LINE, LOCN, CONTINUE_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 3087 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 78: /* statement: LABEL_TOKEN  */
#line 532 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound, LINE, LOCN, LABEL_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 3093 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 79: /* statement: NEWLINE  */
#line 534 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = NULL; }
#line 3099 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 80: /* if_goto: IF_TOKEN expr goto T_IDENT NEWLINE  */
#line 540 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              {
                  yyvsp[-2]->left = NULL;
                  yyvsp[-2]->right = make_leaf(csound, LINE,LOCN,
                                        T_IDENT, (ORCTOKEN *)yyvsp[-1]);
                  yyval = make_node(csound,LINE,LOCN, IF_TOKEN, yyvsp[-3], yyvsp[-2]);
              }
#line 3110 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 81: /* if_then: if_then_base ENDIF_TOKEN NEWLINE  */
#line 549 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = yyvsp[-2]; }
#line 3116 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 82: /* if_then: if_then_base ELSE_TOKEN statement_list ENDIF_TOKEN NEWLINE  */
#line 551 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = yyvsp[-4];
            yyval->right->next = make_node(csound,LINE,LOCN, ELSE_TOKEN, NULL, yyvsp[-2]); }
#line 3123 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 83: /* if_then: if_then_base elseif_list ENDIF_TOKEN NEWLINE  */
#line 554 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = yyvsp[-3];
            yyval->right->next = yyvsp[-2]; }
#line 3130 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 84: /* if_then: if_then_base elseif_list ELSE_TOKEN statement_list ENDIF_TOKEN NEWLINE  */
#line 557 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { TREE * tempLastNode;
            yyval = yyvsp[-5];
            yyval->right->next = yyvsp[-4];

            tempLastNode = yyval;

            while (tempLastNode->right!=NULL && tempLastNode->right->next!=NULL) {
              tempLastNode = tempLastNode->right->next;
            }
            tempLastNode->right->next = make_node(csound, LINE,LOCN, ELSE_TOKEN, NULL, yyvsp[-2]);
            }
#line 3146 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 85: /* if_then_base: IF_TOKEN expr then NEWLINE statement_list  */
#line 571 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyvsp[-2]->right = yyvsp[0];
                yyval = make_node(csound,LINE,LOCN, IF_TOKEN, yyvsp[-3], yyvsp[-2]); }
#line 3153 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 86: /* elseif_list: elseif_list elseif  */
#line 576 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { TREE * tempLastNode = yyvsp[-1];
                while (tempLastNode->right!=NULL &&
                  tempLastNode->right->next!=NULL) {
                  tempLastNode = tempLastNode->right->next;
                }
                tempLastNode->right->next = yyvsp[0];
                yyval = yyvsp[-1]; }
#line 3165 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 87: /* elseif_list: elseif  */
#line 583 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                     { yyval = yyvsp[0]; }
#line 3171 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 88: /* elseif: ELSEIF_TOKEN expr then NEWLINE statement_list  */
#line 587 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyvsp[-2]->right = yyvsp[0];
              yyval = make_node(csound,LINE,LOCN, ELSEIF_TOKEN, yyvsp[-3], yyvsp[-2]); }
#line 3178 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 89: /* until: UNTIL_TOKEN expr DO_TOKEN statement_list OD_TOKEN  */
#line 592 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = make_leaf(csound,LINE,LOCN, UNTIL_TOKEN, (ORCTOKEN *)yyvsp[-4]);
                yyval->left = yyvsp[-3];
                yyval->right = yyvsp[-1]; }
#line 3186 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 90: /* while: WHILE_TOKEN expr DO_TOKEN statement_list OD_TOKEN  */
#line 598 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = make_leaf(csound,LINE,LOCN, WHILE_TOKEN, (ORCTOKEN *)yyvsp[-4]);
                yyval->left = yyvsp[-3];
                yyval->right = yyvsp[-1]; }
#line 3194 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 91: /* case: CASE_TOKEN expr_list NEWLINE statement_list  */
#line 604 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
      {
        yyval = make_leaf(csound, LINE, LOCN, CASE_TOKEN, (ORCTOKEN *)yyvsp[-3]);
        yyval->left = yyvsp[-2];
        yyval->right = yyvsp[0];
      }
#line 3204 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 92: /* case: DEFAULT_TOKEN NEWLINE statement_list  */
#line 610 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
      {
        yyval = make_leaf(csound, LINE, LOCN, DEFAULT_TOKEN, (ORCTOKEN *)yyvsp[-2]);
        yyval->right = yyvsp[0];
      }
#line 3213 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 93: /* case_list: case_list case  */
#line 617 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { TREE * tempLastNode = yyvsp[-1];
                while (tempLastNode->next != NULL) {
                  tempLastNode = tempLastNode->next;
                }
                tempLastNode->next = yyvsp[0];
                yyval = yyvsp[-1]; }
#line 3224 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 94: /* case_list: case  */
#line 623 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                   { yyval = yyvsp[0]; }
#line 3230 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 95: /* case_list: NEWLINE case_list  */
#line 625 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
             {
              yyval = yyvsp[0];
             }
#line 3238 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 96: /* switch: SWITCH_TOKEN expr NEWLINE case_list ENDSW_TOKEN  */
#line 631 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
          yyval = make_leaf(csound,LINE,LOCN, SWITCH_TOKEN, (ORCTOKEN *)yyvsp[-4]);
          yyval->left = yyvsp[-3];
          yyval->right = yyvsp[-1];
        }
#line 3248 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 97: /* for_in: FOR_TOKEN identifier in expr DO_TOKEN statement_list OD_TOKEN  */
#line 639 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
          yyvsp[-4]->left = yyvsp[-3];
          yyvsp[-4]->right = yyvsp[-1];
          yyval = make_node(csound,LINE,LOCN, FOR_TOKEN, yyvsp[-5], yyvsp[-4]);
        }
#line 3258 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 98: /* for_in: FOR_TOKEN identifier ',' identifier in expr DO_TOKEN statement_list OD_TOKEN  */
#line 645 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
          yyvsp[-7]->next = yyvsp[-5];
          yyvsp[-4]->left = yyvsp[-3];
          yyvsp[-4]->right = yyvsp[-1];
          yyval = make_node(csound,LINE,LOCN, FOR_TOKEN, yyvsp[-7], yyvsp[-4]);
        }
#line 3269 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 99: /* for_in: FOR_TOKEN typed_identifier in expr DO_TOKEN statement_list OD_TOKEN  */
#line 652 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
          yyvsp[-4]->left = yyvsp[-3];
          yyvsp[-4]->right = yyvsp[-1];
          yyval = make_node(csound,LINE,LOCN, FOR_TOKEN,
                         make_leaf(csound,LINE,LOCN, T_TYPED_IDENT, 
                                   lookup_token(csound, yyvsp[-5]->value->lexeme, NULL)), yyvsp[-4]);
        }
#line 3281 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 100: /* for_in: FOR_TOKEN typed_identifier ',' identifier in expr DO_TOKEN statement_list OD_TOKEN  */
#line 660 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
          
          yyvsp[-4]->left = yyvsp[-3];
          yyvsp[-4]->right = yyvsp[-1];
          yyval = make_leaf(csound,LINE,LOCN, T_TYPED_IDENT, 
                         lookup_token(csound, yyvsp[-7]->value->lexeme, NULL));
          yyval->next = yyvsp[-5];
          yyval = make_node(csound,LINE,LOCN, FOR_TOKEN, yyval, yyvsp[-4]);
          }
#line 3295 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 101: /* for_in: FOR_TOKEN typed_identifier ',' typed_identifier in expr DO_TOKEN statement_list OD_TOKEN  */
#line 670 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
          yyvsp[-4]->left = yyvsp[-3];
          yyvsp[-4]->right = yyvsp[-1];
          yyval = make_leaf(csound,LINE,LOCN, T_TYPED_IDENT, 
                         lookup_token(csound, yyvsp[-7]->value->lexeme, NULL));
          yyval->next = make_leaf(csound,LINE,LOCN, T_TYPED_IDENT, 
                         lookup_token(csound, yyvsp[-5]->value->lexeme, NULL));
          yyval = make_node(csound,LINE,LOCN, FOR_TOKEN, yyval, yyvsp[-4]);
          }
#line 3309 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 102: /* for_in: FOR_TOKEN identifier ',' typed_identifier in expr DO_TOKEN statement_list OD_TOKEN  */
#line 680 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
          yyvsp[-7]->next = make_leaf(csound,LINE,LOCN, T_TYPED_IDENT, 
                         lookup_token(csound, yyvsp[-5]->value->lexeme, NULL));
          yyvsp[-4]->left = yyvsp[-3];
          yyvsp[-4]->right = yyvsp[-1];
          yyval = make_node(csound,LINE,LOCN, FOR_TOKEN, yyvsp[-7], yyvsp[-4]);
        }
#line 3321 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 103: /* declare_definition: DECLARE_TOKEN identifier udo_arg_list ':' udo_out_arg_list NEWLINE  */
#line 690 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
 {
   yyval = make_leaf(csound, LINE, LOCN, T_DECLARE, make_token(csound, yyvsp[-4]->value->lexeme, NULL));
   yyval->left = yyvsp[-4];
   yyval->left->left = yyvsp[-1];
   yyval->left->right = yyvsp[-3];
 }
#line 3332 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 104: /* expr_list: expr_list ',' expr  */
#line 699 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = parser_append(csound, yyvsp[-2], yyvsp[0]); }
#line 3338 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 105: /* expr_list: expr_list ',' NEWLINE expr  */
#line 701 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = parser_append(csound, yyvsp[-3], yyvsp[0]); }
#line 3344 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 108: /* expr: '(' expr ')'  */
#line 707 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          { yyval = yyvsp[-1] ; }
#line 3350 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 109: /* expr: '(' expr error  */
#line 708 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL;  }
#line 3356 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 110: /* expr: '(' error  */
#line 709 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL; }
#line 3362 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 125: /* gen_array: '[' expr S_ELIPSIS2 expr_list ']'  */
#line 727 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                               {
            yyval = make_leaf(csound, LINE,LOCN, T_FUNCTION, make_token(csound, "genarray", NULL));
            yyval->right = yyvsp[-3];
            parser_append(csound, yyval->right, yyvsp[-1]);
             }
#line 3372 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 126: /* slice_array: identifier '[' expr ':' expr_list ']'  */
#line 734 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                                 {
            yyval = make_leaf(csound,LINE,LOCN, T_FUNCTION, make_token(csound, "slicearray", NULL));
            yyval->right = yyvsp[-5];
            yyval->right = parser_append(csound, yyval->right, yyvsp[-3]);
            parser_append(csound, yyval->right, yyvsp[-1]);
           }
#line 3383 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 127: /* slice_array: identifier '[' ':' expr_list ']'  */
#line 741 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                             {
            yyval = make_leaf(csound,LINE,LOCN, T_FUNCTION, make_token(csound, "slicearray", NULL));
            yyval->right = yyvsp[-4];
            yyval->right = parser_append(csound, yyval->right,
                                       make_leaf(csound,LINE,LOCN, T_IDENT,
                                                 make_int(csound, "0", NULL)));
            parser_append(csound, yyval->right, yyvsp[-1]);
           }
#line 3396 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 128: /* slice_array: identifier '[' expr ':' ']'  */
#line 750 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                       {
            yyval = make_leaf(csound,LINE,LOCN, T_FUNCTION, make_token(csound, "slicearray", NULL));
            yyval->right = yyvsp[-4];
            yyval->right = parser_append(csound, yyval->right, yyvsp[-2]);
           }
#line 3406 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 129: /* slice_array: expr '[' expr ':' expr_list ']'  */
#line 756 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           {
            yyval = make_leaf(csound,LINE,LOCN, T_FUNCTION, make_token(csound, "slicearray", NULL));
            yyval->right = yyvsp[-5];
            yyval->right = parser_append(csound, yyval->right, yyvsp[-3]);
            parser_append(csound, yyval->right, yyvsp[-1]);
           }
#line 3417 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 130: /* slice_array: expr '[' ':' expr_list ']'  */
#line 763 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                       {
            yyval = make_leaf(csound,LINE,LOCN, T_FUNCTION, make_token(csound, "slicearray", NULL));
            yyval->right = yyvsp[-4];
            yyval->right = parser_append(csound, yyval->right,
                                       make_leaf(csound,LINE,LOCN, T_IDENT,
                                                 make_int(csound, "0", NULL)));
            parser_append(csound, yyval->right, yyvsp[-1]);
           }
#line 3430 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 131: /* slice_array: expr '[' expr ':' ']'  */
#line 772 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                 {
            yyval = make_leaf(csound,LINE,LOCN, T_FUNCTION, make_token(csound, "slicearray", NULL));
            yyval->right = yyvsp[-4];
            yyval->right = parser_append(csound, yyval->right, yyvsp[-2]);
           }
#line 3440 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 132: /* static_array: '[' expr_list ']'  */
#line 779 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                 {
            yyval = make_leaf(csound,LINE,LOCN, T_FUNCTION, make_token(csound, "fillarray", NULL));
            yyval->right = yyvsp[-1];
          }
#line 3449 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 133: /* array_expr: array_expr '[' expr ']'  */
#line 788 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          {
            parser_append(csound, yyvsp[-3]->right, yyvsp[-1]);
            yyval = yyvsp[-3];
          }
#line 3458 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 134: /* array_expr: identifier '[' expr ']'  */
#line 793 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          {
           char* arrayName = yyvsp[-3]->value->lexeme;
            yyval = make_node(csound, LINE, LOCN, T_ARRAY,
                           make_leaf(csound, LINE, LOCN, T_IDENT, make_token(csound, arrayName, NULL)), yyvsp[-1]);
          }
#line 3468 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 135: /* array_expr: function_call '[' expr ']'  */
#line 799 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          {
            yyval = make_node(csound, LINE, LOCN, T_ARRAY, yyvsp[-3], yyvsp[-1]);
          }
#line 3476 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 136: /* struct_expr: struct_expr '.' identifier  */
#line 805 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            {
              char* memberName = yyvsp[0]->value->lexeme;
              // Important: Clear the next pointer of $3 to prevent it from being processed separately
              yyvsp[0]->next = NULL;
              yyval = make_node(
                csound, LINE, LOCN, STRUCT_EXPR,
                yyvsp[-2],
                make_leaf(
                          csound, LINE, LOCN, T_MEMBER_IDENT, make_token(csound, memberName, NULL)
                )
              );
            }
#line 3493 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 137: /* struct_expr: struct_expr '.' array_expr  */
#line 818 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            {
              /* Build a struct member node from array_expr's base identifier, then
                 attach the array indexing so nested chains like a.b[0] work. */
              char* memberName = yyvsp[0]->value == NULL ?
                                  yyvsp[0]->left->value->lexeme :
                                  yyvsp[0]->value->lexeme;
              TREE* memberLeaf = make_leaf(csound, LINE, LOCN, T_MEMBER_IDENT,
                                           make_token(csound, memberName, NULL));
              TREE* structMember = make_node(csound, LINE, LOCN, STRUCT_EXPR, yyvsp[-2], memberLeaf);
              /* Now make the array_expr index the struct member */
              yyvsp[0]->left = structMember;
              yyval = yyvsp[0];
            }
#line 3511 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 138: /* struct_expr: array_expr '.' identifier  */
#line 832 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            {
              yyvsp[0]->type = T_MEMBER_IDENT;
              yyval = make_node(csound, LINE, LOCN, STRUCT_EXPR, yyvsp[-2], yyvsp[0]);
            }
#line 3520 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 139: /* struct_expr: identifier '.' array_expr  */
#line 837 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            {
              char* structName = yyvsp[-2]->value->lexeme;
              char* memberName = yyvsp[0]->value == NULL ?
                yyvsp[0]->left->value->lexeme :
                yyvsp[0]->value->lexeme;

              yyval = make_node(csound, LINE, LOCN, STRUCT_EXPR,
                             make_leaf(csound, LINE, LOCN, T_IDENT, make_token(csound, structName, NULL)),
                             make_leaf(csound, LINE, LOCN, T_MEMBER_IDENT, make_token(csound, memberName, NULL))
              );
              yyvsp[0]->left = yyval;
              yyval = yyvsp[0];
            }
#line 3538 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 140: /* struct_expr: struct_expr '[' expr ']'  */
#line 851 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            {
              yyval = make_node(csound, LINE, LOCN, T_ARRAY, yyvsp[-3], yyvsp[-1]);
            }
#line 3546 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 141: /* struct_expr: identifier '.' identifier  */
#line 855 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            {
              char* structName = yyvsp[-2]->value->lexeme;
              char* memberName = yyvsp[0]->value->lexeme;
              // Important: Clear the next pointer of $3 to prevent it from being processed separately
              yyvsp[0]->next = NULL;
              yyval = make_node(csound, LINE, LOCN, STRUCT_EXPR,
                             make_leaf(csound, LINE, LOCN, T_IDENT, make_token(csound, structName, NULL)),
                             make_leaf(csound, LINE, LOCN, T_MEMBER_IDENT, make_token(csound, memberName, NULL))
                   );
            }
#line 3561 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 142: /* ternary_expr: expr '?' expr ':' expr  */
#line 868 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_node(csound,LINE,LOCN, '?', yyvsp[-4],
                             make_node(csound, LINE,LOCN, ':', yyvsp[-2], yyvsp[0])); }
#line 3568 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 143: /* ternary_expr: expr '?' expr ':' error  */
#line 870 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                    { yyval = NULL; }
#line 3574 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 144: /* ternary_expr: expr '?' expr error  */
#line 871 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                { yyval = NULL; }
#line 3580 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 145: /* ternary_expr: expr '?' error  */
#line 872 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3586 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 146: /* unary_expr: '~' expr  */
#line 876 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_node(csound, LINE,LOCN, '~', NULL, yyvsp[0]);}
#line 3592 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 147: /* unary_expr: '~' error  */
#line 877 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL; }
#line 3598 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 148: /* unary_expr: '!' expr  */
#line 878 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                { yyval = make_node(csound, LINE,LOCN,
                                                    S_UNOT, yyvsp[0], NULL); }
#line 3605 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 149: /* unary_expr: '!' error  */
#line 880 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                              { yyval = NULL; }
#line 3611 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 150: /* unary_expr: '-' expr  */
#line 882 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          {
              yyval = make_node(csound,LINE,LOCN, S_UMINUS, NULL, yyvsp[0]);
          }
#line 3619 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 151: /* unary_expr: '-' error  */
#line 885 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                              { yyval = NULL; }
#line 3625 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 152: /* unary_expr: '+' expr  */
#line 904 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
          {
              yyval = make_node(csound,LINE,LOCN, S_UPLUS, NULL, yyvsp[0]);
          }
#line 3633 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 153: /* unary_expr: '+' error  */
#line 908 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                              { yyval = NULL; }
#line 3639 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 154: /* binary_expr: expr '+' optnewline expr  */
#line 911 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                         { yyval = make_node(csound, LINE,LOCN, '+', yyvsp[-3], yyvsp[0]); }
#line 3645 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 155: /* binary_expr: expr '+' error  */
#line 912 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3651 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 156: /* binary_expr: expr '-' optnewline expr  */
#line 913 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                      { yyval = make_node(csound ,LINE,LOCN, '-', yyvsp[-3], yyvsp[0]); }
#line 3657 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 157: /* binary_expr: expr '-' error  */
#line 914 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3663 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 158: /* binary_expr: expr S_LE optnewline expr  */
#line 915 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           { yyval = make_node(csound, LINE,LOCN, S_LE, yyvsp[-3], yyvsp[0]); }
#line 3669 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 159: /* binary_expr: expr S_LE error  */
#line 916 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL; }
#line 3675 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 160: /* binary_expr: expr S_GE optnewline expr  */
#line 917 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           { yyval = make_node(csound, LINE,LOCN, S_GE, yyvsp[-3], yyvsp[0]); }
#line 3681 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 161: /* binary_expr: expr S_GE error  */
#line 918 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                { yyval = NULL; }
#line 3687 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 162: /* binary_expr: expr S_NEQ optnewline expr  */
#line 919 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           { yyval = make_node(csound, LINE,LOCN, S_NEQ, yyvsp[-3], yyvsp[0]); }
#line 3693 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 163: /* binary_expr: expr S_NEQ error  */
#line 920 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                { yyval = NULL; }
#line 3699 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 164: /* binary_expr: expr '=' optnewline expr  */
#line 922 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                      { yyval = make_node(csound, LINE,LOCN, S_EQ, yyvsp[-3], yyvsp[0]); }
#line 3705 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 165: /* binary_expr: expr '=' error  */
#line 923 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3711 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 166: /* binary_expr: expr S_EQ optnewline expr  */
#line 924 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           { yyval = make_node(csound, LINE,LOCN, S_EQ, yyvsp[-3], yyvsp[0]); }
#line 3717 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 167: /* binary_expr: expr S_EQ error  */
#line 925 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL; }
#line 3723 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 168: /* binary_expr: expr S_GT optnewline expr  */
#line 926 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           { yyval = make_node(csound, LINE,LOCN, S_GT, yyvsp[-3], yyvsp[0]); }
#line 3729 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 169: /* binary_expr: expr S_GT error  */
#line 927 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL; }
#line 3735 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 170: /* binary_expr: expr S_LT optnewline expr  */
#line 928 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           { yyval = make_node(csound, LINE,LOCN, S_LT, yyvsp[-3], yyvsp[0]); }
#line 3741 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 171: /* binary_expr: expr S_LT error  */
#line 929 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL; }
#line 3747 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 172: /* binary_expr: expr S_AND optnewline expr  */
#line 930 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                         { yyval = make_node(csound, LINE,LOCN, S_AND, yyvsp[-3], yyvsp[0]); }
#line 3753 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 173: /* binary_expr: expr S_AND error  */
#line 931 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                             { yyval = NULL; }
#line 3759 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 174: /* binary_expr: expr S_OR optnewline expr  */
#line 932 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                         { yyval = make_node(csound, LINE,LOCN, S_OR, yyvsp[-3], yyvsp[0]); }
#line 3765 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 175: /* binary_expr: expr S_OR error  */
#line 933 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                            { yyval = NULL; }
#line 3771 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 176: /* binary_expr: expr '*' optnewline expr  */
#line 934 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                        { yyval = make_node(csound, LINE,LOCN, '*', yyvsp[-3], yyvsp[0]); }
#line 3777 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 177: /* binary_expr: expr '*' error  */
#line 935 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3783 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 178: /* binary_expr: expr '/' optnewline expr  */
#line 936 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                        { yyval = make_node(csound, LINE,LOCN, '/', yyvsp[-3], yyvsp[0]); }
#line 3789 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 179: /* binary_expr: expr '/' error  */
#line 937 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3795 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 180: /* binary_expr: expr '^' optnewline expr  */
#line 938 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                        { yyval = make_node(csound, LINE,LOCN, '^', yyvsp[-3], yyvsp[0]); }
#line 3801 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 181: /* binary_expr: expr '^' error  */
#line 939 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3807 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 182: /* binary_expr: expr '%' optnewline expr  */
#line 940 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                        { yyval = make_node(csound, LINE,LOCN, '%', yyvsp[-3], yyvsp[0]); }
#line 3813 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 183: /* binary_expr: expr '%' error  */
#line 941 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3819 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 184: /* binary_expr: expr '|' optnewline expr  */
#line 942 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                            { yyval = make_node(csound, LINE,LOCN, '|', yyvsp[-3], yyvsp[0]); }
#line 3825 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 185: /* binary_expr: expr '|' error  */
#line 943 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3831 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 186: /* binary_expr: expr '&' optnewline expr  */
#line 944 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                            { yyval = make_node(csound, LINE,LOCN, '&', yyvsp[-3], yyvsp[0]); }
#line 3837 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 187: /* binary_expr: expr '&' error  */
#line 945 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3843 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 188: /* binary_expr: expr '#' optnewline expr  */
#line 946 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                            { yyval = make_node(csound, LINE,LOCN, '#', yyvsp[-3], yyvsp[0]); }
#line 3849 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 189: /* binary_expr: expr '#' error  */
#line 947 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                           { yyval = NULL; }
#line 3855 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 190: /* binary_expr: expr S_BITSHIFT_LEFT optnewline expr  */
#line 949 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                 { yyval = make_node(csound, LINE,LOCN, S_BITSHIFT_LEFT, yyvsp[-3], yyvsp[0]); }
#line 3861 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 191: /* binary_expr: expr S_BITSHIFT_LEFT error  */
#line 950 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                       { yyval = NULL; }
#line 3867 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 192: /* binary_expr: expr S_BITSHIFT_RIGHT optnewline expr  */
#line 952 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                 { yyval = make_node(csound, LINE,LOCN, S_BITSHIFT_RIGHT, yyvsp[-3], yyvsp[0]); }
#line 3873 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 193: /* binary_expr: expr S_BITSHIFT_RIGHT error  */
#line 953 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                        { yyval = NULL; }
#line 3879 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 194: /* out_arg_list: out_arg_list ',' out_arg  */
#line 958 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = parser_append(csound, yyvsp[-2], yyvsp[0]); }
#line 3885 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 200: /* out_arg_list_array: out_arg_list_array ',' array_expr  */
#line 969 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
              { yyval = parser_append(csound, yyvsp[-2], yyvsp[0]); }
#line 3891 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 202: /* array_identifier: array_identifier '[' ']'  */
#line 973 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                           {
            parser_append(csound, yyvsp[-2]->right,
                           make_leaf(csound, LINE, LOCN, '[', make_token(csound, "[", NULL)));
            yyval = yyvsp[-2];
          }
#line 3901 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 203: /* array_identifier: identifier '[' ']'  */
#line 978 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                               {
            yyval = yyvsp[-2];
            yyvsp[-2]->type = T_ARRAY_IDENT;
            yyval->right = make_leaf(csound, LINE, LOCN, '[', make_token(csound, "[", NULL));
          }
#line 3911 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 204: /* array_identifier: typed_identifier '[' ']'  */
#line 983 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                                     {
            yyval = yyvsp[-2];
            // Check if this is a type annotation (e.g., "var:Type[]") vs array access
            // If the typed_identifier already has a type annotation ending with "[]",
            // keep it as T_TYPED_IDENT rather than converting to T_ARRAY_IDENT
            if (yyvsp[-2]->value && yyvsp[-2]->value->optype) {
              size_t len = strlen(yyvsp[-2]->value->optype);
              if (len >= 2 && yyvsp[-2]->value->optype[len-2] == '[' && yyvsp[-2]->value->optype[len-1] == ']') {
                // This is a type annotation like "Person[]", keep as T_TYPED_IDENT
                // Don't attach the '[' token or change the type
              } else {
                // This is array access syntax, convert to T_ARRAY_IDENT
                yyvsp[-2]->type = T_ARRAY_IDENT;
                yyval->right = make_leaf(csound, LINE, LOCN, '[', make_token(csound, "[", NULL));
              }
            } else {
              // No type annotation, treat as array access
              yyvsp[-2]->type = T_ARRAY_IDENT;
              yyval->right = make_leaf(csound, LINE, LOCN, '[', make_token(csound, "[", NULL));
            }
          }
#line 3937 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 205: /* assignment: '='  */
#line 1008 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, T_ASSIGNMENT, make_token(csound, "=", NULL)); }
#line 3943 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 206: /* assignment: S_ADDIN  */
#line 1010 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, S_ADDIN, make_token(csound, "##addin", NULL)); }
#line 3949 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 207: /* assignment: S_SUBIN  */
#line 1012 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, S_SUBIN, make_token(csound, "##subin", NULL)); }
#line 3955 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 208: /* assignment: S_DIVIN  */
#line 1014 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, S_DIVIN, make_token(csound, "##divin", NULL)); }
#line 3961 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 209: /* assignment: S_MULIN  */
#line 1016 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, S_MULIN, make_token(csound, "##mulin", NULL)); }
#line 3967 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 210: /* assignment_array: '='  */
#line 1021 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, T_ASSIGNMENT, make_token(csound, "=", NULL)); }
#line 3973 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 211: /* assignment_array: S_ADDIN  */
#line 1023 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, T_ASSIGNMENT, make_token(csound, "=", NULL));
                  yyval->right = make_leaf(csound, LINE, LOCN, '+', make_token(csound, "+", NULL));
                }
#line 3981 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 212: /* assignment_array: S_SUBIN  */
#line 1027 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, T_ASSIGNMENT, make_token(csound, "=", NULL));
                  yyval->right = make_leaf(csound, LINE, LOCN, '-', make_token(csound, "-", NULL));
                }
#line 3989 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 213: /* assignment_array: S_DIVIN  */
#line 1031 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, T_ASSIGNMENT, make_token(csound, "=", NULL));
                  yyval->right = make_leaf(csound, LINE, LOCN, '/', make_token(csound, "/", NULL));
                }
#line 3997 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 214: /* assignment_array: S_MULIN  */
#line 1035 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
                { yyval = make_leaf(csound,LINE,LOCN, T_ASSIGNMENT, make_token(csound, "=", NULL));
                  yyval->right = make_leaf(csound, LINE, LOCN, '*', make_token(csound, "*", NULL));
                }
#line 4005 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 215: /* in: IN_TOKEN  */
#line 1041 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound,LINE,LOCN, IN_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4011 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 216: /* then: THEN_TOKEN  */
#line 1044 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound,LINE,LOCN, THEN_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4017 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 217: /* then: KTHEN_TOKEN  */
#line 1046 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound,LINE,LOCN, KTHEN_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4023 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 218: /* then: ITHEN_TOKEN  */
#line 1048 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound,LINE,LOCN, ITHEN_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4029 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 219: /* goto: GOTO_TOKEN  */
#line 1052 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound,LINE,LOCN, GOTO_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4035 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 220: /* goto: KGOTO_TOKEN  */
#line 1054 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound,LINE,LOCN, KGOTO_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4041 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 221: /* goto: IGOTO_TOKEN  */
#line 1056 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
            { yyval = make_leaf(csound,LINE,LOCN, IGOTO_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4047 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 224: /* string: STRING_TOKEN  */
#line 1064 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        { yyval = make_leaf(csound, LINE,LOCN, STRING_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4053 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 225: /* false_const: FALSE_TOKEN  */
#line 1068 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
       { yyval = make_leaf(csound, LINE,LOCN, FALSE_TOKEN,
                        make_token(csound,"false", NULL)); }
#line 4060 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 226: /* false_const: FALSEK_TOKEN  */
#line 1071 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
       { yyval = make_leaf(csound, LINE,LOCN, FALSEK_TOKEN,
                        make_token(csound,"falsek", NULL)); }
#line 4067 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 227: /* true_const: TRUE_TOKEN  */
#line 1077 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
           { yyval = make_leaf(csound, LINE,LOCN, TRUE_TOKEN,
                            make_token(csound,"true", NULL)); }
#line 4074 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 228: /* true_const: TRUEK_TOKEN  */
#line 1080 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
       { yyval = make_leaf(csound, LINE,LOCN, TRUEK_TOKEN,
                        make_token(csound,"truek", NULL)); }
#line 4081 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 229: /* number: NUMBER_TOKEN  */
#line 1086 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
       { yyval = make_leaf(csound, LINE,LOCN, NUMBER_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4087 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 230: /* integer: INTEGER_TOKEN  */
#line 1090 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        { yyval = make_leaf(csound, LINE, LOCN, INTEGER_TOKEN, (ORCTOKEN *)yyvsp[0]); }
#line 4093 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 231: /* plus_identifier: '+' T_IDENT  */
#line 1098 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        {
	  yyval = make_leaf(csound, LINE, LOCN, T_PLUS_IDENT, (ORCTOKEN *)yyvsp[0]);
	}
#line 4101 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 232: /* typed_identifier: T_TYPED_IDENT  */
#line 1104 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        { yyval = make_leaf(csound, LINE, LOCN, T_TYPED_IDENT, (ORCTOKEN *)yyvsp[0]); }
#line 4107 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 233: /* typed_identifierb: T_TYPED_IDENTB  */
#line 1108 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        { yyval = make_leaf(csound, LINE, LOCN, T_TYPED_IDENT, (ORCTOKEN *)yyvsp[0]); }
#line 4113 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 234: /* identifier: T_IDENT  */
#line 1112 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        { yyval = make_leaf(csound, LINE, LOCN, T_IDENT, (ORCTOKEN *)yyvsp[0]); }
#line 4119 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;

  case 235: /* identifierb: T_IDENTB  */
#line 1116 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"
        { yyval = make_leaf(csound, LINE, LOCN, T_IDENT, (ORCTOKEN *)yyvsp[0]); }
#line 4125 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"
    break;


#line 4129 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-arm64/csound_orcparse.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (parm, scanner, csound, astTree, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, parm, scanner, csound, astTree);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, parm, scanner, csound, astTree);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (parm, scanner, csound, astTree, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, parm, scanner, csound, astTree);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, parm, scanner, csound, astTree);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 1119 "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/Engine/csound_orc.y"


#ifdef SOME_FINE_DAY
void
yyerror(char *s, ...)
{
  va_list ap;
  va_start(ap, s);

  if (yylloc.first_line)
    fprintf(stderr, "%d.%d-%d.%d: error: ",
            yylloc.first_line, yylloc.first_column,
            yylloc.last_line, yylloc.last_column);
  vfprintf(stderr, s, ap);
  fprintf(stderr, "\n");

}

void
lyyerror(YYLTYPE t, char *s, ...)
{
  va_list ap;
  va_start(ap, s);

  if (t.first_line)
    fprintf(stderr, "%d.%d-%d.%d: error: ", t.first_line, t.first_column,
            t.last_line, t.last_column);
  vfprintf(stderr, s, ap);
  fprintf(stderr, "\n");
}

#endif
