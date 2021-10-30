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
		module = std::make_unique<llvm::Module>("my cool jit", *context);
		module->setDataLayout(jit->getDataLayout());

		// Create a new builder for the module.
		builder = std::make_unique<llvm::IRBuilder<>>(*context);

		// Create a new pass manager attached to it.
		fpm = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());

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

		auto ret = std::make_pair(std::move(self.module), std::move(self.context));

		self.new_module_and_context();

		return ret;
	}

	global_context& global_context::get()
	{
		static global_context context{};
		return context;
	}

	int global_context::get_token_precedence(int tok)
	{
		// todo: is-ascii was deprecated
		if (!isascii(tok)) { return -1; }

		// Make sure it's a declared bin_op
		const int tok_prec = get().bin_op_precedence_[static_cast<char>(tok)];
		if (tok_prec <= 0) { return -1; }
		return tok_prec;
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

	std::pair<decltype(global_context::functions_proto)::iterator, bool> global_context::insert_or_assign_function(std::unique_ptr<prototype_ast> ast)
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

	llvm::Value* unary_expr_ast::codegen()
	{
		auto* operand = operand_->codegen();
		if (!operand)
		{
			return nullptr;
		}

		auto* func = global_context::get().get_function(std::string{"unary"} + op_);
		if (!func)
		{
			return log_error_v("unknown unary operator");
		}

		return global_context::get().builder->CreateCall(func, operand, "unary_op");
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
			default: break;
		}

		// If it wasn't a builtin binary operator, it must be a user defined one. Emit
		// a call to it.
		auto* func = global_context::get().get_function(std::string{"binary"} + op_);
		if (!func)
		{
			return log_error_v("unknown binary operator");
		}

		llvm::Value* ops[]{l, r};
		return global_context::get().builder->CreateCall(func, ops, "binary_op");
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
			auto* v = arg->codegen();
			if (!v)
			{
				return nullptr;
			}
			vec.push_back(v);
		}

		return global_context::get().builder->CreateCall(callee_func, vec, "call_tmp");
	}

	llvm::Value* if_expr_ast::codegen()
	{
		auto& context = global_context::get();

		auto* cond_val = cond_->codegen();
		if (!cond_val) { return nullptr; }

		// Convert condition to a bool by comparing non-equal to 0.0.
		cond_val = context.builder->CreateFCmpONE(cond_val, llvm::ConstantFP::get(*context.context, llvm::APFloat(0.0)), "if_cond");

		auto* func = context.builder->GetInsertBlock()->getParent();

		// Create blocks for the then and else cases.  Insert the 'then' block at the
		// end of the function.
		auto* then_bb = llvm::BasicBlock::Create(*context.context, "then", func);
		auto* else_bb = llvm::BasicBlock::Create(*context.context, "else");
		auto* merge_bb = llvm::BasicBlock::Create(*context.context, "if_count");

		context.builder->CreateCondBr(cond_val, then_bb, else_bb);

		// Emit then value.
		context.builder->SetInsertPoint(then_bb);

		auto* then_val = then_->codegen();
		if (!then_val) { return nullptr; }

		context.builder->CreateBr(merge_bb);
		// Codegen of 'then_' can change the current block, update then_bb for the PHI.
		then_bb = context.builder->GetInsertBlock();

		// Emit else block.
		func->getBasicBlockList().push_back(else_bb);
		context.builder->SetInsertPoint(else_bb);

		auto* else_val = else_->codegen();
		if (!else_val) { return nullptr; }

		context.builder->CreateBr(merge_bb);
		// Codegen of 'else_' can change the current block, update else_bb for the PHI.
		else_bb = context.builder->GetInsertBlock();

		// Emit merge block.
		func->getBasicBlockList().push_back(merge_bb);
		context.builder->SetInsertPoint(merge_bb);
		auto* pn = context.builder->CreatePHI(llvm::Type::getDoubleTy(*context.context), 2, "if_tmp");

		pn->addIncoming(then_val, then_bb);
		pn->addIncoming(else_val, else_bb);
		return pn;
	}

	llvm::Value* for_expr_ast::codegen()
	{
		// Output for-loop as:
		//   ...
		//   init = init-expr
		//   goto loop
		// loop:
		//   variable = phi [init, loop-header], [next-variable, loop-end]
		//   ...
		//   body-expr
		//   ...
		// loop-end:
		//   step = step-expr
		//   next-variable = variable + step
		//   end-cond = end-expr
		//   br end-cond, loop, end-loop
		// out-loop:

		auto& context = global_context::get();

		// Emit the init code first, without 'variable' in scope.
		auto* cond_val = init_->codegen();
		if (!cond_val) { return nullptr; }

		// Make the new basic block for the loop header, inserting after current block
		auto* func = context.builder->GetInsertBlock()->getParent();
		auto* ph_bb = context.builder->GetInsertBlock();
		auto* loop_bb = llvm::BasicBlock::Create(*context.context, "loop", func);

		// Insert an explicit fall through from the current block to the loop_bb
		context.builder->CreateBr(loop_bb);

		// Start insertion in loop_bb
		context.builder->SetInsertPoint(loop_bb);

		// Start the PHI node with an entry for init
		auto* var = context.builder->CreatePHI(llvm::Type::getDoubleTy(*context.context), 2, cond_name_);
		var->addIncoming(cond_val, ph_bb);

		// Within the loop, the variable is defined equal to the PHI node.  If it
		// shadows an existing variable, we have to restore it, so save it now.
		auto* old_val = std::exchange(context.named_values[cond_name_], var);

		// Emit the body of the loop.  This, like any other expr, can change the
		// current BB.  Note that we ignore the value computed by the body, but don't
		// allow an error.
		if (!body_->codegen()) { return nullptr; }

		// Emit the step value.
		llvm::Value* step_val;
		if (step_)
		{
			step_val = step_->codegen();
			if (!step_val) { return nullptr; }
		}
		else
		{
			// If not specified, use 1.0
			step_val = llvm::ConstantFP::get(*context.context, llvm::APFloat(1.0));
		}

		auto* next_val = context.builder->CreateFAdd(var, step_val, "next_val");

		// Compute the end condition
		auto* end_cond = end_->codegen();
		if (!end_cond) { return nullptr; }

		// Convert condition to a bool by comparing non-equal to 0.0.
		end_cond = context.builder->CreateFCmpONE(end_cond, llvm::ConstantFP::get(*context.context, llvm::APFloat(0.0)), "loop_cond");

		// Create the "after loop" block and insert it.
		auto* loop_end_bb = context.builder->GetInsertBlock();
		auto* after_bb = llvm::BasicBlock::Create(*context.context, "after_loop", func);

		// Insert the conditional branch into the end of loop_end_bb
		context.builder->CreateCondBr(end_cond, loop_bb, after_bb);

		// Any new code will be inserted in after_bb
		context.builder->SetInsertPoint(after_bb);

		// Add a new entry to the PHI node for the back-edge.
		var->addIncoming(next_val, loop_end_bb);

		// Restore the un-shadowed variable.
		std::exchange(context.named_values[cond_name_], old_val);

		// for expr always returns 0.0.
		return llvm::ConstantFP::getNullValue(llvm::Type::getDoubleTy(*context.context));
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
		auto& context = global_context::get();
		// Transfer ownership of the prototype to the Functions Proto map, but keep a
		// reference to it for use below.
		const auto& p = *proto_;
		global_context::insert_or_assign_function(std::move(proto_));

		auto* func = global_context::get_function(p.get_name());
		if (!func) { return nullptr; }

		// If this is an operator, install it.
		if (p.is_binary())
		{
			global_context::add_bin_op_precedence(p.get_operator_name(), p.get_precedence());
		}

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

		if (p.is_binary())
		{
			global_context::erase_bin_op(p.get_operator_name());
		}

		return nullptr;
	}
}// namespace hello_llvm
