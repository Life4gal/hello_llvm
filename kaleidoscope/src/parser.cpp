#include <kaleidoscope/parser.hpp>

#include <iomanip>
#include <iostream>

#include <llvm-12/llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <kaleidoscope/details/KaleidoscopeJIT.hpp>

namespace hello_llvm
{
	int parser::get_token_precedence()
	{
		// todo: is-ascii was deprecated
		if (!isascii(curr_tok_)) { return -1; }

		// Make sure it's a declared bin_op
		const int tok_prec = bin_op_precedence_[static_cast<char>(curr_tok_)];
		if (tok_prec <= 0) { return -1; }
		return tok_prec;
	}

	std::unique_ptr<expr_ast> parser::parse_number_expr()
	{
		auto result = std::make_unique<number_expr_ast>(tok_.num_val);
		get_next_token();// consume the number
		return result;
	}

	std::unique_ptr<expr_ast> parser::parse_paren_expr()
	{
		get_next_token();// eat (
		auto v = parse_expression();
		if (!v) { return nullptr; }

		if (curr_tok_ != ')') { return log_error("expected ')'"); }
		get_next_token();// eat )
		return v;
	}

	std::unique_ptr<expr_ast> parser::parse_identifier_expr()
	{
		std::string id_name = tok_.identifier_str;

		get_next_token();// eat identifier.

		if (curr_tok_ != '(')
		{
			// Simple variable ref.
			return std::make_unique<variable_expr_ast>(id_name);
		}

		// Call
		get_next_token();// eat (

		std::vector<std::unique_ptr<expr_ast>> args;
		if (curr_tok_ != ')')
		{
			while (true)
			{
				if (auto arg = parse_expression(); arg) { args.push_back(std::move(arg)); }
				else { return nullptr; }

				if (curr_tok_ == ')') { break; }

				if (curr_tok_ != ',') { return log_error("expected ')' or ',' in argument list"); }

				get_next_token();
			}
		}

		// Eat the ')'
		get_next_token();
		return std::make_unique<call_expr_ast>(id_name, std::move(args));
	}

	std::unique_ptr<expr_ast> parser::parse_primary()
	{
		switch (curr_tok_)
		{
			case tokenizer::tok_identifier: return parse_identifier_expr();
			case tokenizer::tok_number: return parse_number_expr();
			case '(': return parse_paren_expr();
			default: return log_error("unknown token when expecting an expression");
		}
	}

	std::unique_ptr<expr_ast> parser::parse_bin_op_rhs(int expr_prec, std::unique_ptr<expr_ast> lhs)
	{
		// If this is a bin_op, find its precedence.
		while (true)
		{
			const auto tok_prec = get_token_precedence();

			// If this is a bin_op that binds at least as tightly as the current bin_op,
			// consume it, otherwise we are done.
			if (tok_prec < expr_prec) { return lhs; }

			// Okay, we know this is a bin_op.
			auto bin_op = curr_tok_;
			get_next_token();// eat bin_op

			// Parse the primary expression after the binary operator.
			auto rhs = parse_primary();
			if (!rhs) { return nullptr; }

			// If bin_op binds less tightly with RHS than the operator after RHS, let
			// the pending operator take RHS as its lhs.
			const auto next_prec = get_token_precedence();
			if (tok_prec < next_prec)
			{
				rhs = parse_bin_op_rhs(tok_prec + 1, std::move(rhs));
				if (!rhs) { return nullptr; }
			}

			// Merge lhs/rhs.
			lhs = std::make_unique<binary_expr_ast>(bin_op, std::move(lhs), std::move(rhs));
		}
	}

	std::unique_ptr<expr_ast> parser::parse_expression()
	{
		auto lhs = parse_primary();
		if (!lhs) { return nullptr; }

		return parse_bin_op_rhs(0, std::move(lhs));
	}

