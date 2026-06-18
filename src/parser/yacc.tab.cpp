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
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "yacc.y"

#include "ast.h"
#include "yacc.tab.h"
#include <iostream>
#include <memory>
#include <map>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc);

void yyerror(YYLTYPE *locp, const char* s) {
    std::cerr << "Parser Error at line " << locp->first_line << " column " << locp->first_column << ": " << s << std::endl;
}

using namespace ast;
// 全局变量：暂存JOIN ON条件和别名映射，供SELECT规则合并
static std::vector<std::shared_ptr<BinaryExpr>> g_join_on_conds;
static std::map<std::string, std::string> g_alias_map_;

#line 90 "yacc.tab.cpp"

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

#include "yacc.tab.hpp"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_SHOW = 3,                       /* SHOW  */
  YYSYMBOL_TABLES = 4,                     /* TABLES  */
  YYSYMBOL_CREATE = 5,                     /* CREATE  */
  YYSYMBOL_TABLE = 6,                      /* TABLE  */
  YYSYMBOL_DROP = 7,                       /* DROP  */
  YYSYMBOL_DESC = 8,                       /* DESC  */
  YYSYMBOL_INSERT = 9,                     /* INSERT  */
  YYSYMBOL_INTO = 10,                      /* INTO  */
  YYSYMBOL_VALUES = 11,                    /* VALUES  */
  YYSYMBOL_DELETE = 12,                    /* DELETE  */
  YYSYMBOL_FROM = 13,                      /* FROM  */
  YYSYMBOL_ASC = 14,                       /* ASC  */
  YYSYMBOL_ORDER = 15,                     /* ORDER  */
  YYSYMBOL_BY = 16,                        /* BY  */
  YYSYMBOL_WHERE = 17,                     /* WHERE  */
  YYSYMBOL_UPDATE = 18,                    /* UPDATE  */
  YYSYMBOL_SET = 19,                       /* SET  */
  YYSYMBOL_SELECT = 20,                    /* SELECT  */
  YYSYMBOL_INT = 21,                       /* INT  */
  YYSYMBOL_CHAR = 22,                      /* CHAR  */
  YYSYMBOL_FLOAT = 23,                     /* FLOAT  */
  YYSYMBOL_INDEX = 24,                     /* INDEX  */
  YYSYMBOL_AND = 25,                       /* AND  */
  YYSYMBOL_JOIN = 26,                      /* JOIN  */
  YYSYMBOL_EXIT = 27,                      /* EXIT  */
  YYSYMBOL_HELP = 28,                      /* HELP  */
  YYSYMBOL_TXN_BEGIN = 29,                 /* TXN_BEGIN  */
  YYSYMBOL_TXN_COMMIT = 30,                /* TXN_COMMIT  */
  YYSYMBOL_TXN_ABORT = 31,                 /* TXN_ABORT  */
  YYSYMBOL_TXN_ROLLBACK = 32,              /* TXN_ROLLBACK  */
  YYSYMBOL_ORDER_BY = 33,                  /* ORDER_BY  */
  YYSYMBOL_ENABLE_NESTLOOP = 34,           /* ENABLE_NESTLOOP  */
  YYSYMBOL_ENABLE_SORTMERGE = 35,          /* ENABLE_SORTMERGE  */
  YYSYMBOL_EXPLAIN = 36,                   /* EXPLAIN  */
  YYSYMBOL_ANALYZE = 37,                   /* ANALYZE  */
  YYSYMBOL_COUNT = 38,                     /* COUNT  */
  YYSYMBOL_MAX_TOKEN = 39,                 /* MAX_TOKEN  */
  YYSYMBOL_MIN_TOKEN = 40,                 /* MIN_TOKEN  */
  YYSYMBOL_SUM_TOKEN = 41,                 /* SUM_TOKEN  */
  YYSYMBOL_AVG = 42,                       /* AVG  */
  YYSYMBOL_GROUP = 43,                     /* GROUP  */
  YYSYMBOL_HAVING = 44,                    /* HAVING  */
  YYSYMBOL_LIMIT = 45,                     /* LIMIT  */
  YYSYMBOL_UNION_TOKEN = 46,               /* UNION_TOKEN  */
  YYSYMBOL_ALL = 47,                       /* ALL  */
  YYSYMBOL_ON = 48,                        /* ON  */
  YYSYMBOL_AS = 49,                        /* AS  */
  YYSYMBOL_ISOLATION = 50,                 /* ISOLATION  */
  YYSYMBOL_LEVEL = 51,                     /* LEVEL  */
  YYSYMBOL_SNAPSHOT_TOKEN = 52,            /* SNAPSHOT_TOKEN  */
  YYSYMBOL_SERIALIZABLE = 53,              /* SERIALIZABLE  */
  YYSYMBOL_TRANSACTION = 54,               /* TRANSACTION  */
  YYSYMBOL_CHECKPOINT = 55,                /* CHECKPOINT  */
  YYSYMBOL_STATIC_CHECKPOINT = 56,         /* STATIC_CHECKPOINT  */
  YYSYMBOL_LEQ = 57,                       /* LEQ  */
  YYSYMBOL_NEQ = 58,                       /* NEQ  */
  YYSYMBOL_GEQ = 59,                       /* GEQ  */
  YYSYMBOL_T_EOF = 60,                     /* T_EOF  */
  YYSYMBOL_IDENTIFIER = 61,                /* IDENTIFIER  */
  YYSYMBOL_VALUE_STRING = 62,              /* VALUE_STRING  */
  YYSYMBOL_VALUE_INT = 63,                 /* VALUE_INT  */
  YYSYMBOL_VALUE_FLOAT = 64,               /* VALUE_FLOAT  */
  YYSYMBOL_VALUE_BOOL = 65,                /* VALUE_BOOL  */
  YYSYMBOL_66_ = 66,                       /* ';'  */
  YYSYMBOL_67_ = 67,                       /* '='  */
  YYSYMBOL_68_ = 68,                       /* '('  */
  YYSYMBOL_69_ = 69,                       /* ')'  */
  YYSYMBOL_70_ = 70,                       /* ','  */
  YYSYMBOL_71_ = 71,                       /* '*'  */
  YYSYMBOL_72_ = 72,                       /* '.'  */
  YYSYMBOL_73_ = 73,                       /* '<'  */
  YYSYMBOL_74_ = 74,                       /* '>'  */
  YYSYMBOL_YYACCEPT = 75,                  /* $accept  */
  YYSYMBOL_start = 76,                     /* start  */
  YYSYMBOL_stmt = 77,                      /* stmt  */
  YYSYMBOL_txnStmt = 78,                   /* txnStmt  */
  YYSYMBOL_dbStmt = 79,                    /* dbStmt  */
  YYSYMBOL_setStmt = 80,                   /* setStmt  */
  YYSYMBOL_ddl = 81,                       /* ddl  */
  YYSYMBOL_dml = 82,                       /* dml  */
  YYSYMBOL_opt_group_clause = 83,          /* opt_group_clause  */
  YYSYMBOL_optHavingClause = 84,           /* optHavingClause  */
  YYSYMBOL_opt_limit = 85,                 /* opt_limit  */
  YYSYMBOL_fieldList = 86,                 /* fieldList  */
  YYSYMBOL_colNameList = 87,               /* colNameList  */
  YYSYMBOL_field = 88,                     /* field  */
  YYSYMBOL_type = 89,                      /* type  */
  YYSYMBOL_valueList = 90,                 /* valueList  */
  YYSYMBOL_value = 91,                     /* value  */
  YYSYMBOL_condition = 92,                 /* condition  */
  YYSYMBOL_optWhereClause = 93,            /* optWhereClause  */
  YYSYMBOL_whereClause = 94,               /* whereClause  */
  YYSYMBOL_havingClause = 95,              /* havingClause  */
  YYSYMBOL_having_condition = 96,          /* having_condition  */
  YYSYMBOL_having_lhs = 97,                /* having_lhs  */
  YYSYMBOL_having_agg = 98,                /* having_agg  */
  YYSYMBOL_col = 99,                       /* col  */
  YYSYMBOL_colList = 100,                  /* colList  */
  YYSYMBOL_op = 101,                       /* op  */
  YYSYMBOL_expr = 102,                     /* expr  */
  YYSYMBOL_setClauses = 103,               /* setClauses  */
  YYSYMBOL_setClause = 104,                /* setClause  */
  YYSYMBOL_selector = 105,                 /* selector  */
  YYSYMBOL_agg_item = 106,                 /* agg_item  */
  YYSYMBOL_tableList = 107,                /* tableList  */
  YYSYMBOL_opt_order_clause = 108,         /* opt_order_clause  */
  YYSYMBOL_order_clause = 109,             /* order_clause  */
  YYSYMBOL_opt_asc_desc = 110,             /* opt_asc_desc  */
  YYSYMBOL_set_knob_type = 111,            /* set_knob_type  */
  YYSYMBOL_tbName = 112,                   /* tbName  */
  YYSYMBOL_colName = 113                   /* colName  */
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
typedef yytype_uint8 yy_state_t;

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
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

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
#define YYFINAL  55
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   229

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  39
/* YYNRULES -- Number of rules.  */
#define YYNRULES  113
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  232

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   320


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
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      68,    69,    71,     2,    70,     2,    72,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    66,
      73,    67,    74,     2,     2,     2,     2,     2,     2,     2,
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
      65
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    71,    71,    76,    81,    86,    94,    95,    96,    97,
      98,   102,   106,   110,   114,   121,   125,   132,   136,   140,
     147,   151,   155,   159,   163,   167,   174,   178,   182,   186,
     198,   211,   212,   216,   217,   224,   225,   232,   236,   243,
     247,   254,   261,   265,   269,   276,   280,   287,   291,   295,
     299,   306,   313,   314,   321,   325,   332,   336,   343,   350,
     354,   361,   368,   375,   382,   389,   396,   405,   409,   416,
     420,   425,   429,   435,   439,   443,   447,   454,   458,   462,
     466,   470,   474,   481,   485,   492,   496,   503,   510,   514,
     520,   527,   534,   541,   548,   555,   565,   569,   574,   578,
     583,   587,   592,   597,   606,   610,   614,   621,   622,   623,
     627,   628,   631,   633
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
  "\"end of file\"", "error", "\"invalid token\"", "SHOW", "TABLES",
  "CREATE", "TABLE", "DROP", "DESC", "INSERT", "INTO", "VALUES", "DELETE",
  "FROM", "ASC", "ORDER", "BY", "WHERE", "UPDATE", "SET", "SELECT", "INT",
  "CHAR", "FLOAT", "INDEX", "AND", "JOIN", "EXIT", "HELP", "TXN_BEGIN",
  "TXN_COMMIT", "TXN_ABORT", "TXN_ROLLBACK", "ORDER_BY", "ENABLE_NESTLOOP",
  "ENABLE_SORTMERGE", "EXPLAIN", "ANALYZE", "COUNT", "MAX_TOKEN",
  "MIN_TOKEN", "SUM_TOKEN", "AVG", "GROUP", "HAVING", "LIMIT",
  "UNION_TOKEN", "ALL", "ON", "AS", "ISOLATION", "LEVEL", "SNAPSHOT_TOKEN",
  "SERIALIZABLE", "TRANSACTION", "CHECKPOINT", "STATIC_CHECKPOINT", "LEQ",
  "NEQ", "GEQ", "T_EOF", "IDENTIFIER", "VALUE_STRING", "VALUE_INT",
  "VALUE_FLOAT", "VALUE_BOOL", "';'", "'='", "'('", "')'", "','", "'*'",
  "'.'", "'<'", "'>'", "$accept", "start", "stmt", "txnStmt", "dbStmt",
  "setStmt", "ddl", "dml", "opt_group_clause", "optHavingClause",
  "opt_limit", "fieldList", "colNameList", "field", "type", "valueList",
  "value", "condition", "optWhereClause", "whereClause", "havingClause",
  "having_condition", "having_lhs", "having_agg", "col", "colList", "op",
  "expr", "setClauses", "setClause", "selector", "agg_item", "tableList",
  "opt_order_clause", "order_clause", "opt_asc_desc", "set_knob_type",
  "tbName", "colName", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-114)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-113)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      31,    13,    29,    17,   -37,    35,    39,   -37,    43,    42,
    -114,  -114,  -114,  -114,  -114,  -114,    34,  -114,    96,     9,
    -114,  -114,  -114,  -114,  -114,  -114,    86,   -37,   -37,  -114,
     -37,   -37,  -114,  -114,   -37,   -37,    92,  -114,  -114,    68,
      45,    51,    61,    82,    83,    84,    56,  -114,   104,    85,
     143,   108,    87,  -114,   138,  -114,  -114,   -37,    94,    95,
    -114,    97,   149,   144,   103,   115,   102,   -49,   103,   103,
     103,   103,   103,    48,   -37,   103,   103,    42,  -114,   103,
     103,   103,   100,   110,  -114,  -114,    -1,  -114,   105,    -5,
    -114,   106,   107,   109,   111,   112,   113,  -114,   120,   124,
       4,   116,  -114,  -114,   161,    24,  -114,   126,    32,  -114,
      38,    60,  -114,   154,    47,   103,  -114,    60,   133,  -114,
    -114,  -114,  -114,  -114,  -114,  -114,   103,   103,   -37,   -37,
     141,  -114,   -37,  -114,   103,  -114,   117,  -114,  -114,  -114,
     103,  -114,  -114,  -114,  -114,  -114,    46,  -114,   110,  -114,
    -114,  -114,  -114,  -114,  -114,    75,  -114,  -114,  -114,  -114,
    -114,   -17,   125,   171,   145,     4,  -114,   127,  -114,  -114,
      60,  -114,  -114,  -114,  -114,   110,   140,  -114,   103,    93,
     176,   176,   123,  -114,   154,   110,   129,   128,   134,   135,
     136,   137,   168,  -114,    47,  -114,  -114,   178,   150,  -114,
    -114,   154,   -15,   103,   103,   103,   103,    93,    60,   110,
     146,  -114,   132,   139,   142,   147,   148,   151,  -114,  -114,
      62,  -114,  -114,  -114,  -114,  -114,  -114,  -114,  -114,  -114,
    -114,  -114
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       4,     3,    11,    12,    13,    14,     0,     5,     0,     0,
       9,     6,    10,     7,     8,    15,     0,     0,     0,    25,
       0,     0,   112,    22,     0,     0,     0,   110,   111,     0,
       0,     0,     0,     0,     0,     0,   113,    88,    69,    89,
       0,    71,     0,    68,     0,     1,     2,     0,     0,     0,
      21,     0,     0,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    16,     0,
       0,     0,     0,     0,    27,   113,    52,    85,     0,     0,
      17,     0,     0,     0,     0,     0,     0,    70,    73,    75,
      52,    96,    72,    67,     0,     0,    37,     0,     0,    39,
       0,     0,    54,    53,     0,     0,    28,     0,     0,    19,
      90,    91,    92,    93,    94,    95,     0,     0,     0,     0,
      31,    97,     0,    20,     0,    42,     0,    44,    41,    23,
       0,    24,    49,    47,    48,    50,     0,    45,     0,    81,
      80,    82,    77,    78,    79,     0,    86,    87,    18,    74,
      76,   100,    98,     0,    33,    52,    38,     0,    40,    26,
       0,    55,    83,    84,    51,     0,   101,    99,     0,     0,
     105,   105,     0,    46,   102,     0,    32,     0,     0,     0,
       0,     0,    34,    56,     0,    60,    59,     0,    35,    30,
      43,   103,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,     0,     0,     0,     0,    57,    58,
     109,   104,    36,    61,    62,    63,    64,    65,    66,   108,
     107,   106
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -114,  -114,  -114,  -114,  -114,  -114,  -114,  -114,  -114,  -114,
    -114,  -114,   -80,    63,  -114,  -114,  -113,    58,   -73,   -75,
    -114,     0,  -114,  -114,    -9,  -114,    16,  -114,  -114,    98,
     152,   153,    80,    33,  -114,  -114,  -114,    -2,   -61
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    18,    19,    20,    21,    22,    23,    24,   164,   180,
     211,   105,   108,   106,   138,   146,   147,   112,    84,   113,
     192,   193,   194,   195,   114,    49,   155,   174,    86,    87,
      50,    51,   100,   198,   221,   231,    40,    52,    53
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      48,   110,    33,    88,   157,    36,    92,    93,    94,    95,
      96,    97,    85,   116,   102,   103,    83,    25,   107,   109,
     109,    83,    91,    30,    32,    58,    59,   130,    60,    61,
     128,   175,    62,    63,     1,    27,     2,    26,     3,     4,
       5,    31,   172,     6,   176,    34,    85,   118,   119,     7,
       8,     9,    35,    28,    88,    78,   212,   183,    10,    11,
      12,    13,    14,    15,    98,   159,   160,    16,    48,   115,
     229,    54,   101,   107,   129,    56,   230,    37,    38,   168,
      41,    42,    43,    44,    45,    29,    41,    42,    43,    44,
      45,    17,   181,   133,   134,   219,    55,    39,   186,    57,
     184,   139,   140,    46,   149,   150,   151,   141,   140,    46,
     201,    64,    66,    47,   152,   169,   170,   109,    65,    67,
     153,   154,   142,   143,   144,   145,   161,   162,  -112,    68,
     101,   187,   188,   189,   190,   191,    46,   142,   143,   144,
     145,   213,   214,   215,   216,   217,   173,   135,   136,   137,
      69,    70,    71,    72,    46,    73,    74,    75,    77,    76,
      82,    83,    79,    80,    85,    81,    89,    90,   111,   126,
     196,    46,   117,   127,   132,   120,   121,   131,   122,   148,
     123,   124,   125,   158,   163,   167,   177,   178,   185,   179,
     182,   197,   200,   207,   209,   210,   202,   166,   196,   140,
     220,   223,   203,   204,   205,   206,   171,   218,   224,   222,
     208,   225,   165,   156,   199,     0,   226,   227,     0,     0,
     228,     0,     0,     0,     0,     0,    99,     0,     0,   104
};

