%{

// vim:sw=8:ts=8:ai

/* NOTE: this parser is not reentrant */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <errno.h>
#include <assert.h>
#include <vector>

#include "expr.hpp"
#include "parser.hpp"
#include "auto_fp.hpp"

using namespace pxc;

extern FILE *yyin;
int yylex(void);
void yyerror(const char *msg);

static expr_i *topval = 0;
static const char *cur_fname;
static compile_mode cur_mode;
static std::vector<std::pair<char *, int>> fragment_sizes;

static const char *pxc_src_fragment_fn(int ln) {
        for (size_t i = 0; i < fragment_sizes.size(); ++i) {
                const auto frsz = fragment_sizes[i];
                if (ln <= frsz.second) {
                        return frsz.first;
                }
                ln -= frsz.second;
        }
        return "";
}

static int pxc_src_fragment_ln(int ln) {
        for (size_t i = 0; i < fragment_sizes.size(); ++i) {
                const auto frsz = fragment_sizes[i];
                if (ln <= frsz.second) {
                        return ln;
                }
                ln -= frsz.second;
        }
        return ln;
}

#define PXC_FN(x) pxc_src_fragment_fn((x).first_line)
#define PXC_LN(x) pxc_src_fragment_ln((x).first_line)

%}

// %define parse.lac full
%error-verbose
// %define parse.error verbose

%union {
        int void_val;
        int int_val;
        pxc::attribute_e visi_val;
        const char *str_val;
        struct expr_i *expr_val;
}

%token<str_val>  TOK_INTLIT
%token<str_val>  TOK_UINTLIT
%token<str_val>  TOK_FLOATLIT
%token<int_val>  TOK_BOOLLIT
%token<str_val>  TOK_STRLIT
%token<str_val>  TOK_SYMBOL
%token<str_val>  TOK_INLINE
%token<void_val> TOK_EXTERN
%token<void_val> TOK_THREADED
%token<void_val> TOK_MULTITHR
%token<void_val> TOK_VALUETYPE
%token<void_val> TOK_MTVALUETYPE
%token<void_val> TOK_TSVALUETYPE
%token<void_val> TOK_PURE
%token<void_val> TOK_EPHEMERAL
%token<void_val> TOK_NAMESPACE
%token<void_val> TOK_NSDELIM
%token<void_val> TOK_IMPORT
%token<void_val> TOK_PUBLIC
%token<void_val> TOK_PRIVATE
%token<void_val> TOK_IF
%token<void_val> TOK_ELSE
%token<void_val> TOK_ELSIF
%token<void_val> TOK_WHILE
%token<void_val> TOK_DO
%token<void_val> TOK_FOR
%token<void_val> TOK_BREAK
%token<void_val> TOK_CONTINUE
%token<void_val> TOK_RETURN
%token<void_val> TOK_FUNCTION
%token<void_val> TOK_TYPEDEF
%token<void_val> TOK_METAFUNCTION
%token<void_val> TOK_STRUCT
%token<void_val> TOK_UNION
%token<void_val> TOK_INTERFACE
%token<void_val> TOK_LOR
%token<void_val> TOK_LAND
%token<void_val> TOK_SHIFTL
%token<void_val> TOK_SHIFTR
%token<void_val> TOK_PLUS
%token<void_val> TOK_MINUS
%token<void_val> TOK_INC
%token<void_val> TOK_DEC
%token<void_val> TOK_EQ
%token<void_val> TOK_NE
%token<void_val> TOK_GE
%token<void_val> TOK_LE
%token<void_val> TOK_PTR_REF
%token<void_val> TOK_PTR_DEREF
%token<void_val> TOK_ARROW
%token<void_val> TOK_ADD_ASSIGN
%token<void_val> TOK_SUB_ASSIGN
%token<void_val> TOK_MUL_ASSIGN
%token<void_val> TOK_DIV_ASSIGN
%token<void_val> TOK_MOD_ASSIGN
%token<void_val> TOK_OR_ASSIGN
%token<void_val> TOK_AND_ASSIGN
%token<void_val> TOK_XOR_ASSIGN
%token<void_val> TOK_SHIFTL_ASSIGN
%token<void_val> TOK_SHIFTR_ASSIGN
%token<void_val> TOK_SLICE
%token<void_val> TOK_TRY
%token<void_val> TOK_THROW
%token<void_val> TOK_CATCH
%token<void_val> TOK_SWITCH
%token<void_val> TOK_CASE
%token<void_val> TOK_CONST
%token<void_val> TOK_MUTABLE
%token<void_val> TOK_AUTO
%token<void_val> TOK_ENUM
%token<void_val> TOK_BITMASK
%token<void_val> TOK_EXPAND

%type<expr_val> toplevel_stmt_list
%type<expr_val> toplevel_stmt
%type<expr_val> defs_stmt
%type<expr_val> tparam_variadic
%type<expr_val> tparam_list
%type<expr_val> opt_tparams_expr
%type<expr_val> struct_body_stmt_list
%type<expr_val> struct_body_stmt
%type<expr_val> dunion_body_stmt_list
%type<expr_val> dunion_body_stmt
%type<expr_val> interface_body_stmt_list
%type<expr_val> interface_body_stmt
%type<expr_val> function_body_stmt_list
%type<str_val> opt_symbol
%type<expr_val> function_body_stmt
%type<expr_val> body_stmt
%type<expr_val> expression_stmt
%type<expr_val> if_stmt
%type<expr_val> elseif_stmt
%type<expr_val> ifdef_argdecl
%type<expr_val> try_stmt
%type<expr_val> catch_stmt
%type<expr_val> ext_stmt
%type<str_val>  opt_nsalias
%type<str_val>  opt_nssafety
%type<expr_val> funcdef_stmt
%type<expr_val> c_funcdecl_stmt
%type<str_val>  opt_copt
%type<expr_val> funcdecl_stmt
%type<int_val> opt_passby
%type<int_val> opt_cv
%type<visi_val> opt_attribute
%type<visi_val> opt_attribute_threading
%type<visi_val> visibility
%type<int_val> opt_mutable
%type<int_val> opt_private
%type<str_val>  opt_strlit
%type<expr_val> struct_stmt
%type<expr_val> dunion_stmt
%type<expr_val> opt_inherit_expr
%type<expr_val> interface_stmt
%type<str_val>  opt_extern
%type<expr_val> enum_stmt
%type<expr_val> enum_value_list
%type<expr_val> metafdef_stmt
%type<expr_val> c_enumval_stmt
%type<expr_val> visi_vardef_stmt
%type<expr_val> visi_union_flddef_stmt
%type<expr_val> argdecl_key
%type<expr_val> argdecl_mapped
%type<expr_val> argdecl_list
%type<expr_val> argdecl_list_trail
%type<expr_val> forrange_argdecl
%type<expr_val> type_expr
%type<expr_val> type_arg_list
%type<expr_val> type_arg
%type<expr_val> type_arg_excl_metalist
%type<expr_val> simple_unnamed_funcdef
%type<expr_val> unnamed_funcdef
%type<expr_val> nssym_expr
%type<expr_val> symbol_expr
%type<expr_val> expression
%type<expr_val> opt_expression
%type<expr_val> assign_expr
%type<expr_val> cond_expr
%type<expr_val> lor_expr
%type<expr_val> land_expr
%type<expr_val> or_expr
%type<expr_val> xor_expr
%type<expr_val> and_expr
%type<expr_val> equality_expr
%type<expr_val> rel_expr
%type<expr_val> shift_expr
%type<expr_val> add_expr
%type<expr_val> mul_expr
%type<expr_val> unary_expr
%type<expr_val> postfix_expr
%type<expr_val> vardef_stmt
%type<expr_val> vardef_expr
%type<expr_val> primary_expr
%type<expr_val> int_literal
%type<expr_val> array_index_expr

