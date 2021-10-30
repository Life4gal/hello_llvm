#include <kaleidoscope/parser.hpp>

#include <llvm-12/llvm/Support/TargetSelect.h>

#include <iostream>

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern 'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
	#define DLLEXPORT __declspec(dllexport)
#else
	#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(const double x)
{
	return std::fputc(static_cast<char>(x), stderr);
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(const double x)
{
	return std::fprintf(stderr, "%.3f\n", x);
}

///// top ::= definition | external | expression | ';'
void main_loop(hello_llvm::parser& parser)
{
	while (true)
	{
		std::cerr << "ready> ";
		switch (parser.get_next_token())
		{
			case '_':
			case hello_llvm::tokenizer::tok_eof:
				return;
			case ';':// ignore top-level semicolons.
				parser.get_next_token();
				break;
			case hello_llvm::tokenizer::tok_def:
				parser.handle_definition();
				break;
			case hello_llvm::tokenizer::tok_extern:
				parser.handle_extern();
				break;
			default:
				parser.handle_top_level_expression();
				break;
		}

	}
}

////===----------------------------------------------------------------------===//
//// Main driver code.
////===----------------------------------------------------------------------===//

int main()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetDisassembler();

	hello_llvm::parser		   parser{};

	// Install standard binary operators.
	// 1 is the lowest precedence.
	hello_llvm::global_context::add_bin_op_precedence('<', 10);
	hello_llvm::global_context::add_bin_op_precedence('+', 20);
	hello_llvm::global_context::add_bin_op_precedence('-', 20);
	hello_llvm::global_context::add_bin_op_precedence('*', 40);// highest.

	// Run the main "interpreter loop" now.
	main_loop(parser);

	// Print out all the generated code.
	// hello_llvm::global_context::get().module->print(llvm::errs(), nullptr);

	return 0;
}