static const yytype_int16 yycheck[] =
{
       9,    81,     4,    64,   117,     7,    67,    68,    69,    70,
      71,    72,    61,    86,    75,    76,    17,     4,    79,    80,
      81,    17,    71,     6,    61,    27,    28,   100,    30,    31,
      26,    48,    34,    35,     3,     6,     5,    24,     7,     8,
       9,    24,   155,    12,    61,    10,    61,    52,    53,    18,
      19,    20,    13,    24,   115,    57,    71,   170,    27,    28,
      29,    30,    31,    32,    73,   126,   127,    36,    77,    70,
       8,    37,    74,   134,    70,    66,    14,    34,    35,   140,
      38,    39,    40,    41,    42,    56,    38,    39,    40,    41,
      42,    60,   165,    69,    70,   208,     0,    54,   178,    13,
     175,    69,    70,    61,    57,    58,    59,    69,    70,    61,
     185,    19,    67,    71,    67,    69,    70,   178,    50,    68,
      73,    74,    62,    63,    64,    65,   128,   129,    72,    68,
     132,    38,    39,    40,    41,    42,    61,    62,    63,    64,
      65,   202,   203,   204,   205,   206,   155,    21,    22,    23,
      68,    68,    68,    49,    61,    70,    13,    49,    20,    72,
      11,    17,    68,    68,    61,    68,    51,    65,    68,    49,
     179,    61,    67,    49,    13,    69,    69,    61,    69,    25,
      69,    69,    69,    50,    43,    68,    61,    16,    48,    44,
      63,    15,    69,    25,    16,    45,    68,   134,   207,    70,
     209,    69,    68,    68,    68,    68,   148,   207,    69,    63,
     194,    69,   132,   115,   181,    -1,    69,    69,    -1,    -1,
      69,    -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,    77
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     5,     7,     8,     9,    12,    18,    19,    20,
      27,    28,    29,    30,    31,    32,    36,    60,    76,    77,
      78,    79,    80,    81,    82,     4,    24,     6,    24,    56,
       6,    24,    61,   112,    10,    13,   112,    34,    35,    54,
     111,    38,    39,    40,    41,    42,    61,    71,    99,   100,
     105,   106,   112,   113,    37,     0,    66,    13,   112,   112,
     112,   112,   112,   112,    19,    50,    67,    68,    68,    68,
      68,    68,    49,    70,    13,    49,    72,    20,   112,    68,
      68,    68,    11,    17,    93,    61,   103,   104,   113,    51,
      65,    71,   113,   113,   113,   113,   113,   113,    99,   106,
     107,   112,   113,   113,   105,    86,    88,   113,    87,   113,
      87,    68,    92,    94,    99,    70,    93,    67,    52,    53,
      69,    69,    69,    69,    69,    69,    49,    49,    26,    70,
      93,    61,    13,    69,    70,    21,    22,    23,    89,    69,
      70,    69,    62,    63,    64,    65,    90,    91,    25,    57,
      58,    59,    67,    73,    74,   101,   104,    91,    50,   113,
     113,   112,   112,    43,    83,   107,    88,    68,   113,    69,
      70,    92,    91,    99,   102,    48,    61,    61,    16,    44,
      84,    93,    63,    91,    94,    48,    87,    38,    39,    40,
      41,    42,    95,    96,    97,    98,    99,    15,   108,   108,
      69,    94,    68,    68,    68,    68,    68,    25,   101,    16,
      45,    85,    71,   113,   113,   113,   113,   113,    96,    91,
      99,   109,    63,    69,    69,    69,    69,    69,    69,     8,
      14,   110
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    75,    76,    76,    76,    76,    77,    77,    77,    77,
      77,    78,    78,    78,    78,    79,    79,    80,    80,    80,
      81,    81,    81,    81,    81,    81,    82,    82,    82,    82,
      82,    83,    83,    84,    84,    85,    85,    86,    86,    87,
      87,    88,    89,    89,    89,    90,    90,    91,    91,    91,
      91,    92,    93,    93,    94,    94,    95,    95,    96,    97,
      97,    98,    98,    98,    98,    98,    98,    99,    99,   100,
     100,   100,   100,   100,   100,   100,   100,   101,   101,   101,
     101,   101,   101,   102,   102,   103,   103,   104,   105,   105,
     106,   106,   106,   106,   106,   106,   107,   107,   107,   107,
     107,   107,   107,   107,   108,   108,   109,   110,   110,   110,
     111,   111,   112,   113
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     4,     4,     6,     5,
       6,     3,     2,     6,     6,     2,     7,     4,     5,     9,
       8,     0,     3,     0,     2,     0,     2,     1,     3,     1,
       3,     2,     1,     4,     1,     1,     3,     1,     1,     1,
       1,     3,     0,     2,     1,     3,     1,     3,     3,     1,
       1,     4,     4,     4,     4,     4,     4,     3,     1,     1,
       3,     1,     3,     3,     5,     3,     5,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     3,     1,     1,
       4,     4,     4,     4,     4,     4,     1,     2,     3,     4,
       3,     4,     5,     6,     3,     0,     2,     1,     1,     0,
       1,     1,     1,     1
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
        yyerror (&yylloc, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


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


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
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
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp);
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
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule)
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
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]));
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule); \
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
  YYLTYPE *yylloc;
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
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
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
yyparse (void)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

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

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
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
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
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
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

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
      yychar = yylex (&yylval, &yylloc);
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
      yyerror_range[1] = yylloc;
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
  *++yylsp = yylloc;

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

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* start: stmt ';'  */
#line 72 "yacc.y"
    {
        parse_tree = (yyvsp[-1].sv_node);
        YYACCEPT;
    }