%%

toplevel_stmt_list
        :
          { topval = $$ = 0; }
        | TOK_EXPAND '(' TOK_SYMBOL opt_symbol ':' type_arg ')' '{'
                toplevel_stmt_list '}' toplevel_stmt_list
          { topval = $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1),
                expr_expand_new(PXC_FN(@1), PXC_LN(@1), 0, $3, $4, $6, $9,
                        expand_e_statement, 0), $11); }
        | TOK_EXPAND type_arg ';' toplevel_stmt_list
          { topval = $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1),
                expr_expand_new(PXC_FN(@1), PXC_LN(@1), $2, 0, 0, 0, 0,
                        expand_e_statement, 0), $4); }
        | toplevel_stmt toplevel_stmt_list
          { topval = $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1), $1, $2); }
        ;
toplevel_stmt
        : body_stmt
        | ext_stmt
        | c_enumval_stmt
        | defs_stmt
        | enum_stmt
        | visi_vardef_stmt
        ;
defs_stmt
        : funcdef_stmt
        | c_funcdecl_stmt
        | struct_stmt
        | dunion_stmt
        | interface_stmt
        | metafdef_stmt
        ;
tparam_variadic
        : TOK_SYMBOL '*'
          { $$ = expr_tparams_new(PXC_FN(@1), PXC_LN(@1), $1, 1, 0); }
        ;
tparam_list
        : TOK_SYMBOL
          { $$ = expr_tparams_new(PXC_FN(@1), PXC_LN(@1), $1, 0, 0); }
        | TOK_SYMBOL ',' tparam_list
          { $$ = expr_tparams_new(PXC_FN(@1), PXC_LN(@1), $1, 0, $3); }
        ;
opt_tparams_expr
        :
          { $$ = 0; }
        | '{' tparam_list '}'
          { $$ = $2; }
        ;
struct_body_stmt_list
        :
          { $$ = 0; }
        | TOK_EXPAND '(' TOK_SYMBOL opt_symbol ':' type_arg ')' '{'
                struct_body_stmt_list '}' struct_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1),
                expr_expand_new(PXC_FN(@1), PXC_LN(@1), 0, $3, $4, $6, $9,
                        expand_e_statement, 0), $11); }
        | struct_body_stmt struct_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1), $1, $2); }
        ;
struct_body_stmt
        : expression_stmt /* only vardef is allowed ?? */
        | defs_stmt
        | visi_vardef_stmt
        ;
dunion_body_stmt_list
        :
          { $$ = 0; }
        | TOK_EXPAND '(' TOK_SYMBOL opt_symbol ':' type_arg ')' '{'
                dunion_body_stmt_list '}' dunion_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1),
                expr_expand_new(PXC_FN(@1), PXC_LN(@1), 0, $3, $4, $6, $9,
                        expand_e_statement, 0), $11); }
        | dunion_body_stmt dunion_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1), $1, $2); }
        ;
dunion_body_stmt
        : expression_stmt /* only vardef is allowed */
        | defs_stmt
        | visi_union_flddef_stmt
        ;
interface_body_stmt_list
        :
          { $$ = 0; }
        | interface_body_stmt interface_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1), $1, $2); }
        ;
interface_body_stmt
        : funcdecl_stmt
        | c_funcdecl_stmt
        | struct_stmt
        | dunion_stmt
        | interface_stmt
        | metafdef_stmt
        ;
function_body_stmt_list
        :
          { $$ = 0; }
        | TOK_EXPAND '(' TOK_SYMBOL opt_symbol ':' type_arg ')' '{'
                function_body_stmt_list '}' function_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1),
                expr_expand_new(PXC_FN(@1), PXC_LN(@1), 0, $3, $4, $6, $9,
                        expand_e_statement, 0), $11); }
        | TOK_EXPAND type_arg ';' function_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1),
                expr_expand_new(PXC_FN(@1), PXC_LN(@1), $2, 0, 0, 0, 0,
                        expand_e_statement, 0), $4); }
        | function_body_stmt function_body_stmt_list
          { $$ = expr_stmts_new(PXC_FN(@1), PXC_LN(@1), $1, $2); }
        ;
opt_symbol
        :
          { $$ = 0; }
        | ',' TOK_SYMBOL
          { $$ = $2; }
        ;
function_body_stmt
        : body_stmt
        | defs_stmt
        | c_enumval_stmt
        | opt_attribute /* dummy */ TOK_EXTERN TOK_STRLIT type_arg ';'
          { $$ = expr_inline_c_new(PXC_FN(@1), PXC_LN(@1),
                arena_dequote_strdup($3), "", false, $4); }
          /* for pragma */
        ;
