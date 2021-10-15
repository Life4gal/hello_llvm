#ifndef HELLO_LLVM_LEXER_HPP
#define HELLO_LLVM_LEXER_HPP

#include <string>

namespace hello_llvm
{
	//===----------------------------------------------------------------------===//
	// Lexer
	//===----------------------------------------------------------------------===//

	struct tokenizer
	{
		// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
		// of these for known things.
		enum token
		{
			tok_eof		   = -1,

			// commands
			tok_def		   = -2,
			tok_extern	   = -3,

			// primary
			tok_identifier = -4,
			tok_number	   = -5
		};

		std::string identifier_str;// Filled in if tok_identifier
		double		num_val{};	   // Filled in if tok_number

		int			get_token();
	};
}// namespace hello_llvm

#endif//HELLO_LLVM_LEXER_HPP