#line 1750 "yacc.tab.cpp"
    break;

  case 3: /* start: HELP  */
#line 77 "yacc.y"
    {
        parse_tree = std::make_shared<Help>();
        YYACCEPT;
    }
#line 1759 "yacc.tab.cpp"
    break;

  case 4: /* start: EXIT  */
#line 82 "yacc.y"
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
#line 1768 "yacc.tab.cpp"
    break;

  case 5: /* start: T_EOF  */
#line 87 "yacc.y"
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
#line 1777 "yacc.tab.cpp"
    break;

  case 11: /* txnStmt: TXN_BEGIN  */
#line 103 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnBegin>();
    }
#line 1785 "yacc.tab.cpp"
    break;

  case 12: /* txnStmt: TXN_COMMIT  */
#line 107 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnCommit>();
    }
#line 1793 "yacc.tab.cpp"
    break;

  case 13: /* txnStmt: TXN_ABORT  */
#line 111 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnAbort>();
    }
#line 1801 "yacc.tab.cpp"
    break;

  case 14: /* txnStmt: TXN_ROLLBACK  */
#line 115 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnRollback>();
    }
#line 1809 "yacc.tab.cpp"
    break;

  case 15: /* dbStmt: SHOW TABLES  */
#line 122 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<ShowTables>();
    }