body_stmt
        : expression_stmt
          { $$ = $1; }
        | '{' function_body_stmt_list '}'
          { $$ = expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                passby_e_mutable_value, $2); }
        | if_stmt
          { $$ = $1; }
        | try_stmt
          { $$ = $1; }
        | TOK_WHILE '(' expression ')' '{' function_body_stmt_list '}'
          { $$ = expr_while_new(PXC_FN(@1), PXC_LN(@1), $3,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                        passby_e_mutable_value, $6)); }
        | TOK_FOR '(' opt_expression ';' opt_expression ';' opt_expression ')'
                '{' function_body_stmt_list '}'
          { $$ = expr_for_new(PXC_FN(@1), PXC_LN(@1), $3, $5, $7,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                        passby_e_mutable_value, $10)); }
        | TOK_FOR '(' forrange_argdecl ':' expression TOK_SLICE expression
                ')' '{' function_body_stmt_list '}'
          { $$ = expr_forrange_new(PXC_FN(@1), PXC_LN(@1), $5, $7,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, $3, 0,
                        passby_e_mutable_value, $10)); }
        | TOK_FOR '(' argdecl_key ':' expression ')'
                '{' function_body_stmt_list '}'
          { $$ = expr_feach_new(PXC_FN(@1), PXC_LN(@1), $5,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, $3, 0,
                        passby_e_mutable_value, $8)); }
        | TOK_BREAK ';'
          { $$ = expr_special_new(PXC_FN(@1), PXC_LN(@1), TOK_BREAK, 0); }
        | TOK_CONTINUE ';'
          { $$ = expr_special_new(PXC_FN(@1), PXC_LN(@1), TOK_CONTINUE, 0); }
        | TOK_RETURN ';'
          { $$ = expr_special_new(PXC_FN(@1), PXC_LN(@1), TOK_RETURN, 0); }
        | TOK_RETURN expression ';'
          { $$ = expr_special_new(PXC_FN(@1), PXC_LN(@1), TOK_RETURN, $2); }
        | TOK_THROW expression ';'
          { $$ = expr_special_new(PXC_FN(@1), PXC_LN(@1), TOK_THROW, $2); }
        ;
forrange_argdecl
        : type_expr TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, $1,
                        passby_e_const_value, 0); }
        | TOK_CONST TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                        passby_e_const_value, 0); }
        ;
expression_stmt
        : expression ';'
          { $$ = $1; }
        | expression unnamed_funcdef
          { $$ = expr_add_te(PXC_FN(@1), PXC_LN(@1), $1, $2); }
        | postfix_expr '(' opt_expression ')' simple_unnamed_funcdef
          { $$ = expr_add_te(PXC_FN(@1), PXC_LN(@1),
                expr_funccall_new(PXC_FN(@1), PXC_LN(@1), $1, $3),
                $5); }
        | unary_expr '=' postfix_expr '(' opt_expression ')'
                simple_unnamed_funcdef
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '=', $1,
                expr_add_te(PXC_FN(@1), PXC_LN(@1),
                        expr_funccall_new(PXC_FN(@1), PXC_LN(@1), $3, $5),
                        $7)); }
        | vardef_stmt
          { $$ = $1; }
        ;
if_stmt
        : TOK_IF '(' expression ')' '{' function_body_stmt_list '}'
                elseif_stmt
          { $$ = expr_if_new(PXC_FN(@1), PXC_LN(@1), $3,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                        passby_e_mutable_value, $6),
                0, $8); }
        | TOK_IF '(' expression ')' '{' function_body_stmt_list '}'
                TOK_ELSE '{' function_body_stmt_list '}'
          { $$ = expr_if_new(PXC_FN(@1), PXC_LN(@1), $3,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                        passby_e_mutable_value, $6),
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                        passby_e_mutable_value, $10),
                0); }
        | TOK_IF '(' ifdef_argdecl ':' expression ')'
                '{' function_body_stmt_list '}' elseif_stmt
          { $$ = expr_if_new(PXC_FN(@1), PXC_LN(@1), $5,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, $3, 0,
                        passby_e_mutable_value, $8),
                0, $10); }
        | TOK_IF '(' ifdef_argdecl ':' expression ')'
                '{' function_body_stmt_list '}'
                TOK_ELSE '{' function_body_stmt_list '}'
          { $$ = expr_if_new(PXC_FN(@1), PXC_LN(@1), $5,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, $3, 0,
                        passby_e_mutable_value, $8),
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                        passby_e_mutable_value, $12),
                0); }
        ;
elseif_stmt
        :
          { $$ = 0; }
        | TOK_ELSE if_stmt
          { $$ = $2; }
        ;
ifdef_argdecl
        : type_expr TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, $1,
                        passby_e_mutable_value, 0); }
        | type_expr TOK_MUTABLE TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                        passby_e_mutable_value, 0); }
        | type_expr TOK_CONST TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                        passby_e_const_value, 0); }
        /*
        // prohibited to avoid conflict
        | type_expr '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                        passby_e_mutable_reference, 0); }
        */
        | type_expr TOK_MUTABLE '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                        passby_e_mutable_reference, 0); }
        | type_expr TOK_CONST '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                        passby_e_const_reference, 0); }
        | TOK_MUTABLE TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                        passby_e_mutable_value, 0); }
        | TOK_CONST TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                        passby_e_const_value, 0); }
        | TOK_MUTABLE '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, 0,
                        passby_e_mutable_reference, 0); }
        | TOK_CONST '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, 0,
                        passby_e_const_reference, 0); }
        ;
try_stmt
        : TOK_TRY '{' function_body_stmt_list '}' TOK_CATCH '('
                type_expr TOK_SYMBOL ')' '{' function_body_stmt_list '}'
                catch_stmt
          { $$ = expr_try_new(PXC_FN(@1), PXC_LN(@1),
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                        passby_e_mutable_value, $3),
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0,
                        expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $8, $7,
                                passby_e_const_reference, 0),
                0, passby_e_mutable_value, $11), $13); }
        ;
catch_stmt
        :
          { $$ = 0; }
        | TOK_CATCH '(' type_expr TOK_SYMBOL ')' '{'
                function_body_stmt_list '}' catch_stmt
          { $$ = expr_try_new(PXC_FN(@1), PXC_LN(@1), 0,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0,
                        expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $3,
                                passby_e_const_reference, 0),
                0, passby_e_mutable_value, $7), $9); }
        ;
