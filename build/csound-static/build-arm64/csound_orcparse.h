/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_CSOUND_ORC_USERS_ALESSANDROPETROLATI_DESKTOP_ADVANCED_2018_CSOUND_BUILD_CSOUND_STATIC_BUILD_ARM64_CSOUND_ORCPARSE_H_INCLUDED
# define YY_CSOUND_ORC_USERS_ALESSANDROPETROLATI_DESKTOP_ADVANCED_2018_CSOUND_BUILD_CSOUND_STATIC_BUILD_ARM64_CSOUND_ORCPARSE_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int csound_orcdebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    NEWLINE = 258,                 /* NEWLINE  */
    S_NEQ = 259,                   /* S_NEQ  */
    S_AND = 260,                   /* S_AND  */
    S_OR = 261,                    /* S_OR  */
    S_LT = 262,                    /* S_LT  */
    S_LE = 263,                    /* S_LE  */
    S_EQ = 264,                    /* S_EQ  */
    S_ADDIN = 265,                 /* S_ADDIN  */
    S_SUBIN = 266,                 /* S_SUBIN  */
    S_MULIN = 267,                 /* S_MULIN  */
    S_DIVIN = 268,                 /* S_DIVIN  */
    S_GT = 269,                    /* S_GT  */
    S_GE = 270,                    /* S_GE  */
    S_BITSHIFT_LEFT = 271,         /* S_BITSHIFT_LEFT  */
    S_BITSHIFT_RIGHT = 272,        /* S_BITSHIFT_RIGHT  */
    LABEL_TOKEN = 273,             /* LABEL_TOKEN  */
    IF_TOKEN = 274,                /* IF_TOKEN  */
    DECLARE_TOKEN = 275,           /* DECLARE_TOKEN  */
    UDO_TOKEN = 276,               /* UDO_TOKEN  */
    UDOSTART_DEFINITION = 277,     /* UDOSTART_DEFINITION  */
    UDOEND_TOKEN = 278,            /* UDOEND_TOKEN  */
    UDO_ANS_TOKEN = 279,           /* UDO_ANS_TOKEN  */
    UDO_ARGS_TOKEN = 280,          /* UDO_ARGS_TOKEN  */
    UDO_IDENT = 281,               /* UDO_IDENT  */
    VOID_TOKEN = 282,              /* VOID_TOKEN  */
    ERROR_TOKEN = 283,             /* ERROR_TOKEN  */
    T_OPCALL = 284,                /* T_OPCALL  */
    T_FUNCTION = 285,              /* T_FUNCTION  */
    T_ASSIGNMENT = 286,            /* T_ASSIGNMENT  */
    STRUCT_TOKEN = 287,            /* STRUCT_TOKEN  */
    INSTR_TOKEN = 288,             /* INSTR_TOKEN  */
    ENDIN_TOKEN = 289,             /* ENDIN_TOKEN  */
    GOTO_TOKEN = 290,              /* GOTO_TOKEN  */
    KGOTO_TOKEN = 291,             /* KGOTO_TOKEN  */
    IGOTO_TOKEN = 292,             /* IGOTO_TOKEN  */
    STRING_TOKEN = 293,            /* STRING_TOKEN  */
    T_IDENT = 294,                 /* T_IDENT  */
    T_IDENTB = 295,                /* T_IDENTB  */
    T_TYPED_IDENT = 296,           /* T_TYPED_IDENT  */
    T_TYPED_IDENTB = 297,          /* T_TYPED_IDENTB  */
    T_MEMBER_IDENT = 298,          /* T_MEMBER_IDENT  */
    T_PLUS_IDENT = 299,            /* T_PLUS_IDENT  */
    INTEGER_TOKEN = 300,           /* INTEGER_TOKEN  */
    NUMBER_TOKEN = 301,            /* NUMBER_TOKEN  */
    THEN_TOKEN = 302,              /* THEN_TOKEN  */
    ITHEN_TOKEN = 303,             /* ITHEN_TOKEN  */
    KTHEN_TOKEN = 304,             /* KTHEN_TOKEN  */
    ELSEIF_TOKEN = 305,            /* ELSEIF_TOKEN  */
    ELSE_TOKEN = 306,              /* ELSE_TOKEN  */
    ENDIF_TOKEN = 307,             /* ENDIF_TOKEN  */
    UNTIL_TOKEN = 308,             /* UNTIL_TOKEN  */
    WHILE_TOKEN = 309,             /* WHILE_TOKEN  */
    DO_TOKEN = 310,                /* DO_TOKEN  */
    OD_TOKEN = 311,                /* OD_TOKEN  */
    BREAK_TOKEN = 312,             /* BREAK_TOKEN  */
    CONTINUE_TOKEN = 313,          /* CONTINUE_TOKEN  */
    SWITCH_TOKEN = 314,            /* SWITCH_TOKEN  */
    CASE_TOKEN = 315,              /* CASE_TOKEN  */
    DEFAULT_TOKEN = 316,           /* DEFAULT_TOKEN  */
    ENDSW_TOKEN = 317,             /* ENDSW_TOKEN  */
    FOR_TOKEN = 318,               /* FOR_TOKEN  */
    IN_TOKEN = 319,                /* IN_TOKEN  */
    TRUE_TOKEN = 320,              /* TRUE_TOKEN  */
    FALSE_TOKEN = 321,             /* FALSE_TOKEN  */
    TRUEK_TOKEN = 322,             /* TRUEK_TOKEN  */
    FALSEK_TOKEN = 323,            /* FALSEK_TOKEN  */
    S_ELIPSIS = 324,               /* S_ELIPSIS  */
    S_ELIPSIS2 = 325,              /* S_ELIPSIS2  */
    T_ARRAY = 326,                 /* T_ARRAY  */
    T_ARRAY_IDENT = 327,           /* T_ARRAY_IDENT  */
    T_DECLARE = 328,               /* T_DECLARE  */
    STRUCT_EXPR = 329,             /* STRUCT_EXPR  */
    T_MAPI = 330,                  /* T_MAPI  */
    T_MAPK = 331,                  /* T_MAPK  */
    S_UNOT = 332,                  /* S_UNOT  */
    S_UMINUS = 333,                /* S_UMINUS  */
    S_UPLUS = 334,                 /* S_UPLUS  */
    S_GOTO = 335,                  /* S_GOTO  */
    T_HIGHEST = 336                /* T_HIGHEST  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif




int csound_orcparse (PARSE_PARM *parm, void *scanner, CSOUND * csound, TREE ** astTree);


#endif /* !YY_CSOUND_ORC_USERS_ALESSANDROPETROLATI_DESKTOP_ADVANCED_2018_CSOUND_BUILD_CSOUND_STATIC_BUILD_ARM64_CSOUND_ORCPARSE_H_INCLUDED  */