#line 1817 "yacc.tab.cpp"
    break;

  case 16: /* dbStmt: SHOW INDEX FROM tbName  */
#line 126 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<ShowIndex>((yyvsp[0].sv_str));
    }
#line 1825 "yacc.tab.cpp"
    break;

  case 17: /* setStmt: SET set_knob_type '=' VALUE_BOOL  */
#line 133 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<SetStmt>((yyvsp[-2].sv_setKnobType), (yyvsp[0].sv_bool));
    }
#line 1833 "yacc.tab.cpp"
    break;

  case 18: /* setStmt: SET TRANSACTION ISOLATION LEVEL SNAPSHOT_TOKEN ISOLATION  */
#line 137 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<SetIsolationLevel>("SNAPSHOT_ISOLATION");
    }
#line 1841 "yacc.tab.cpp"
    break;

  case 19: /* setStmt: SET TRANSACTION ISOLATION LEVEL SERIALIZABLE  */
#line 141 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<SetIsolationLevel>("SERIALIZABLE");
    }
#line 1849 "yacc.tab.cpp"
    break;

  case 20: /* ddl: CREATE TABLE tbName '(' fieldList ')'  */
#line 148 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateTable>((yyvsp[-3].sv_str), (yyvsp[-1].sv_fields));
    }