ext_stmt
        : TOK_NAMESPACE nssym_expr opt_nssafety ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $2, false, false,
                false, 0, $3); }
        | TOK_PRIVATE TOK_NAMESPACE nssym_expr opt_nssafety ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $3, false, false,
                false, 0, $4); }
        | TOK_PUBLIC TOK_NAMESPACE nssym_expr opt_nssafety ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $3, false, true,
                false, 0, $4); }
        | TOK_THREADED TOK_NAMESPACE nssym_expr opt_nssafety ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $3, false, false,
                true, 0, $4); }
        | TOK_PRIVATE TOK_THREADED TOK_NAMESPACE nssym_expr opt_nssafety ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $4, false, false,
                true, 0, $5); }
        | TOK_PUBLIC TOK_THREADED TOK_NAMESPACE nssym_expr opt_nssafety ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $4, false, true,
                true, 0, $5); }
        | TOK_IMPORT nssym_expr opt_nsalias ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $2, true, false,
                false, $3, 0); }
        | TOK_PRIVATE TOK_IMPORT nssym_expr opt_nsalias ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $3, true, false,
                false, $4, 0); }
        | TOK_PUBLIC TOK_IMPORT nssym_expr opt_nsalias ';'
          { $$ = expr_ns_new(PXC_FN(@1), PXC_LN(@1), $3, true, true,
                false, $4, 0); }
        | opt_attribute /* dummy */ TOK_EXTERN TOK_STRLIT TOK_INLINE
          { $$ = expr_inline_c_new(PXC_FN(@2), PXC_LN(@2),
                arena_dequote_strdup($3), arena_decode_inline_strdup($4),
                cur_mode != compile_mode_main, 0); }
          /* inlined c++ code */
        | opt_attribute /* dummy */ TOK_EXTERN TOK_STRLIT TOK_STRLIT ';'
          { $$ = expr_inline_c_new(PXC_FN(@2), PXC_LN(@2),
                arena_dequote_strdup($3), arena_dequote_strdup($4),
                cur_mode != compile_mode_main, 0); }
          /* c++ compilation flags */
        ;
opt_nsalias
        :
          { $$ = 0; }
        | TOK_SYMBOL
          { $$ = $1; }
        | '-'
          { $$ = "-"; }
        | '+'
          { $$ = "+"; }
        | '*'
          { $$ = "*"; }
        ;

opt_nssafety
        :
          { $$ = 0; }
        | TOK_STRLIT
          { $$ = arena_dequote_strdup($1); }
        ;
funcdef_stmt
        : opt_attribute TOK_FUNCTION opt_tparams_expr type_expr TOK_SYMBOL
                '(' argdecl_list ')' opt_cv '{' function_body_stmt_list '}'
          { $$ = expr_funcdef_new(PXC_FN(@2), PXC_LN(@2), $5, 0, 0, $9,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $3, 0, $7, $4,
                        passby_e_mutable_value, $11),
                cur_mode != compile_mode_main, false, $1); }
        | opt_attribute TOK_FUNCTION opt_tparams_expr type_expr TOK_SYMBOL
                '(' argdecl_list ')' opt_cv ';'
          /* prototype decl only */
          { $$ = expr_funcdef_new(PXC_FN(@2), PXC_LN(@2), $5, 0, 0, $9,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $3, 0, $7, $4,
                        passby_e_mutable_value, 0),
                cur_mode != compile_mode_main, true, $1); }
        | opt_attribute TOK_FUNCTION TOK_EXTERN TOK_STRLIT opt_copt
                opt_tparams_expr type_expr opt_passby TOK_SYMBOL
                '(' argdecl_list ')' opt_cv '{' function_body_stmt_list '}'
          /* extern with body */
          { $$ = expr_funcdef_new(PXC_FN(@2), PXC_LN(@2), $9,
                arena_dequote_strdup($4), $5, $13,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $6, 0, $11, $7,
                        passby_e($8), $15),
                cur_mode != compile_mode_main, false, $1); }
        | opt_attribute TOK_FUNCTION '~' '{' function_body_stmt_list '}'
          { $$ = expr_funcdef_new(PXC_FN(@2), PXC_LN(@2), 0, 0, 0, false,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), 0, 0, 0, 0,
                        passby_e_mutable_value, $5),
                cur_mode != compile_mode_main, false, $1); }
        | opt_attribute TOK_FUNCTION opt_tparams_expr TOK_EXPAND TOK_SYMBOL
                '{' function_body_stmt_list '}'
          { $$ = expr_funcdef_new(PXC_FN(@2), PXC_LN(@2), $5, 0, 0, false,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $3, 0, 0, 0,
                        passby_e_mutable_value, $7),
                cur_mode != compile_mode_main, false, $1); }
        ;
c_funcdecl_stmt
        : opt_attribute TOK_FUNCTION TOK_EXTERN TOK_STRLIT opt_copt
                opt_tparams_expr type_expr opt_passby TOK_SYMBOL '('
                argdecl_list ')' opt_cv ';'
          /* extern without body */
          { $$ = expr_funcdef_new(PXC_FN(@2), PXC_LN(@2), $9,
                arena_dequote_strdup($4), $5, $13,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $6, 0, $11, $7,
                        passby_e($8), 0),
                false, true, $1); }
        ;
opt_copt
        :
                { $$ = 0; }
        | TOK_STRLIT
          { $$ = arena_dequote_strdup($1); }
        ;
funcdecl_stmt
        : opt_attribute TOK_FUNCTION type_expr TOK_SYMBOL
                '(' argdecl_list ')' opt_cv ';'
          { $$ = expr_funcdef_new(PXC_FN(@2), PXC_LN(@2), $4, 0, 0, $8,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), 0, 0, $6, $3,
                        passby_e_mutable_value, 0),
                false, true, $1); }
        ;
opt_passby
        :
                { $$ = passby_e_mutable_value; }
        | TOK_CONST
                { $$ = passby_e_const_value; }
        | TOK_MUTABLE
                { $$ = passby_e_mutable_value; }
        | '&'
                { $$ = passby_e_mutable_reference; }
        | TOK_CONST '&'
                { $$ = passby_e_const_reference; }
        | TOK_MUTABLE '&'
                { $$ = passby_e_mutable_reference; }
        ;
opt_cv
        :
                { $$ = 0; }
        | TOK_CONST
                { $$ = 1; }
        ;
opt_attribute
        : opt_attribute_threading
                { $$ = $1; }
        | TOK_PRIVATE opt_attribute_threading
                { $$ = attribute_e(attribute_private | $2); }
        | TOK_PUBLIC opt_attribute_threading
                { $$ = attribute_e(attribute_public | $2); }
        ;
