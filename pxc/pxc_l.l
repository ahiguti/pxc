NUM		[0-9]
HDNUM		[0-9a-fA-F]
AL		[a-zA-Z_]
ALNUM		[0-9a-zA-Z_]
ESCCH		[abfnrtv\'\"\\\?\0]
NOEL		([^\n]|\n[^\n])
NOEC		([^\*]|\*[^\/])
%{

#include <string.h>
#include <string>
#include "expr.hpp"
#include "pxc_y.hpp"

namespace pxc {

extern char *arena_strdup(const char *str);
extern char *arena_unescape_strdup(const char *str);
extern int count_newline(const char *str);

};

%}
%option noyywrap nounput
%%
"namespace"		{ return TOK_NAMESPACE; }
"import"		{ return TOK_IMPORT; }
"public"		{ return TOK_PUBLIC; }
"private"		{ return TOK_PRIVATE; }
"if"			{ return TOK_IF; }
"else"			{ return TOK_ELSE; }
"elsif"			{ return TOK_ELSIF; }
"while"			{ return TOK_WHILE; }
"do"			{ return TOK_DO; }
"for"			{ return TOK_FOR; }
"foreach"		{ return TOK_FOREACH; }
"break"			{ return TOK_BREAK; }
"continue"		{ return TOK_CONTINUE; }
"return"		{ return TOK_RETURN; }
"try"			{ return TOK_TRY; }
"catch"			{ return TOK_CATCH; }
"throw"			{ return TOK_THROW; }
"switch"		{ return TOK_SWITCH; }
"case"			{ return TOK_CASE; }
"function"		{ return TOK_FUNCTION; }
"var"			{ return TOK_VAR; }
"const"			{ return TOK_CONST; }
"mutable"		{ return TOK_MUTABLE; }
"typedef"		{ return TOK_TYPEDEF; }
"macro"			{ return TOK_MACRO; }
"struct"		{ return TOK_STRUCT; }
"union"			{ return TOK_VARIANT; }
"interface"		{ return TOK_INTERFACE; }
"enum"			{ return TOK_ENUM; }
"bitmask"		{ return TOK_BITMASK; }
"true"			{ yylval.int_val = 1; return TOK_BOOLLIT; }
"false"			{ yylval.int_val = 0; return TOK_BOOLLIT; }
"extern"		{ return TOK_EXTERN; }
"threaded"		{ return TOK_THREADED; }
"multithreaded"		{ return TOK_MULTITHR; }
"valuetype"		{ return TOK_VALUETYPE; }
"mtvaluetype"		{ return TOK_MTVALUETYPE; }
"tsvaluetype"		{ return TOK_TSVALUETYPE; }
\'(\\{ESCCH}|[^\\'])\'	{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_UINTLIT; }
{NUM}+UL?		{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_UINTLIT; }
0x{HDNUM}+UL?		{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_UINTLIT; }
{NUM}+L?		{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_INTLIT; }
0x{HDNUM}+L?		{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_INTLIT; }
{AL}{ALNUM}*		{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_SYMBOL; }
{NUM}+\.{NUM}+		{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_FLOATLIT; }
\"(\\.|[^\\"])*\"	{ yylval.str_val = pxc::arena_strdup(yytext);
			  return TOK_STRLIT; }
"||"			{ return TOK_LOR; }
"&&"			{ return TOK_LAND; }
"=="			{ return TOK_EQ; }
"!="			{ return TOK_NE; }
">="			{ return TOK_GE; }
"<="			{ return TOK_LE; }
"<<"			{ return TOK_SHIFTL; }
">>"			{ return TOK_SHIFTR; }
"++"			{ return TOK_INC; }
"--"			{ return TOK_DEC; }
"+="			{ return TOK_ADD_ASSIGN; }
"-="			{ return TOK_SUB_ASSIGN; }
"*="			{ return TOK_MUL_ASSIGN; }
"/="			{ return TOK_DIV_ASSIGN; }
"%="			{ return TOK_MOD_ASSIGN; }
"|="			{ return TOK_OR_ASSIGN; }
"&="			{ return TOK_AND_ASSIGN; }
"^="			{ return TOK_XOR_ASSIGN; }
"->"			{ return TOK_ARROW; }
"::"			{ return TOK_NSDELIM; }
".."			{ return TOK_SLICE; }
"\n"			{ yylloc.first_line++; }
inline\n{NOEL}*\n\n	{ yylval.str_val = pxc::arena_strdup(yytext);
			  yylloc.first_line += pxc::count_newline(yytext);
			  return TOK_INLINE; }
\/\*{NOEC}*\*\/		{ yylloc.first_line += pxc::count_newline(yytext); }
[ \t]
(#|\/\/).*$
.			{ return yytext[0]; }
%%

namespace pxc {

YY_BUFFER_STATE cur_hnd = 0;

void pxc_lex_set_buffer(const char *data, size_t len)
{
  if (cur_hnd) {
  	yy_delete_buffer(cur_hnd);
	cur_hnd = 0;
  }
  cur_hnd = yy_scan_bytes(data, len);
}

void pxc_lex_unset_buffer()
{
  if (cur_hnd) {
  	yy_delete_buffer(cur_hnd);
	cur_hnd = 0;
  }
  /* version 2.5.9 or later */
  #if (YY_FLEX_MAJOR_VERSION * 1000000 + YY_FLEX_MINOR_VERSION * 1000 \
  	+ YY_FLEX_SUBMINOR_VERSION >= 2005009)
  yylex_destroy();
  #endif
}

};

