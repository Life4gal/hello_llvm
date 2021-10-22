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

		/// bin_op_precedence - This holds the precedence for each binary operator that is defined.
		std::map<char, int> bin_op_precedence_;

		/// GetTokPrecedence - Get the precedence of the pending binary operator token.
		int get_token_precedence();

		/// expression
		///   ::= primary bin_op_rhs
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

		/// bin_op_rhs
		///   ::= ('+' primary)*
		std::unique_ptr<expr_ast> parse_bin_op_rhs(int expr_prec, std::unique_ptr<expr_ast> lhs);

		/// prototype
		///   ::= id '(' id* ')'
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

		auto add_bin_op_precedence(char op, int precedence) { return bin_op_precedence_.emplace(op, precedence); }

		void handle_definition();
		void handle_extern();
		void handle_top_level_expression();
	};
}// namespace hello_llvm

#endif//HELLO_LLVM_PARSER_HPP