opt_attribute_threading
        :
                { $$ = attribute_unknown; }
        | TOK_THREADED
                { $$ = attribute_threaded; }
        | TOK_MULTITHR
                { $$ = attribute_e(attribute_threaded | attribute_multithr); }
        | TOK_VALUETYPE
                { $$ = attribute_e(attribute_threaded | attribute_valuetype); }
        | TOK_MTVALUETYPE
                { $$ = attribute_e(attribute_threaded | attribute_multithr |
                        attribute_valuetype); }
        | TOK_TSVALUETYPE
                { $$ = attribute_e(attribute_threaded | attribute_multithr |
                        attribute_valuetype | attribute_tsvaluetype); }
        | TOK_PURE opt_attribute_threading
                { $$ = attribute_e(attribute_threaded | attribute_pure | $2); }
        | TOK_EPHEMERAL opt_attribute_threading
                { $$ = attribute_e(attribute_threaded | attribute_pure |
                        attribute_ephemeral | $2); }
        ;
struct_stmt
        : opt_attribute TOK_STRUCT opt_extern opt_strlit opt_tparams_expr
                TOK_SYMBOL opt_inherit_expr opt_private
                '{' struct_body_stmt_list '}'
          { $$ = expr_struct_new(PXC_FN(@2), PXC_LN(@2), $6,
                $3 != 0 ? arena_dequote_strdup($3) : 0,
                $4 != 0 ? arena_dequote_strdup($4) : 0,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $5, $7, 0, 0,
                        passby_e_mutable_value, $10),
                $1, false, $8 != 0 ? true : false); }
        | opt_attribute TOK_STRUCT opt_extern opt_strlit opt_tparams_expr
                TOK_SYMBOL '(' argdecl_list ')' opt_inherit_expr opt_private
                '{' struct_body_stmt_list '}'
          { $$ = expr_struct_new(PXC_FN(@2), PXC_LN(@2), $6,
                $3 != 0 ? arena_dequote_strdup($3) : 0,
                $4 != 0 ? arena_dequote_strdup($4) : 0,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $5, $10, $8, 0,
                        passby_e_mutable_value, $13),
                $1, true, $11 != 0 ? true : false); }
        ;
opt_private
        :
          { $$ = 0; }
        | TOK_PUBLIC
          { $$ = 0; }
        | TOK_PRIVATE
          { $$ = 1; }
        ;
opt_strlit
        :
          { $$ = 0; }
        | TOK_STRLIT
          { $$ = $1; }
        ;
dunion_stmt
        : opt_attribute TOK_UNION opt_tparams_expr TOK_SYMBOL '{'
                dunion_body_stmt_list '}'
          { $$ = expr_dunion_new(PXC_FN(@2), PXC_LN(@2), $4,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $3, 0, 0, 0,
                        passby_e_mutable_value, $6),
                $1); }
        ;
opt_inherit_expr
        :
          { $$ = 0; }
        | '<' type_arg_list '>'
          { $$ = $2; }
        ;
interface_stmt
        : opt_attribute TOK_INTERFACE opt_extern opt_tparams_expr TOK_SYMBOL
                opt_inherit_expr '{' interface_body_stmt_list '}'
          { $$ = expr_interface_new(PXC_FN(@2), PXC_LN(@2), $5,
                $3 != 0 ? arena_dequote_strdup($3) : 0, 0,
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $4, $6, 0, 0,
                        passby_e_mutable_value, $8),
                $1); }
        | opt_attribute TOK_INTERFACE opt_extern opt_tparams_expr TOK_SYMBOL
                TOK_SYMBOL '{' interface_body_stmt_list '}'
          { $$ = expr_interface_new(PXC_FN(@2), PXC_LN(@2), $5,
                $3 != 0 ? arena_dequote_strdup($3) : 0,
                expr_symbol_new(PXC_FN(@2), PXC_LN(@2),
                        expr_nssym_new(PXC_FN(@1), PXC_LN(@1), 0, $6)),
                expr_block_new(PXC_FN(@2), PXC_LN(@2), $4, 0, 0, 0,
                        passby_e_mutable_value, $8),
                $1); }
        ;
opt_extern
        :
          { $$ = 0; }
        | TOK_EXTERN TOK_STRLIT
          { $$ = $2; }
        ;
enum_stmt
        : opt_attribute TOK_ENUM TOK_SYMBOL '{' enum_value_list '}'
          { $$ = expr_typedef_new(PXC_FN(@2), PXC_LN(@2), $3,
                0, 0, true, false, $5, 0, $1); }
        | opt_attribute TOK_BITMASK TOK_SYMBOL '{' enum_value_list '}'
          { $$ = expr_typedef_new(PXC_FN(@2), PXC_LN(@2), $3,
                0, 0, false, true, $5, 0, $1); }
        ;
enum_value_list
        :
          { $$ = 0; }
        | TOK_SYMBOL '=' int_literal
          { $$ = expr_enumval_new(PXC_FN(@1), PXC_LN(@1), $1, 0,
                0, $3, attribute_public, 0); }
        | TOK_SYMBOL '=' int_literal ',' enum_value_list
          { $$ = expr_enumval_new(PXC_FN(@1), PXC_LN(@1), $1, 0,
                0, $3, attribute_public, $5); }
        ;
c_enumval_stmt
        : opt_attribute TOK_EXTERN TOK_STRLIT type_expr TOK_SYMBOL ';'
         { $$ = expr_enumval_new(PXC_FN(@2), PXC_LN(@2), $5, $4,
                arena_dequote_strdup($3), 0, $1, 0); }
        ;
metafdef_stmt
        : opt_attribute TOK_METAFUNCTION TOK_SYMBOL type_arg_excl_metalist ';'
          { $$ = expr_metafdef_new(PXC_FN(@2), PXC_LN(@2), $3, 0, $4, $1); }
        | opt_attribute TOK_METAFUNCTION TOK_SYMBOL '{' type_arg_list '}' ';'
          { $$ = expr_metafdef_new(PXC_FN(@2), PXC_LN(@2), $3, 0,
                  expr_metalist_new($5), $1); }
        | opt_attribute TOK_METAFUNCTION TOK_SYMBOL '{' type_arg_list '}'
                type_arg ';'
          { $$ = expr_metafdef_new(PXC_FN(@2), PXC_LN(@2), $3, $5, $7, $1); }
          /* $5 expects tparam_list but parsed as type_arg_list to avoid
           conflicts. it will be converted to expr_tparams when compiled. */
        | opt_attribute TOK_METAFUNCTION TOK_SYMBOL '{' tparam_variadic '}'
                type_arg ';'
          /* variadic metafunction */
          { $$ = expr_metafdef_new(PXC_FN(@2), PXC_LN(@2), $3, $5, $7, $1); }
        ;
