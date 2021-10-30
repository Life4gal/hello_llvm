#ifndef HELLO_LLVM_PARSER_HPP
#define HELLO_LLVM_PARSER_HPP

#include <map>

#include <kaleidoscope/ast.hpp>
#include <kaleidoscope/lexer.hpp>

namespace hello_llvm
{
	//===----------------------------------------------------------------------===//
	// Parser
	//===----------------------------------------------------------------------===//

	class parser
	{
		tokenizer tok_;

		/// curr_tok/get_next_token - Provide a simple token buffer.  curr_tok is the current
		/// token the parser is looking at.  get_next_token reads another token from the
		/// lexer and updates curr_tok with its results.

		int curr_tok_{};

		/// expression
		///   ::= unary binoprhs
		///   ::= bin_op_rhs
		std::unique_ptr<expr_ast> parse_expression();

		/// number_expr ::= number
		std::unique_ptr<expr_ast> parse_number_expr();

		/// paren_expr ::= '(' expression ')'
		std::unique_ptr<expr_ast> parse_paren_expr();

		/// identifier_expr
		///   ::= identifier
		///   ::= identifier '(' expression* ')'
		std::unique_ptr<expr_ast> parse_identifier_expr();

		/// if_expr ::= 'if' expression 'then' expression 'else' expression
		std::unique_ptr<expr_ast>	   parse_if_expr();

		/// for_expr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
		std::unique_ptr<expr_ast>	   parse_for_expr();

		/// primary
		///   ::= identifier_expr
		///   ::= number_expr
		///   ::= paren_expr
		///   ::= if_expr
		///   ::= for_expr
		std::unique_ptr<expr_ast> parse_primary();

		/// unary
		///   ::= primary
		///   ::= '!' unary
		std::unique_ptr<expr_ast>	   parse_unary_op();

		/// bin_op_rhs
		///   ::= ('+' unary)*
		std::unique_ptr<expr_ast> parse_bin_op_rhs(int expr_prec, std::unique_ptr<expr_ast> lhs);

		/// prototype
		///   ::= id '(' id* ')'
		/// ::= binary LETTER number? (id, id)
		/// ::= unary LETTER (id)
		std::unique_ptr<prototype_ast> parse_prototype();

		/// definition ::= 'def' prototype expression
		std::unique_ptr<function_ast> parse_definition();

		/// top_level_expr ::= expression
		std::unique_ptr<function_ast> parse_top_level_expr();

		/// external ::= 'extern' prototype
		std::unique_ptr<prototype_ast> parse_extern();

	public:
		[[nodiscard]] int get_curr_token() const { return curr_tok_; }

		int get_next_token() { return curr_tok_ = tok_.get_token(); }

		void handle_definition();
		void handle_extern();
		void handle_top_level_expression();
	};
}// namespace hello_llvm

#endif//HELLO_LLVM_PARSER_HPP
