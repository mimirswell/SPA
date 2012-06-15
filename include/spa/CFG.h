/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CFG_H__
#define __CFG_H__

#include <map>
#include <set>
#include <ostream>

#include "llvm/Module.h"
#include "llvm/Instruction.h"

#include "spa/InstructionFilter.h"

namespace SPA {
	class CFG {
	private:
		std::set<llvm::Instruction *> instructions;
		std::map<llvm::Function *,std::vector<llvm::Instruction *> > functionInstructions;
		std::map<llvm::Instruction *,std::set<llvm::Instruction *> > predecessors, successors;

	public:
		typedef std::set<llvm::Instruction *>::iterator iterator;

		CFG( llvm::Module *module );
		// Iterates over all instructions in an arbitrary order.
		iterator begin();
		iterator end();
		// Gets the instructions of a function.
		const std::vector<llvm::Instruction *> &getInstructions( llvm::Function *fn );
		// Gets CFG data.
		const std::set<llvm::Instruction *> &getSuccessors( llvm::Instruction *instruction );
		const std::set<llvm::Instruction *> &getPredecessors( llvm::Instruction *instruction );
		// Dumps CFG as a GraphViz DOT-file.
		void dump( std::ostream &dotFile, InstructionFilter *filter, std::map<InstructionFilter *, std::string> &annotations );
	};
}

#endif // #ifndef __CFG_H__
