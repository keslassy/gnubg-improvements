/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     EOL = 258,
     EXIT = 259,
     DISABLED = 260,
     EXTVERSION = 261,
     EXTSTRING = 262,
     EXTCHARACTER = 263,
     EXTINTEGER = 264,
     EXTFLOAT = 265,
     EXTBOOLEAN = 266,
     FIBSBOARD = 267,
     FIBSBOARDEND = 268,
     EVALUATION = 269,
     CRAWFORDRULE = 270,
     JACOBYRULE = 271,
     CUBE = 272,
     CUBEFUL = 273,
     CUBELESS = 274,
     DETERMINISTIC = 275,
     NOISE = 276,
     PLIES = 277,
     PRUNE = 278
   };
#endif
/* Tokens.  */
#define EOL 258
#define EXIT 259
#define DISABLED 260
#define EXTVERSION 261
#define EXTSTRING 262
#define EXTCHARACTER 263
#define EXTINTEGER 264
#define EXTFLOAT 265
#define EXTBOOLEAN 266
#define FIBSBOARD 267
#define FIBSBOARDEND 268
#define EVALUATION 269
#define CRAWFORDRULE 270
#define JACOBYRULE 271
#define CUBE 272
#define CUBEFUL 273
#define CUBELESS 274
#define DETERMINISTIC 275
#define NOISE 276
#define PLIES 277
#define PRUNE 278




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 111 "external_y.y"

    gboolean bool;
    gchar character;
    gdouble floatnum;
    gint intnum;
    GString *str;
    GValue *gv;
    GList *list;
    commandinfo *cmd;



/* Line 2068 of yacc.c  */
#line 109 "external_y.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




