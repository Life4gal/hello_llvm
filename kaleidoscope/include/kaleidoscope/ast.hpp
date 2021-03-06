#ifndef HELLO_LLVM_AST_HPP
#define HELLO_LLVM_AST_HPP

#include <llvm-12/llvm/IR/IRBuilder.h>
#include <llvm-12/llvm/Support/Error.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <iosfwd>

namespace llvm
{
	class LLVMContext;
	class Module;
	// template<>
	// class IRBuilder<>;
	class Value;
	class Function;

	namespace legacy
	{
		class FunctionPassManager;
	}

	namespace orc
	{
		class KaleidoscopeJIT;
	}
}

namespace hello_llvm
{
	class expr_ast;
	class number_expr_ast;
	class variable_expr_ast;
	class binary_expr_ast;
	class call_expr_ast;
	class prototype_ast;
	class function_ast;

	////===----------------------------------------------------------------------===//
	//// Top-Level parsing and JIT Driver
	////===----------------------------------------------------------------------===//
	struct global_context
	{
		llvm::ExitOnError exit_on_error;

		std::unique_ptr<llvm::LLVMContext> context;
		std::unique_ptr<llvm::Module> module;
		std::unique_ptr<llvm::IRBuilder<>> builder;
		std::unique_ptr<llvm::legacy::FunctionPassManager> fpm;
		std::unique_ptr<llvm::orc::KaleidoscopeJIT> jit;

		std::map<std::string, std::unique_ptr<prototype_ast>> functions_proto;
		std::map<std::string, llvm::Value*> named_values;

		/// bin_op_precedence - This holds the precedence for each binary operator that is defined.
		std::map<char, int> bin_op_precedence_;

		static global_context& get();

		/// GetTokPrecedence - Get the precedence of the pending binary operator token.
		[[nodiscard]] static int get_token_precedence(int tok);

		static auto add_bin_op_precedence(char op, int precedence)
		{
			return get().bin_op_precedence_.emplace(op, precedence);
		}

		static auto erase_bin_op(char op)
		{
			return get().bin_op_precedence_.erase(op);
		}

		[[nodiscard]] static std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>> refresh();

		[[nodiscard]] static llvm::Function*															  get_function(const std::string& name);

		static std::pair<decltype(functions_proto)::iterator, bool>						  insert_or_assign_function(std::unique_ptr<prototype_ast> ast);

	private:
		global_context();

		void new_module_and_context();
	};

	std::unique_ptr<expr_ast> log_error(const char* str);
	std::unique_ptr<prototype_ast> log_error_p(const char* str);
	llvm::Value* log_error_v(const char* str);

	//===----------------------------------------------------------------------===//
	// Abstract Syntax Tree (aka Parse Tree) and Code Generation
	//===----------------------------------------------------------------------===//

	/// expr_ast - Base class for all expression nodes.
	class expr_ast
	{
	public:
		virtual ~expr_ast();

		expr_ast() = default;
		expr_ast(const expr_ast& other) = default;
		expr_ast(expr_ast&& other) noexcept = default;
		expr_ast& operator=(const expr_ast& other) = default;
		expr_ast& operator=(expr_ast&& other) noexcept = default;

		virtual llvm::Value* codegen() = 0;
	};

	/// number_expr_ast - Expression class for numeric literals like "1.0".
	class number_expr_ast final : public expr_ast
	{
		double val_;

	public:
		explicit number_expr_ast(const double val)
			: val_(val) {}

		llvm::Value* codegen() override;
	};

	/// variable_expr_ast - Expression class for referencing a variable, like "a".
	class variable_expr_ast final : public expr_ast
	{
		std::string name_;

	public:
		explicit variable_expr_ast(std::string name)
			: name_(std::move(name)) {}

		llvm::Value* codegen() override;
	};

	#if __clang__
		#pragma clang	diagnostic push
		#pragma clang diagnostic ignored "-Wpadded"
	#endif

	/// unary_expr_ast - Expression class for a unary operator.
	class unary_expr_ast final : public expr_ast
	{
		// padding 7 bytes :(
		char op_;
		std::unique_ptr<expr_ast> operand_;