#line 1857 "yacc.tab.cpp"
    break;

  case 21: /* ddl: DROP TABLE tbName  */
#line 152 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DropTable>((yyvsp[0].sv_str));
    }
#line 1865 "yacc.tab.cpp"
    break;

  case 22: /* ddl: DESC tbName  */
#line 156 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DescTable>((yyvsp[0].sv_str));
    }
#line 1873 "yacc.tab.cpp"
    break;

  case 23: /* ddl: CREATE INDEX tbName '(' colNameList ')'  */
#line 160 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1881 "yacc.tab.cpp"
    break;

  case 24: /* ddl: DROP INDEX tbName '(' colNameList ')'  */
#line 164 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DropIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1889 "yacc.tab.cpp"
    break;

  case 25: /* ddl: CREATE STATIC_CHECKPOINT  */
#line 168 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateStaticCheckpoint>();
    }
#line 1897 "yacc.tab.cpp"
    break;

  case 26: /* dml: INSERT INTO tbName VALUES '(' valueList ')'  */
#line 175 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<InsertStmt>((yyvsp[-4].sv_str), (yyvsp[-1].sv_vals));
    }
#line 1905 "yacc.tab.cpp"
    break;

  case 27: /* dml: DELETE FROM tbName optWhereClause  */
#line 179 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DeleteStmt>((yyvsp[-1].sv_str), (yyvsp[0].sv_conds));
    }
#line 1913 "yacc.tab.cpp"
    break;

  case 28: /* dml: UPDATE tbName SET setClauses optWhereClause  */
#line 183 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<UpdateStmt>((yyvsp[-3].sv_str), (yyvsp[-1].sv_set_clauses), (yyvsp[0].sv_conds));
    }
#line 1921 "yacc.tab.cpp"
    break;

  case 29: /* dml: SELECT selector FROM tableList optWhereClause opt_group_clause optHavingClause opt_order_clause opt_limit  */
#line 187 "yacc.y"
    {
        auto merged_conds = (yyvsp[-4].sv_conds);
        merged_conds.insert(merged_conds.end(), g_join_on_conds.begin(), g_join_on_conds.end());
        g_join_on_conds.clear();
        auto sel = std::make_shared<SelectStmt>((yyvsp[-7].sv_cols), (yyvsp[-5].sv_strs), merged_conds, (yyvsp[-1].sv_orderby));
        sel->group_by = (yyvsp[-3].sv_strs);
        sel->having = (yyvsp[-2].sv_conds);
        if ((yyvsp[0].sv_int) > 0) sel->limit_val = (yyvsp[0].sv_int);
        sel->alias_map = g_alias_map_; g_alias_map_.clear();
        (yyval.sv_node) = sel;
    }
#line 1937 "yacc.tab.cpp"
    break;

  case 30: /* dml: EXPLAIN ANALYZE SELECT selector FROM tableList optWhereClause opt_order_clause  */
#line 199 "yacc.y"
    {
        auto merged_conds = (yyvsp[-1].sv_conds);
        merged_conds.insert(merged_conds.end(), g_join_on_conds.begin(), g_join_on_conds.end());
        g_join_on_conds.clear();
        auto sel = std::make_shared<SelectStmt>((yyvsp[-4].sv_cols), (yyvsp[-2].sv_strs), merged_conds, (yyvsp[0].sv_orderby));
        sel->explain_analyze = true;
        sel->alias_map = g_alias_map_; g_alias_map_.clear();
        (yyval.sv_node) = sel;
    }
#line 1951 "yacc.tab.cpp"
    break;

  case 31: /* opt_group_clause: %empty  */
#line 211 "yacc.y"
                      { (yyval.sv_strs) = std::vector<std::string>(); }
#line 1957 "yacc.tab.cpp"
    break;

  case 32: /* opt_group_clause: GROUP BY colNameList  */
#line 212 "yacc.y"
                             { (yyval.sv_strs) = (yyvsp[0].sv_strs); }
#line 1963 "yacc.tab.cpp"
    break;

  case 33: /* optHavingClause: %empty  */
#line 216 "yacc.y"
                      { /* ignore */ }