visi_vardef_stmt
        : visibility type_expr opt_mutable TOK_SYMBOL ';'
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $4, $2,
                $3 ? passby_e_mutable_value : passby_e_const_value,
                $1, 0); }
        | visibility type_expr opt_mutable TOK_SYMBOL '=' assign_expr ';'
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '=',
                expr_var_new(PXC_FN(@1), PXC_LN(@1), $4, $2,
                        $3 ? passby_e_mutable_value : passby_e_const_value,
                        $1, $6),
                $6); }
        ;
visi_union_flddef_stmt
        : visibility type_expr TOK_SYMBOL ';'
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, $2,
                passby_e_mutable_value, $1, 0); }
        ;
visibility
        : TOK_PUBLIC
          { $$ = attribute_public; }
        | TOK_PRIVATE
          { $$ = attribute_private; }
        ;
opt_mutable
        :
          { $$ = 1; }
        | TOK_MUTABLE
          { $$ = 1; }
        | TOK_CONST
          { $$ = 0; }
        ;
vardef_stmt
        : vardef_expr ';'
          { $$ = $1; }
        | vardef_expr '=' assign_expr ';'
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '=', $1, $3); }
        | vardef_expr '=' assign_expr unnamed_funcdef
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '=', $1,
                expr_add_te(PXC_FN(@1), PXC_LN(@1), $3, $4)); }
        | vardef_expr '=' postfix_expr '(' opt_expression ')'
                simple_unnamed_funcdef
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '=', $1,
                expr_add_te(PXC_FN(@1), PXC_LN(@1),
                        expr_funccall_new(PXC_FN(@1), PXC_LN(@1), $3, $5),
                        $7)); }
        ;
vardef_expr
        : type_expr TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $2, $1,
                passby_e_mutable_value, attribute_unknown, 0); }
        | type_expr TOK_CONST TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_const_value, attribute_unknown, 0); }
        | TOK_CONST type_expr TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, $2,
                passby_e_const_value, attribute_unknown, 0); }
        | type_expr TOK_MUTABLE TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_mutable_value, attribute_unknown, 0); }
        | TOK_MUTABLE type_expr TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, $2,
                passby_e_mutable_value, attribute_unknown, 0); }
        | type_expr TOK_CONST '&' TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                passby_e_const_reference, attribute_unknown, 0); }
        | TOK_CONST type_expr '&' TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $4, $2,
                passby_e_const_reference, attribute_unknown, 0); }
        | type_expr TOK_MUTABLE '&' TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                passby_e_mutable_reference, attribute_unknown, 0); }
        | TOK_MUTABLE type_expr '&' TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $4, $2,
                passby_e_mutable_reference, attribute_unknown, 0); }
        /*
        // prohibited to avoid conflict
        | type_expr '&' TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_mutable_reference, attribute_unknown, 0); }
        */
        | TOK_CONST TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                passby_e_const_value, attribute_unknown, 0); }
        | TOK_MUTABLE TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                passby_e_mutable_value, attribute_unknown, 0); }
        | TOK_CONST '&' TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, 0,
                passby_e_const_reference, attribute_unknown, 0); }
        | TOK_MUTABLE '&' TOK_SYMBOL
          { $$ = expr_var_new(PXC_FN(@1), PXC_LN(@1), $3, 0,
                passby_e_mutable_reference, attribute_unknown, 0); }
        ;
argdecl_key
        : type_expr TOK_SYMBOL ',' argdecl_mapped
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, $1,
                passby_e_mutable_value, $4); }
        | TOK_CONST TOK_SYMBOL ',' argdecl_mapped
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                passby_e_const_value, $4); }
        | type_expr TOK_CONST TOK_SYMBOL ',' argdecl_mapped
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_const_value, $5); }
        | TOK_CONST '&' TOK_SYMBOL ',' argdecl_mapped
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, 0,
                passby_e_const_reference, $5); }
        | type_expr TOK_CONST '&' TOK_SYMBOL ',' argdecl_mapped
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                passby_e_const_reference, $6); }
        ;
argdecl_mapped
        : type_expr TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, $1,
                passby_e_mutable_value, 0); }
        | TOK_CONST TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                passby_e_const_value, 0); }
        | type_expr TOK_CONST TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_const_value, 0); }
        | TOK_CONST '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, 0,
                passby_e_const_reference, 0); }
        | type_expr TOK_CONST '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                passby_e_const_reference, 0); }
        /*
        | TOK_MUTABLE TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, 0,
                passby_e_mutable_value, 0); }
        | type_expr TOK_MUTABLE TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_mutable_value, 0); }
        */
        | TOK_MUTABLE '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, 0,
                passby_e_mutable_reference, 0); }
        | type_expr TOK_MUTABLE '&' TOK_SYMBOL
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                passby_e_mutable_reference, 0); }
        ;
argdecl_list
        :
          { $$ = 0; }
        | TOK_EXPAND '(' type_arg ')' argdecl_list_trail
          { $$ = expr_expand_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, $3, 0,
                expand_e_argdecls, $5); }
        | type_expr TOK_SYMBOL argdecl_list_trail
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $2, $1,
                passby_e_mutable_value, $3); }
        /*
        | type_expr TOK_AUTO TOK_SYMBOL argdecl_list_trail
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_unspecified, $4); }
        */
        | type_expr TOK_CONST TOK_SYMBOL argdecl_list_trail
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_const_value, $4); }
        | type_expr TOK_MUTABLE TOK_SYMBOL argdecl_list_trail
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_mutable_value, $4); }
        /*
        */
        | type_expr '&' TOK_SYMBOL argdecl_list_trail
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $3, $1,
                passby_e_mutable_reference, $4); }
        | type_expr TOK_CONST '&' TOK_SYMBOL argdecl_list_trail
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                passby_e_const_reference, $5); }
        | type_expr TOK_MUTABLE '&' TOK_SYMBOL argdecl_list_trail
          { $$ = expr_argdecls_new(PXC_FN(@1), PXC_LN(@1), $4, $1,
                passby_e_mutable_reference, $5); }
        ;