	public:
		unary_expr_ast(const char op, std::unique_ptr<expr_ast> operand)
			: op_(op),
			  operand_(std::move(operand)) {}

		llvm::Value* codegen() override;
	};

	/// binary_expr_ast - Expression class for a binary operator.
	class binary_expr_ast final : public expr_ast
	{
		// padding 7 bytes :(
		char op_;
		std::unique_ptr<expr_ast> lhs_;
		std::unique_ptr<expr_ast> rhs_;

	public:
		binary_expr_ast(const char op, std::unique_ptr<expr_ast> lhs, std::unique_ptr<expr_ast> rhs)
			: op_(op),
			  lhs_(std::move(lhs)),
			  rhs_(std::move(rhs)) {}

		llvm::Value* codegen() override;
	};

	#ifdef __clang__
		#pragma clang diagnostic pop
	#endif

	/// call_expr_ast - Expression class for function calls.
	class call_expr_ast final : public expr_ast
	{
		std::string callee_;
		std::vector<std::unique_ptr<expr_ast>> args_;

	public:
		call_expr_ast(std::string callee,
		              std::vector<std::unique_ptr<expr_ast>> args)
			: callee_(std::move(callee)),
			  args_(std::move(args)) {}

		llvm::Value* codegen() override;
	};

	/// if_expr_ast - Expression class for if/then/else.
	class if_expr_ast final : public expr_ast
	{
		std::unique_ptr<expr_ast> cond_;
		std::unique_ptr<expr_ast> then_;
		std::unique_ptr<expr_ast> else_;

	public:
		if_expr_ast(std::unique_ptr<expr_ast> cond, std::unique_ptr<expr_ast> then, std::unique_ptr<expr_ast> else_)
			: cond_(std::move(cond)),
			  then_(std::move(then)),
			  else_(std::move(else_)) {}

		llvm::Value* codegen() override;
	};

	/// ForExprAST - Expression class for for/in.
	class for_expr_ast final : public expr_ast
	{
		std::string cond_name_;
		std::unique_ptr<expr_ast> init_;
		std::unique_ptr<expr_ast> end_;
		std::unique_ptr<expr_ast> step_;
		std::unique_ptr<expr_ast> body_;

	public:
		for_expr_ast(std::string cond_var, std::unique_ptr<expr_ast> init, std::unique_ptr<expr_ast> end, std::unique_ptr<expr_ast> step, std::unique_ptr<expr_ast> body)
			: cond_name_(std::move(cond_var)),
			  init_(std::move(init)),
			  end_(std::move(end)),
			  step_(std::move(step)),
			  body_(std::move(body)) {}

		llvm::Value* codegen() override;
	};

	/// prototype_ast - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes), as well as if it is an operator.
	class prototype_ast
	{
		std::string name_;
		std::vector<std::string> args_;

		bool					 is_operator_;
		int						 precedence_; // Precedence if a binary op.

	public:
		prototype_ast(std::string name, std::vector<std::string> args,
			bool is_operator = false, int precendence = 0)
			: name_(std::move(name)),
			  args_(std::move(args)),
			  is_operator_(is_operator),
			  precedence_(precendence) {}

		llvm::Function* codegen();

		[[nodiscard]] const std::string& get_name() const noexcept { return name_; }

		[[nodiscard]] bool				 is_unary() const noexcept { return is_operator_ && args_.size() == 1; }
		[[nodiscard]] bool				 is_binary() const noexcept { return is_operator_ && args_.size() == 2; }

		[[nodiscard]] char				 get_operator_name() const noexcept { return name_.back(); }

		[[nodiscard]] int get_precedence() const noexcept { return precedence_; }
	};

	/// function_ast - This class represents a function definition itself.
	class function_ast
	{
		std::unique_ptr<prototype_ast> proto_;
		std::unique_ptr<expr_ast> body_;

	public:
		function_ast(std::unique_ptr<prototype_ast> proto,
		             std::unique_ptr<expr_ast> body)
			: proto_(std::move(proto)),
			  body_(std::move(body)) {}

		llvm::Function* codegen();
	};
}// namespace hello_llvm

#endif//HELLO_LLVM_AST_HPP