#line 1969 "yacc.tab.cpp"
    break;

  case 34: /* optHavingClause: HAVING havingClause  */
#line 218 "yacc.y"
    {
        (yyval.sv_conds) = (yyvsp[0].sv_conds);
    }
#line 1977 "yacc.tab.cpp"
    break;

  case 35: /* opt_limit: %empty  */
#line 224 "yacc.y"
                      { (yyval.sv_int) = -1; }
#line 1983 "yacc.tab.cpp"
    break;

  case 36: /* opt_limit: LIMIT VALUE_INT  */
#line 226 "yacc.y"
    {
        (yyval.sv_int) = (yyvsp[0].sv_int);
    }
#line 1991 "yacc.tab.cpp"
    break;

  case 37: /* fieldList: field  */
#line 233 "yacc.y"
    {
        (yyval.sv_fields) = std::vector<std::shared_ptr<Field>>{(yyvsp[0].sv_field)};
    }
#line 1999 "yacc.tab.cpp"
    break;

  case 38: /* fieldList: fieldList ',' field  */
#line 237 "yacc.y"
    {
        (yyval.sv_fields).push_back((yyvsp[0].sv_field));
    }
#line 2007 "yacc.tab.cpp"
    break;

  case 39: /* colNameList: colName  */
#line 244 "yacc.y"
    {
        (yyval.sv_strs) = std::vector<std::string>{(yyvsp[0].sv_str)};
    }
#line 2015 "yacc.tab.cpp"
    break;

  case 40: /* colNameList: colNameList ',' colName  */
#line 248 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[0].sv_str));
    }
#line 2023 "yacc.tab.cpp"
    break;

  case 41: /* field: colName type  */
#line 255 "yacc.y"
    {
        (yyval.sv_field) = std::make_shared<ColDef>((yyvsp[-1].sv_str), (yyvsp[0].sv_type_len));
    }
#line 2031 "yacc.tab.cpp"
    break;

  case 42: /* type: INT  */
#line 262 "yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_INT, sizeof(int));
    }
#line 2039 "yacc.tab.cpp"
    break;

  case 43: /* type: CHAR '(' VALUE_INT ')'  */
#line 266 "yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_STRING, (yyvsp[-1].sv_int));
    }
#line 2047 "yacc.tab.cpp"
    break;

  case 44: /* type: FLOAT  */
#line 270 "yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
#line 2055 "yacc.tab.cpp"
    break;

  case 45: /* valueList: value  */
#line 277 "yacc.y"
    {
        (yyval.sv_vals) = std::vector<std::shared_ptr<Value>>{(yyvsp[0].sv_val)};
    }
#line 2063 "yacc.tab.cpp"
    break;

  case 46: /* valueList: valueList ',' value  */
#line 281 "yacc.y"
    {
        (yyval.sv_vals).push_back((yyvsp[0].sv_val));
    }
#line 2071 "yacc.tab.cpp"
    break;

  case 47: /* value: VALUE_INT  */
#line 288 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<IntLit>((yyvsp[0].sv_int));
    }
#line 2079 "yacc.tab.cpp"
    break;

  case 48: /* value: VALUE_FLOAT  */
#line 292 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<FloatLit>((yyvsp[0].sv_float));
    }
#line 2087 "yacc.tab.cpp"
    break;

  case 49: /* value: VALUE_STRING  */
#line 296 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<StringLit>((yyvsp[0].sv_str));
    }
#line 2095 "yacc.tab.cpp"
    break;

  case 50: /* value: VALUE_BOOL  */
#line 300 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<BoolLit>((yyvsp[0].sv_bool));
    }
#line 2103 "yacc.tab.cpp"
    break;

  case 51: /* condition: col op expr  */
#line 307 "yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<BinaryExpr>((yyvsp[-2].sv_col), (yyvsp[-1].sv_comp_op), (yyvsp[0].sv_expr));
    }
#line 2111 "yacc.tab.cpp"
    break;

  case 52: /* optWhereClause: %empty  */
#line 313 "yacc.y"
                      { /* ignore*/ }
#line 2117 "yacc.tab.cpp"
    break;

  case 53: /* optWhereClause: WHERE whereClause  */
#line 315 "yacc.y"
    {
        (yyval.sv_conds) = (yyvsp[0].sv_conds);
    }
#line 2125 "yacc.tab.cpp"
    break;

  case 54: /* whereClause: condition  */
#line 322 "yacc.y"
    {
        (yyval.sv_conds) = std::vector<std::shared_ptr<BinaryExpr>>{(yyvsp[0].sv_cond)};
    }
#line 2133 "yacc.tab.cpp"
    break;

  case 55: /* whereClause: whereClause AND condition  */
#line 326 "yacc.y"
    {
        (yyval.sv_conds).push_back((yyvsp[0].sv_cond));
    }
#line 2141 "yacc.tab.cpp"
    break;

  case 56: /* havingClause: having_condition  */
#line 333 "yacc.y"
    {
        (yyval.sv_conds) = std::vector<std::shared_ptr<BinaryExpr>>{(yyvsp[0].sv_cond)};
    }
#line 2149 "yacc.tab.cpp"
    break;

  case 57: /* havingClause: havingClause AND having_condition  */
#line 337 "yacc.y"
    {
        (yyval.sv_conds).push_back((yyvsp[0].sv_cond));
    }
#line 2157 "yacc.tab.cpp"
    break;

  case 58: /* having_condition: having_lhs op value  */
#line 344 "yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<BinaryExpr>((yyvsp[-2].sv_col), (yyvsp[-1].sv_comp_op), (yyvsp[0].sv_val));
    }
#line 2165 "yacc.tab.cpp"
    break;

  case 59: /* having_lhs: col  */
#line 351 "yacc.y"
    {
        (yyval.sv_col) = (yyvsp[0].sv_col);
    }
#line 2173 "yacc.tab.cpp"
    break;

  case 60: /* having_lhs: having_agg  */
#line 355 "yacc.y"
    {
        (yyval.sv_col) = (yyvsp[0].sv_col);
    }
#line 2181 "yacc.tab.cpp"
    break;

  case 61: /* having_agg: COUNT '(' '*' ')'  */
