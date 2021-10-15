#include <kaleidoscope/ast.hpp>

#include <llvm-12/llvm/IR/BasicBlock.h>
#include <llvm-12/llvm/IR/Constants.h>
#include <llvm-12/llvm/IR/Verifier.h>

#include <iostream>

namespace hello_llvm
{
	global_context::global_context()
		: context(std::make_unique<llvm::LLVMContext>()),
		  module(std::make_unique<llvm::Module>("my cool jit", *context)),
		  builder(std::make_unique<llvm::IRBuilder<>>(*context)) {}

	global_context& global_context::get()
	{
		static global_context context;
		return context;
	}

	std::unique_ptr<expr_ast> log_error(const char* str)
	{
		std::cerr << "Error: " << str << '\n';
		return nullptr;
	}

	std::unique_ptr<prototype_ast> log_error_p(const char* str)
	{
		log_error(str);
		return nullptr;
	}

	llvm::Value* log_error_v(const char* str)
	{
		log_error(str);
		return nullptr;
	}

	llvm::Value* number_expr_ast::codegen()
	{
		return llvm::ConstantFP::get(*global_context::get().context, llvm::APFloat(val));
	}

	llvm::Value* variable_expr_ast::codegen()
	{
		// Look this variable up in the function.
		auto* v = global_context::get().named_values[name];
		if (!v)
		{
			return log_error_v("unknown variable name");
		}
		return v;
	}

	llvm::Value* binary_expr_ast::codegen()
	{
		auto* l = lhs->codegen();
		auto* r = rhs->codegen();
		if (!l || !r)
		{
			return nullptr;
		}

		switch (op)
		{
			case '+':
				return global_context::get().builder->CreateFAdd(l, r, "add_tmp");
			case '-':
				return global_context::get().builder->CreateFSub(l, r, "sub_tmp");
			case '*':
				return global_context::get().builder->CreateFMul(l, r, "mul_tmp");
			case '<':
				l = global_context::get().builder->CreateFCmpULT(l, r, "cmp_tmp");
				// Convert bool 0/1 to double 0.0 or 1.0
				return global_context::get().builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*global_context::get().context), "bool_tmp");
			default:
				return log_error_v("invalid binary operator");
		}
	}

	llvm::Value* call_expr_ast::codegen()
	{
		// Look up the name in the global module table.
		auto* callee_func = global_context::get().module->getFunction(callee);
		if (!callee_func)
		{
			return log_error_v("unknown function referenced");
		}

		// if argument mismatch error
		if (callee_func->arg_size() != args.size())
		{
			return log_error_v("incorrect arguments passed");
		}

		std::vector<llvm::Value*> vec;
		for (decltype(args.size()) i = 0; i < args.size(); ++i)
		{
			vec.push_back(args[i]->codegen());
			if (!vec.back())
			{
				return nullptr;
			}
		}

		return global_context::get().builder->CreateCall(callee_func, vec, "call_tmp");
	}

	llvm::Function* prototype_ast::codegen()
	{
		// Make the function type:  double(double,double) etc.
		std::vector<llvm::Type*> doubles(args.size(), llvm::Type::getDoubleTy(*global_context::get().context));

		auto*					 func_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(*global_context::get().context), doubles, false);

		auto*					 func	   = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, global_context::get().module.get());

		// Set names for all arguments.
		decltype(args.size())	 index	   = 0;
		for (auto& arg: func->args())
		{
			arg.setName(args[index++]);
		}

		return func;
	}

	llvm::Function* function_ast::codegen()
	{
		// First, check for an existing function from a previous 'extern' declaration.
		auto* func = global_context::get().module->getFunction(proto->get_name());

		if (!func)
		{
			func = proto->codegen();
		}

		if (!func)
		{
			return nullptr;
		}

		// Create a new basic block to start insertion into.
		auto* bb = llvm::BasicBlock::Create(*global_context::get().context, "entry", func);
		global_context::get().builder->SetInsertPoint(bb);

		// Record the function arguments in the named_values map.
		global_context::get().named_values.clear();
		for (auto& arg: func->args())
		{
			global_context::get().named_values[std::string{arg.getName()}] = &arg;
		}

		if (auto* ret = body->codegen(); ret)
		{
			// Finish off the function.
			global_context::get().builder->CreateRet(ret);

			// Validate the generated code, checking for consistency.
			llvm::verifyFunction(*func);

			return func;
		}

		// Error reading body, remove function.
		func->eraseFromParent();
		return nullptr;
	}
}// namespace hello_llvm
