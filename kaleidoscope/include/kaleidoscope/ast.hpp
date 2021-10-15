#ifndef HELLO_LLVM_AST_HPP
#define HELLO_LLVM_AST_HPP

#include <llvm-12/llvm/IR/DerivedTypes.h>
#include <llvm-12/llvm/IR/Function.h>
#include <llvm-12/llvm/IR/IRBuilder.h>
#include <llvm-12/llvm/IR/LLVMContext.h>
#include <llvm-12/llvm/IR/Module.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hello_llvm
{
	////===----------------------------------------------------------------------===//
	//// Top-Level parsing and JIT Driver
	////===----------------------------------------------------------------------===//
	struct global_context
	{
		std::unique_ptr<llvm::LLVMContext>	 context;
		std::unique_ptr<llvm::Module>		 module;
		std::unique_ptr<llvm::IRBuilder<>>	 builder;
		std::map<std::string, llvm::Value *> named_values;

		static global_context& get();

	private:
		global_context();
	};

	//===----------------------------------------------------------------------===//
	// Abstract Syntax Tree (aka Parse Tree) and Code Generation
	//===----------------------------------------------------------------------===//

	/// expr_ast - Base class for all expression nodes.
	class expr_ast
	{
	public:
		virtual ~expr_ast()			   = default;

		virtual llvm::Value *codegen() = 0;
	};

	/// number_expr_ast - Expression class for numeric literals like "1.0".
	class number_expr_ast : public expr_ast
	{
		double val;

	public:
		number_expr_ast(double val) : val(val) {}

		llvm::Value *codegen() override;
	};

	/// variable_expr_ast - Expression class for referencing a variable, like "a".
	class variable_expr_ast : public expr_ast
	{
		std::string name;

	public:
		variable_expr_ast(const std::string &name) : name(name) {}

		llvm::Value *codegen() override;
	};

	/// binary_expr_ast - Expression class for a binary operator.
	class binary_expr_ast : public expr_ast
	{
		char					  op;
		std::unique_ptr<expr_ast> lhs, rhs;

	public:
		binary_expr_ast(char op, std::unique_ptr<expr_ast> lhs, std::unique_ptr<expr_ast> rhs)
			: op(op),
			  lhs(std::move(lhs)),
			  rhs(std::move(rhs)) {}

		llvm::Value *codegen() override;
	};

	/// call_expr_ast - Expression class for function calls.
	class call_expr_ast : public expr_ast
	{
		std::string							   callee;
		std::vector<std::unique_ptr<expr_ast>> args;

	public:
		call_expr_ast(const std::string						&callee,
					  std::vector<std::unique_ptr<expr_ast>> args)
			: callee(callee),
			  args(std::move(args)) {}

		llvm::Value *codegen() override;
	};

	/// prototype_ast - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class prototype_ast
	{
		std::string				 name;
		std::vector<std::string> args;

	public:
		prototype_ast(const std::string &name, std::vector<std::string> args)
			: name(name),
			  args(std::move(args)) {}

		llvm::Function	   *codegen();
		const std::string &get_name() const { return name; }
	};

	/// function_ast - This class represents a function definition itself.
	class function_ast
	{
		std::unique_ptr<prototype_ast> proto;
		std::unique_ptr<expr_ast>	   body;

	public:
		function_ast(std::unique_ptr<prototype_ast> proto,
					 std::unique_ptr<expr_ast>		body)
			: proto(std::move(proto)),
			  body(std::move(body)) {}

		llvm::Function *codegen();
	};

	std::unique_ptr<expr_ast>	   log_error(const char *str);
	std::unique_ptr<prototype_ast> log_error_p(const char *str);
	llvm::Value					*log_error_v(const char *str);
}// namespace hello_llvm

#endif//HELLO_LLVM_AST_HPP