#line 362 "yacc.y"
    {
        auto c = std::make_shared<Col>("*", "*");
        c->is_agg = true;
        c->agg_func = "COUNT";
        (yyval.sv_col) = c;
    }
#line 2192 "yacc.tab.cpp"
    break;

  case 62: /* having_agg: COUNT '(' colName ')'  */
#line 369 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "COUNT";
        (yyval.sv_col) = c;
    }
#line 2203 "yacc.tab.cpp"
    break;

  case 63: /* having_agg: MAX_TOKEN '(' colName ')'  */
#line 376 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "MAX";
        (yyval.sv_col) = c;
    }
#line 2214 "yacc.tab.cpp"
    break;

  case 64: /* having_agg: MIN_TOKEN '(' colName ')'  */
#line 383 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "MIN";
        (yyval.sv_col) = c;
    }
#line 2225 "yacc.tab.cpp"
    break;

  case 65: /* having_agg: SUM_TOKEN '(' colName ')'  */
#line 390 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "SUM";
        (yyval.sv_col) = c;
    }
#line 2236 "yacc.tab.cpp"
    break;

  case 66: /* having_agg: AVG '(' colName ')'  */
#line 397 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "AVG";
        (yyval.sv_col) = c;
    }
#line 2247 "yacc.tab.cpp"
    break;

  case 67: /* col: tbName '.' colName  */
#line 406 "yacc.y"
    {
        (yyval.sv_col) = std::make_shared<Col>((yyvsp[-2].sv_str), (yyvsp[0].sv_str));
    }
#line 2255 "yacc.tab.cpp"
    break;

  case 68: /* col: colName  */
#line 410 "yacc.y"
    {
        (yyval.sv_col) = std::make_shared<Col>("", (yyvsp[0].sv_str));
    }
#line 2263 "yacc.tab.cpp"
    break;

  case 69: /* colList: col  */
#line 417 "yacc.y"
    {
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{(yyvsp[0].sv_col)};
    }
#line 2271 "yacc.tab.cpp"
    break;

  case 70: /* colList: col AS colName  */
#line 421 "yacc.y"
    {
        auto c = (yyvsp[-2].sv_col); c->col_name = (yyvsp[0].sv_str);
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{c};
    }
#line 2280 "yacc.tab.cpp"
    break;

  case 71: /* colList: agg_item  */
#line 426 "yacc.y"
    {
        (yyval.sv_cols) = (yyvsp[0].sv_cols);
    }
#line 2288 "yacc.tab.cpp"
    break;

  case 72: /* colList: agg_item AS colName  */
#line 430 "yacc.y"
    {
        auto v = (yyvsp[-2].sv_cols);
        for (auto &c : v) { if (c->tab_name.empty()) c->tab_name = c->col_name; c->col_name = (yyvsp[0].sv_str); }
        (yyval.sv_cols) = v;
    }
#line 2298 "yacc.tab.cpp"
    break;

  case 73: /* colList: colList ',' col  */
#line 436 "yacc.y"
    {
        (yyval.sv_cols).push_back((yyvsp[0].sv_col));
    }
#line 2306 "yacc.tab.cpp"
    break;

  case 74: /* colList: colList ',' col AS colName  */
#line 440 "yacc.y"
    {
        auto c = (yyvsp[-2].sv_col); c->col_name = (yyvsp[0].sv_str); (yyval.sv_cols).push_back(c);
    }
#line 2314 "yacc.tab.cpp"
    break;

  case 75: /* colList: colList ',' agg_item  */
#line 444 "yacc.y"
    {
        for (auto &c : (yyvsp[0].sv_cols)) (yyval.sv_cols).push_back(c);
    }
#line 2322 "yacc.tab.cpp"
    break;

  case 76: /* colList: colList ',' agg_item AS colName  */
#line 448 "yacc.y"
    {
        for (auto &c : (yyvsp[-2].sv_cols)) { if (c->tab_name.empty()) c->tab_name = c->col_name; c->col_name = (yyvsp[0].sv_str); (yyval.sv_cols).push_back(c); }
    }
#line 2330 "yacc.tab.cpp"
    break;

  case 77: /* op: '='  */
#line 455 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_EQ;
    }
#line 2338 "yacc.tab.cpp"
    break;

  case 78: /* op: '<'  */
#line 459 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_LT;
    }
#line 2346 "yacc.tab.cpp"
    break;

  case 79: /* op: '>'  */
#line 463 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_GT;
    }
#line 2354 "yacc.tab.cpp"
    break;

  case 80: /* op: NEQ  */
#line 467 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_NE;
    }
#line 2362 "yacc.tab.cpp"
    break;

  case 81: /* op: LEQ  */
#line 471 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_LE;
    }
#line 2370 "yacc.tab.cpp"
    break;

  case 82: /* op: GEQ  */
#line 475 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_GE;
    }
#line 2378 "yacc.tab.cpp"
    break;

  case 83: /* expr: value  */
#line 482 "yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_val));
    }
#line 2386 "yacc.tab.cpp"
    break;

  case 84: /* expr: col  */
#line 486 "yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_col));
    }
#line 2394 "yacc.tab.cpp"
    break;

  case 85: /* setClauses: setClause  */
#line 493 "yacc.y"
    {
        (yyval.sv_set_clauses) = std::vector<std::shared_ptr<SetClause>>{(yyvsp[0].sv_set_clause)};
    }
#line 2402 "yacc.tab.cpp"
    break;

  case 86: /* setClauses: setClauses ',' setClause  */
#line 497 "yacc.y"
    {
        (yyval.sv_set_clauses).push_back((yyvsp[0].sv_set_clause));
    }
#line 2410 "yacc.tab.cpp"
    break;

  case 87: /* setClause: colName '=' value  */
#line 504 "yacc.y"
    {
        (yyval.sv_set_clause) = std::make_shared<SetClause>((yyvsp[-2].sv_str), (yyvsp[0].sv_val));
    }
#line 2418 "yacc.tab.cpp"
    break;

  case 88: /* selector: '*'  */
#line 511 "yacc.y"
    {
        (yyval.sv_cols) = {};
    }
#line 2426 "yacc.tab.cpp"
    break;

  case 90: /* agg_item: COUNT '(' '*' ')'  */