argdecl_list_trail
        :
          { $$ = 0; }
        | ',' argdecl_list
          { $$ = $2; }
        ;
type_expr
        : nssym_expr
          { $$ = expr_te_new(PXC_FN(@1), PXC_LN(@1), $1, 0); }
        | nssym_expr '{' type_arg_list '}'
          { $$ = expr_te_new(PXC_FN(@1), PXC_LN(@1), $1, $3); }
        /*
        | nssym_expr ':' type_expr
          { $$ = expr_te_local_chain_new(
                expr_te_new(PXC_FN(@1), PXC_LN(@1), $1, 0), $3); }
        | nssym_expr '{' type_arg_list '}' ':' type_expr
          { $$ = expr_te_local_chain_new(
                expr_te_new(PXC_FN(@1), PXC_LN(@1), $1, $3), $6); }
        */
        ;
type_arg_excl_metalist
        : type_expr
          { $$ = $1; }
        | TOK_INTLIT
          { $$ = expr_int_literal_new(PXC_FN(@1), PXC_LN(@1), $1, false); }
        | '-' TOK_INTLIT
          { $$ = expr_int_literal_new(PXC_FN(@1), PXC_LN(@1),
                arena_concat_strdup("-", $2), false); }
        | TOK_UINTLIT
          { $$ = expr_int_literal_new(PXC_FN(@1), PXC_LN(@1), $1, true); }
        | TOK_STRLIT
          { $$ = expr_str_literal_new(PXC_FN(@1), PXC_LN(@1), $1); }
        | unnamed_funcdef
          { $$ = $1; }
        | TOK_METAFUNCTION '{' tparam_list '}' type_arg
          { $$ = expr_metafdef_new(PXC_FN(@1), PXC_LN(@1), 0, $3, $5,
                attribute_private); }
        | TOK_METAFUNCTION '{' tparam_variadic '}' type_arg
          /* variadic metafunction */
          { $$ = expr_metafdef_new(PXC_FN(@1), PXC_LN(@1), 0, $3, $5,
                attribute_private); }
        ;
simple_unnamed_funcdef
        : '{' function_body_stmt_list '}'
          { $$ = expr_funcdef_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, 0,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0,
                        expr_te_new(PXC_FN(@1), PXC_LN(@1),
                                expr_nssym_new(PXC_FN(@1), PXC_LN(@1),
                                        0, "void"), 0),
                        passby_e_mutable_value, $2),
                cur_mode != compile_mode_main, false, attribute_unknown); }
        | type_expr '(' argdecl_list ')' opt_cv
                '{' function_body_stmt_list '}'
          { $$ = expr_funcdef_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, $5,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), 0, 0, $3, $1,
                        passby_e_mutable_value, $7),
                cur_mode != compile_mode_main, false, attribute_unknown); }
        ;
unnamed_funcdef
        : TOK_FUNCTION opt_tparams_expr type_expr
                '(' argdecl_list ')' opt_cv '{' function_body_stmt_list '}'
          { $$ = expr_funcdef_new(PXC_FN(@1), PXC_LN(@1), 0, 0, 0, $7,
                expr_block_new(PXC_FN(@1), PXC_LN(@1), $2, 0, $5, $3,
                        passby_e_mutable_value, $9),
                cur_mode != compile_mode_main, false, attribute_unknown); }
        ;
type_arg
        : type_arg_excl_metalist
          { $$ = $1; }
        | '{' type_arg_list '}'
          { $$ = expr_metalist_new($2); }
        ;
type_arg_list
        : type_arg
          { $$ = expr_telist_new(PXC_FN(@1), PXC_LN(@1), $1, 0); }
        | type_arg ',' type_arg_list
          { $$ = expr_telist_new(PXC_FN(@1), PXC_LN(@1), $1, $3); }
        ;
nssym_expr
        : TOK_SYMBOL
          { $$ = expr_nssym_new(PXC_FN(@1), PXC_LN(@1), 0, $1); }
        | nssym_expr TOK_NSDELIM TOK_SYMBOL
          { $$ = expr_nssym_new(PXC_FN(@1), PXC_LN(@1), $1, $3); }
        ;
opt_expression
        :
          { $$ = 0; }
        | expression
          { $$ = $1; }
        ;
expression
        : assign_expr
          { $$ = $1; }
        | expression ',' assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), ',', $1, $3); }
        ;
assign_expr
        : cond_expr
          { $$ = $1; }
        | TOK_EXPAND '(' TOK_SYMBOL opt_symbol ':' type_arg ';'
                assign_expr ')'
          { $$ = expr_expand_new(PXC_FN(@1), PXC_LN(@1), 0, $3, $4, $6, $8,
                expand_e_comma, 0); }
        | unary_expr '=' assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '=', $1, $3); }
        | unary_expr TOK_ADD_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_ADD_ASSIGN,
                $1, $3); }
        | unary_expr TOK_SUB_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_SUB_ASSIGN,
                $1, $3); }
        | unary_expr TOK_MUL_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_MUL_ASSIGN,
                $1, $3); }
        | unary_expr TOK_DIV_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_DIV_ASSIGN,
                $1, $3); }
        | unary_expr TOK_MOD_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_MOD_ASSIGN,
                $1, $3); }
        | unary_expr TOK_OR_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_OR_ASSIGN,
                $1, $3); }
        | unary_expr TOK_AND_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_AND_ASSIGN,
                $1, $3); }
        | unary_expr TOK_XOR_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_XOR_ASSIGN,
                $1, $3); }
        | unary_expr TOK_SHIFTL_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_SHIFTL_ASSIGN,
                $1, $3); }
        | unary_expr TOK_SHIFTR_ASSIGN assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_SHIFTR_ASSIGN,
                $1, $3); }
        | unary_expr TOK_EXTERN TOK_STRLIT assign_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_EXTERN, $1, $4,
                arena_dequote_strdup($3)); }
        ;
cond_expr
        : lor_expr
          { $$ = $1; }
        | lor_expr '?' expression ':' cond_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '?', $1,
                 expr_op_new(PXC_FN(@1), PXC_LN(@1), ':', $3, $5)); }
        ;
