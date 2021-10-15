#include <kaleidoscope/parser.hpp>

#include <iostream>

///// top ::= definition | external | expression | ';'
void main_loop(hello_llvm::parser& parser)
{
	while (true)
	{
		std::cerr << "ready> ";
		parser.get_next_token();
		switch (parser.get_curr_token())
		{
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
	hello_llvm::parser		   parser{};

	// Install standard binary operators.
	// 1 is the lowest precedence.
	parser.add_bin_op_precedence('<', 10);
	parser.add_bin_op_precedence('+', 20);
	parser.add_bin_op_precedence('-', 20);
	parser.add_bin_op_precedence('*', 40);// highest.

	// Run the main "interpreter loop" now.
	main_loop(parser);

	// Print out all the generated code.
	hello_llvm::global_context::get().module->print(llvm::errs(), nullptr);

	return 0;
}
