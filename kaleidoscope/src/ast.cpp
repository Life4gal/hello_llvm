#include <kaleidoscope/ast.hpp>

#include <llvm-12/llvm/IR/Function.h>
#include <llvm-12/llvm/IR/IRBuilder.h>
#include <llvm-12/llvm/IR/LLVMContext.h>
#include <llvm-12/llvm/IR/Module.h>
#include <llvm-12/llvm/IR/LegacyPassManager.h>
#include <kaleidoscope/details/KaleidoscopeJIT.hpp>

#include <llvm-12/llvm/IR/BasicBlock.h>
#include <llvm-12/llvm/IR/Constants.h>
#include <llvm-12/llvm/IR/Verifier.h>
#include <llvm-12/llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm-12/llvm/Transforms/Scalar.h>
#include <llvm-12/llvm/Transforms/Scalar/GVN.h>

#include <iostream>

namespace hello_llvm
{
	global_context::global_context()
		: exit_on_error("Fatal Error", -1),
		  jit(exit_on_error(llvm::orc::KaleidoscopeJIT::Create())) { new_module_and_context(); }

	void global_context::new_module_and_context()
	{
		// Open a new context and module.
		context = std::make_unique<llvm::LLVMContext>();
		module	 = std::make_unique<llvm::Module>("my cool jit", *context);
		module->setDataLayout(jit->getDataLayout());

		// Create a new builder for the module.
		builder = std::make_unique<llvm::IRBuilder<>>(*context);

		// Create a new pass manager attached to it.
		fpm	 = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());

		// Do simple "peephole" optimizations and bit-twiddling options.
		fpm->add(llvm::createInstructionCombiningPass());
		// Re-associate expressions.
		fpm->add(llvm::createReassociatePass());
		// Eliminate Common SubExpressions.
		fpm->add(llvm::createGVNPass());
		// Simplify the control flow graph (deleting unreachable blocks, etc).
		fpm->add(llvm::createCFGSimplificationPass());

		fpm->doInitialization();
	}

	std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>> global_context::refresh()
	{
		auto& self = get();

		auto  ret	 = std::make_pair(std::move(self.module), std::move(self.context));

		self.new_module_and_context();

		return ret;
	}

	global_context& global_context::get()
	{
		static global_context context{};
		return context;
	}

	llvm::Function* global_context::get_function(const std::string& name)
	{
		const auto& self = get();
		// First, see if the function has already been added to the current module.
		if (auto* func = self.module->getFunction(name); func) { return func; }

		// If not, check whether we can codegen the declaration from some existing
		// prototype.
		if (const auto it = self.functions_proto.find(name); it != self.functions_proto.end()) { return it->second->codegen(); }

		// If no existing prototype exists, return nullptr.
		return nullptr;
	}

	std::pair<decltype(global_context::functions_proto)::iterator, bool> global_context::insert_or_assign(std::unique_ptr<prototype_ast> ast)
	{
		// oops, here is a undefined behavior :(
		return get().functions_proto.insert_or_assign(ast->get_name(), std::move(ast));
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

	// out-of-line virtual method
	expr_ast::~expr_ast() = default;

	llvm::Value* number_expr_ast::codegen() { return llvm::ConstantFP::get(*global_context::get().context, llvm::APFloat(val_)); }

	llvm::Value* variable_expr_ast::codegen()
	{
		// Look this variable up in the function.
		auto* v = global_context::get().named_values[name_];
		if (!v) { return log_error_v("unknown variable name"); }
		return v;
	}

	llvm::Value* binary_expr_ast::codegen()
	{
		auto* l = lhs_->codegen();
		auto* r = rhs_->codegen();
		if (!l || !r) { return nullptr; }

		switch (op_)
		{
			case '+': return global_context::get().builder->CreateFAdd(l, r, "add_tmp");
			case '-': return global_context::get().builder->CreateFSub(l, r, "sub_tmp");
			case '*': return global_context::get().builder->CreateFMul(l, r, "mul_tmp");
			case '<': l = global_context::get().builder->CreateFCmpULT(l, r, "cmp_tmp");
				// Convert bool 0/1 to double 0.0 or 1.0
				return global_context::get().builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*global_context::get().context), "bool_tmp");
			default: return log_error_v("invalid binary operator");
		}
	}

	llvm::Value* call_expr_ast::codegen()
	{
		// Look up the name in the global module table.
		auto* callee_func = global_context::get().get_function(callee_);
		if (!callee_func) { return log_error_v("unknown function referenced"); }

		// if argument mismatch error
		if (callee_func->arg_size() != args_.size()) { return log_error_v("incorrect arguments passed"); }

		std::vector<llvm::Value*> vec;
		for (const auto& arg: args_)
		{
			vec.push_back(arg->codegen());
			if (!vec.back()) { return nullptr; }
		}

		return global_context::get().builder->CreateCall(callee_func, vec, "call_tmp");
	}

	llvm::Function* prototype_ast::codegen()
	{
		// Make the function type:  double(double,double) etc.
		const std::vector doubles(args_.size(), llvm::Type::getDoubleTy(*global_context::get().context));

		auto* func_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(*global_context::get().context), doubles, false);

		auto* func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name_, global_context::get().module.get());

		// Set names for all arguments.
		decltype(args_.size()) index = 0;
		for (auto& arg: func->args()) { arg.setName(args_[index++]); }

		return func;
	}

	llvm::Function* function_ast::codegen()
	{
		auto&		context = global_context::get();
		// Transfer ownership of the prototype to the Functions Proto map, but keep a
		// reference to it for use below.
		const auto& p = *proto_;
		global_context::insert_or_assign(std::move(proto_));

		auto* func = global_context::get_function(p.get_name());
		if (!func) { return nullptr; }

		// Create a new basic block to start insertion into.
		auto* bb = llvm::BasicBlock::Create(*global_context::get().context, "entry", func);
		context.builder->SetInsertPoint(bb);

		// Record the function arguments in the named_values map.
		context.named_values.clear();
		for (auto& arg: func->args()) { context.named_values[std::string{arg.getName()}] = &arg; }

		if (auto* ret = body_->codegen(); ret)
		{
			// Finish off the function.
			context.builder->CreateRet(ret);

			// Validate the generated code, checking for consistency.
			verifyFunction(*func);

			// Run the optimizer on the function.
			context.fpm->run(*func);

			return func;
		}

		// Error reading body, remove function.
		func->eraseFromParent();
		return nullptr;
	}
}// namespace hello_llvm