#line 521 "yacc.y"
    {
        auto c = std::make_shared<Col>("", "*");
        c->is_agg = true;
        c->agg_func = "COUNT";
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{c};
    }
#line 2437 "yacc.tab.cpp"
    break;

  case 91: /* agg_item: COUNT '(' colName ')'  */
#line 528 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "COUNT";
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{c};
    }
#line 2448 "yacc.tab.cpp"
    break;

  case 92: /* agg_item: MAX_TOKEN '(' colName ')'  */
#line 535 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "MAX";
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{c};
    }
#line 2459 "yacc.tab.cpp"
    break;

  case 93: /* agg_item: MIN_TOKEN '(' colName ')'  */
#line 542 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "MIN";
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{c};
    }
#line 2470 "yacc.tab.cpp"
    break;

  case 94: /* agg_item: SUM_TOKEN '(' colName ')'  */
#line 549 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "SUM";
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{c};
    }
#line 2481 "yacc.tab.cpp"
    break;

  case 95: /* agg_item: AVG '(' colName ')'  */
#line 556 "yacc.y"
    {
        auto c = std::make_shared<Col>("", (yyvsp[-1].sv_str));
        c->is_agg = true;
        c->agg_func = "AVG";
        (yyval.sv_cols) = std::vector<std::shared_ptr<Col>>{c};
    }
#line 2492 "yacc.tab.cpp"
    break;

  case 96: /* tableList: tbName  */
#line 566 "yacc.y"
    {
        (yyval.sv_strs) = std::vector<std::string>{(yyvsp[0].sv_str)};
    }
#line 2500 "yacc.tab.cpp"
    break;

  case 97: /* tableList: tbName IDENTIFIER  */
#line 570 "yacc.y"
    {
        (yyval.sv_strs) = std::vector<std::string>{(yyvsp[-1].sv_str)};  // tabs存真实表名
        g_alias_map_[(yyvsp[0].sv_str)] = (yyvsp[-1].sv_str);
    }
#line 2509 "yacc.tab.cpp"
    break;

  case 98: /* tableList: tableList ',' tbName  */
#line 575 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[0].sv_str));
    }
#line 2517 "yacc.tab.cpp"
    break;

  case 99: /* tableList: tableList ',' tbName IDENTIFIER  */
#line 579 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[-1].sv_str));  // tabs存真实表名
        g_alias_map_[(yyvsp[0].sv_str)] = (yyvsp[-1].sv_str);
    }
#line 2526 "yacc.tab.cpp"
    break;

  case 100: /* tableList: tableList JOIN tbName  */
#line 584 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[0].sv_str));
    }
#line 2534 "yacc.tab.cpp"
    break;

  case 101: /* tableList: tableList JOIN tbName IDENTIFIER  */
#line 588 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[-1].sv_str));  // tabs存真实表名
        g_alias_map_[(yyvsp[0].sv_str)] = (yyvsp[-1].sv_str);
    }
#line 2543 "yacc.tab.cpp"
    break;

  case 102: /* tableList: tableList JOIN tbName ON whereClause  */
#line 593 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[-2].sv_str));
        g_join_on_conds.insert(g_join_on_conds.end(), (yyvsp[0].sv_conds).begin(), (yyvsp[0].sv_conds).end());
    }
#line 2552 "yacc.tab.cpp"
    break;

  case 103: /* tableList: tableList JOIN tbName IDENTIFIER ON whereClause  */
#line 598 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[-3].sv_str));  // tabs存真实表名
        g_alias_map_[(yyvsp[-2].sv_str)] = (yyvsp[-3].sv_str);
        g_join_on_conds.insert(g_join_on_conds.end(), (yyvsp[0].sv_conds).begin(), (yyvsp[0].sv_conds).end());
    }
#line 2562 "yacc.tab.cpp"
    break;

  case 104: /* opt_order_clause: ORDER BY order_clause  */
#line 607 "yacc.y"
    { 
        (yyval.sv_orderby) = (yyvsp[0].sv_orderby); 
    }
#line 2570 "yacc.tab.cpp"
    break;

  case 105: /* opt_order_clause: %empty  */
#line 610 "yacc.y"
                      { /* ignore*/ }
#line 2576 "yacc.tab.cpp"
    break;

  case 106: /* order_clause: col opt_asc_desc  */
#line 615 "yacc.y"
    { 
        (yyval.sv_orderby) = std::make_shared<OrderBy>((yyvsp[-1].sv_col), (yyvsp[0].sv_orderby_dir));
    }
#line 2584 "yacc.tab.cpp"
    break;

  case 107: /* opt_asc_desc: ASC  */
#line 621 "yacc.y"
                 { (yyval.sv_orderby_dir) = OrderBy_ASC;     }
#line 2590 "yacc.tab.cpp"
    break;

  case 108: /* opt_asc_desc: DESC  */
#line 622 "yacc.y"
                 { (yyval.sv_orderby_dir) = OrderBy_DESC;    }
#line 2596 "yacc.tab.cpp"
    break;

  case 109: /* opt_asc_desc: %empty  */
#line 623 "yacc.y"
            { (yyval.sv_orderby_dir) = OrderBy_DEFAULT; }
#line 2602 "yacc.tab.cpp"
    break;

  case 110: /* set_knob_type: ENABLE_NESTLOOP  */
#line 627 "yacc.y"
                    { (yyval.sv_setKnobType) = EnableNestLoop; }
#line 2608 "yacc.tab.cpp"
    break;

  case 111: /* set_knob_type: ENABLE_SORTMERGE  */
#line 628 "yacc.y"
                         { (yyval.sv_setKnobType) = EnableSortMerge; }
#line 2614 "yacc.tab.cpp"
    break;


#line 2618 "yacc.tab.cpp"

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
  *++yylsp = yyloc;

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
          = {yyssp, yytoken, &yylloc};
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
        yyerror (&yylloc, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
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
                      yytoken, &yylval, &yylloc);
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

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

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
  yyerror (&yylloc, YY_("memory exhausted"));
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
                  yytoken, &yylval, &yylloc);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp);
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

#line 634 "yacc.y"