	std::unique_ptr<prototype_ast> parser::parse_prototype()
	{
		if (curr_tok_ != tokenizer::tok_identifier) { return log_error_p("expected function name in prototype"); }

		std::string func_name = tok_.identifier_str;
		get_next_token();

		if (curr_tok_ != '(') { return log_error_p("expected '(' in prototype"); }

		std::vector<std::string> arg_names;
		while (get_next_token() == tokenizer::tok_identifier) { arg_names.push_back(tok_.identifier_str); }

		if (curr_tok_ != ')') { return log_error_p("expected ')' in prototype"); }

		// success
		get_next_token();// eat ')'

		return std::make_unique<prototype_ast>(func_name, std::move(arg_names));
	}

	std::unique_ptr<function_ast> parser::parse_definition()
	{
		get_next_token();// eat def
		auto proto = parse_prototype();
		if (!proto) { return nullptr; }

		if (auto e = parse_expression(); e) { return std::make_unique<function_ast>(std::move(proto), std::move(e)); }
		return nullptr;
	}

	std::unique_ptr<function_ast> parser::parse_top_level_expr()
	{
		if (auto e = parse_expression(); e)
		{
			// Make an anonymous proto
			auto proto = std::make_unique<prototype_ast>("__anon_expr__", std::vector<std::string>());
			return std::make_unique<function_ast>(std::move(proto), std::move(e));
		}
		return nullptr;
	}

	std::unique_ptr<prototype_ast> parser::parse_extern()
	{
		get_next_token();// eat extern
		return parse_prototype();
	}

	void parser::handle_definition()
	{
		if (const auto func_ast = parse_definition(); func_ast)
		{
			if (auto* func_ir = func_ast->codegen(); func_ir)
			{
				std::cerr << "Read function definition: \n";
				func_ir->print(llvm::errs());
				std::cerr << '\n';

				const auto& context = global_context::get();
				auto [m, c] = global_context::refresh();
				context.exit_on_error(context.jit->addModule(llvm::orc::ThreadSafeModule(std::move(m), std::move(c))));
			}
		}
		else
		{
			// Skip token for error recovery.
			get_next_token();
		}
	}

	void parser::handle_extern()
	{
		if (auto proto_ast = parse_extern(); proto_ast)
		{
			if (auto* func_ir = proto_ast->codegen(); func_ir)
			{
				std::cerr << "Read extern: \n";
				func_ir->print(llvm::errs());
				std::cerr << '\n';

				global_context::insert_or_assign(std::move(proto_ast));
			}
		}
		else
		{
			// Skip token for error recovery.
			get_next_token();
		}
	}

	void parser::handle_top_level_expression()
	{
		auto& context = global_context::get();
		// Evaluate a top-level expression into an anonymous function.
		if (const auto func_ast = parse_top_level_expr(); func_ast)
		{
			if (auto* func_ir = func_ast->codegen(); func_ir)
			{
				std::cerr << "Read top-level expression: \n";
				func_ir->print(llvm::errs());
				std::cerr << '\n';

				// Create a ResourceTracker to track JIT 'd memory allocated to our
				// anonymous expression -- that way we can free it after executing.
				const auto rt = context.jit->getMainJITDylib().createResourceTracker();

				auto tsm = llvm::orc::ThreadSafeModule(std::move(context.module), std::move(context.context));
				context.exit_on_error(context.jit->addModule(std::move(tsm), rt));
				global_context::refresh();

				// Search the JIT for the __anon_expr__ symbol.
				const auto expr = context.exit_on_error(context.jit->lookup("__anon_expr__"));

				// Get the symbol's address and cast it to the right type (takes no
				// arguments, returns a double) so we can call it as a native function.
				const auto fp = reinterpret_cast<double(*)()>(static_cast<std::intptr_t>(expr.getAddress()));
				std::cerr << "Evaluated to -->" << std::setw(8) << std::setprecision(3) << fp() << "\n\n";

				// Delete the anonymous expression module from the JIT.
				context.exit_on_error(rt->remove());
			}
		}
		else
		{
			// Skip token for error recovery.
			get_next_token();
		}
	}
}// namespace hello_llvm