lor_expr
        : land_expr
          { $$ = $1; }
        | lor_expr TOK_LOR land_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_LOR, $1, $3); }
        ;
land_expr
        : or_expr
          { $$ = $1; }
        | land_expr TOK_LAND or_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_LAND, $1, $3); }
        ;
or_expr
        : xor_expr
          { $$ = $1; }
        | or_expr '|' xor_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '|', $1, $3); }
        ;
xor_expr
        : and_expr
          { $$ = $1; }
        | xor_expr '^' and_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '^', $1, $3); }
        ;
and_expr
        : equality_expr
          { $$ = $1; }
        | and_expr '&' equality_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '&', $1, $3); }
        ;
equality_expr
        : rel_expr
          { $$ = $1; }
        | equality_expr TOK_EQ rel_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_EQ, $1, $3); }
        | equality_expr TOK_NE rel_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_NE, $1, $3); }
        ;
rel_expr
        : shift_expr
          { $$ = $1; }
        | rel_expr '<' shift_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '<', $1, $3); }
        | rel_expr '>' shift_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '>', $1, $3); }
        | rel_expr TOK_LE shift_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_LE, $1, $3); }
        | rel_expr TOK_GE shift_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_GE, $1, $3); }
        ;
shift_expr
        : add_expr
          { $$ = $1; }
        | shift_expr TOK_SHIFTL add_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_SHIFTL, $1, $3); }
        | shift_expr TOK_SHIFTR add_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_SHIFTR, $1, $3); }
        ;
add_expr
        : mul_expr
          { $$ = $1; }
        | add_expr '+' mul_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '+', $1, $3); }
        | add_expr '-' mul_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '-', $1, $3); }
        ;
mul_expr
        : unary_expr
          { $$ = $1; }
        | mul_expr '*' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '*', $1, $3); }
        | mul_expr '/' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '/', $1, $3); }
        | mul_expr '%' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '%', $1, $3); }
        ;
unary_expr
        : postfix_expr
          { $$ = $1; }
        | TOK_INC unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_INC, $2, 0); }
        | TOK_DEC unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_DEC, $2, 0); }
        | '!' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '!', $2, 0); }
        | '~' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '~', $2, 0); }
        | '*' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_PTR_DEREF, $2, 0); }
        | '+' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_PLUS, $2, 0); }
        | '-' unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_MINUS, $2, 0); }
        | TOK_CASE unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_CASE, $2, 0); }
        | '+' TOK_EXTERN TOK_STRLIT unary_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_EXTERN, $4, 0,
                arena_dequote_strdup($3)); }
        ;
postfix_expr
        : primary_expr
          { $$ = $1; }
        | postfix_expr '[' array_index_expr ']'
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '[', $1, $3); }
        | postfix_expr '(' opt_expression ')'
          { $$ = expr_funccall_new(PXC_FN(@1), PXC_LN(@1), $1, $3); }
        | postfix_expr '.' symbol_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '.', $1, $3); }
        | postfix_expr TOK_ARROW symbol_expr
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_ARROW,
                expr_op_new(PXC_FN(@1), PXC_LN(@1), '(',
                        expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_PTR_DEREF,
                                $1, 0), 0),
                $3); }
        ;
array_index_expr
        : expression
          { $$ = $1; }
        | expression TOK_SLICE expression
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), TOK_SLICE, $1, $3); }
        ;
primary_expr
        : int_literal
          { $$ = $1; }
        | TOK_FLOATLIT
          { $$ = expr_float_literal_new(PXC_FN(@1), PXC_LN(@1), $1); }
        | TOK_BOOLLIT
          { $$ = expr_bool_literal_new(PXC_FN(@1), PXC_LN(@1), $1); }
        | TOK_STRLIT
          { $$ = expr_str_literal_new(PXC_FN(@1), PXC_LN(@1), $1); }
        | symbol_expr
          { $$ = $1; }
        | '(' expression ')'
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '(', $2, 0); }
        | '{' expression '}'
          { $$ = expr_op_new(PXC_FN(@1), PXC_LN(@1), '{', $2, 0); }
        ;
int_literal
        : TOK_INTLIT
          { $$ = expr_int_literal_new(PXC_FN(@1), PXC_LN(@1), $1, false); }
        | TOK_UINTLIT
          { $$ = expr_int_literal_new(PXC_FN(@1), PXC_LN(@1), $1, true); }
        ;
symbol_expr
        : nssym_expr
          { $$ = expr_symbol_new(PXC_FN(@1), PXC_LN(@1), $1); }
        | nssym_expr '{' type_arg_list '}'
          { $$ = expr_te_new(PXC_FN(@1), PXC_LN(@1), $1, $3); }
        ;

%%

namespace pxc {

struct parser_initobj {
        parser_initobj() {
        }
        void set(const char *data, size_t len) {
                pxc_lex_set_buffer(data, len);
        }
        ~parser_initobj() {
                pxc_lex_unset_buffer();
        }
private:
        parser_initobj(const parser_initobj&);
        parser_initobj& operator =(const parser_initobj&);
};

void pxc_parse_file(const source_info& si, imports_type& imports_r)
{
        parser_initobj pi;
        cur_mode = si.mode;
        topval = 0;
        std::list<expr_i *> topvals;
        for (module_info::source_files_type::const_iterator i
                = si.minfo->source_files.begin();
                i != si.minfo->source_files.end(); ++i) {
                if (i->content == nullptr) {
                        continue;
                }
                const std::string& s = i->content->content;
                if (s.empty()) {
                        continue;
                }
                pi.set(s.data(), s.size());
                for (auto frsz: i->content->fragment_sizes) {
                        auto fn = arena_strdup(frsz.first.c_str());
                        fragment_sizes.push_back(
                                std::make_pair(fn, (int)frsz.second));
                }
                cur_fname = arena_strdup(i->filename_trim.c_str());
                assert(s.size());
                yylloc.first_line = 1;
                yyparse();
                arena_error_throw_pushed();
                topvals.push_back(topval);
                topval = 0;
                fragment_sizes.clear();
        }
        arena_append_topval(topvals, si.mode == compile_mode_main, imports_r);
        cur_fname = 0;
        cur_mode = compile_mode_linkonly;
        topval = 0;
}

}; /* namespace pxc */

void yyerror(const char *message)
{
        arena_error_push(0, "%s:%d: %s", cur_fname, yylloc.first_line,
                message);
}

