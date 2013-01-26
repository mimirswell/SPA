/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/SPA.h>
#include <spa/TargetDistanceUtility.h>

#include <sstream>
#include <klee/Internal/Module/KInstruction.h>


namespace SPA {
	TargetDistanceUtility::TargetDistanceUtility( llvm::Module *module, CFG &cfg, CG &cg, InstructionFilter &filter ) {
		std::set<llvm::Instruction *> worklist;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			if ( filter.checkInstruction( *it ) ) {
				distances[*it] = std::pair<double, bool>( +INFINITY, false );
			} else {
				distances[*it] = std::pair<double, bool>( 0, true );
				propagateChanges( cfg, cg, worklist, *it );
			}
		}

		assert( (! worklist.empty()) && "Filter must exclude something." );
		processWorklist( module, cfg, cg, worklist );
	}

	TargetDistanceUtility::TargetDistanceUtility( llvm::Module *module, CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets ) {
		std::set<llvm::Instruction *> worklist;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			distances[*it] = std::pair<double, bool>( +INFINITY, false );

		for ( std::set<llvm::Instruction *>::iterator it = targets.begin(), ie = targets.end(); it != ie; it++ ) {
			distances[*it] = std::pair<double, bool>( 0, true );
			propagateChanges( cfg, cg, worklist, *it );
		}

		processWorklist( module, cfg, cg, worklist );
	}

	void TargetDistanceUtility::propagateChanges( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &worklist, llvm::Instruction *instruction ) {
		// Propagate changes to predecessors.
		worklist.insert( cfg.getPredecessors( instruction ).begin(), cfg.getPredecessors( instruction ).end() );
		// Check if entry instruction and propagate to callers.
		if ( (! instruction->getParent()->getParent()->empty()) && instruction == &(instruction->getParent()->getParent()->getEntryBlock().front()) )
			worklist.insert( cg.getPossibleCallers( instruction->getParent()->getParent() ).begin(), cg.getPossibleCallers( instruction->getParent()->getParent() ).end() );
	}

	void TargetDistanceUtility::processWorklist( llvm::Module *module, CFG &cfg, CG &cg, std::set<llvm::Instruction *> &worklist ) {
		std::set<llvm::Function *> pathFunctions;

		CLOUD9_DEBUG( "      Processing direct paths." );
		while ( ! worklist.empty() ) {
			std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
			llvm::Instruction *inst = *it;
			worklist.erase( it );

			bool updated = false;

			// Check cost of successors.
			for ( it = cfg.getSuccessors( inst ).begin(), ie = cfg.getSuccessors( inst ).end(); it != ie; it++ ) {
				double d = cfg.getSuccessors( inst ).size() > 1 ? distances[*it].first + 1 : distances[*it].first;
				if ( distances[inst].first > d ) {
					distances[inst] = std::pair<double, bool>( d, true );
					updated = true;
				}
			}
			// Check cost of callees.
			for ( std::set<llvm::Function *>::const_iterator it = cg.getPossibleCallees( inst ).begin(), ie = cg.getPossibleCallees( inst ).end(); it != ie; it++ ) {
				if ( (! (*it)->empty()) && distances[inst].first > distances[&(*it)->getEntryBlock().front()].first ) {
					distances[inst] = std::pair<double, bool>( distances[&(*it)->getEntryBlock().front()].first, true );
					updated = true;
				}
			}
			// Mark function as on direct path.
			pathFunctions.insert( inst->getParent()->getParent() );

			if ( updated )
				propagateChanges( cfg, cg, worklist, inst );
		}

		// Instructions in direct path functions but not on direct path are non-reaching.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			if ( pathFunctions.count( (*it)->getParent()->getParent() ) && ! distances.count( *it ) )
				distances[*it] = std::pair<double, bool>( +INFINITY, true );

		CLOUD9_DEBUG( "      Processing indirect paths." );
		std::set<llvm::Function *> spaReturnFunctions;
		Function *fn = module->getFunction( SPA_RETURN_ANNOTATION_FUNCTION );
		if ( fn ) {
			std::set<llvm::Instruction *> spaReturnSuccessors = cg.getDefiniteCallers( fn );
			for ( std::set<llvm::Instruction *>::iterator it = spaReturnSuccessors.begin(), ie = spaReturnSuccessors.end(); it != ie; it++ )
				worklist.insert( cfg.getPredecessors( *it ).begin(), cfg.getPredecessors( *it ).end() );
			while ( ! spaReturnSuccessors.empty() ) {
				llvm::Instruction *inst = *spaReturnSuccessors.begin();
				spaReturnSuccessors.erase( spaReturnSuccessors.begin() );
				distances[inst] = std::pair<double, bool>( 0, false );
				spaReturnSuccessors.insert( cfg.getSuccessors( inst ).begin(), cfg.getSuccessors( inst ).end() );
				if ( ! spaReturnFunctions.count( inst->getParent()->getParent() ) ) {
					CLOUD9_DEBUG( "      Found spa_return in function: " << inst->getParent()->getParent()->getName().str() << "." );
					spaReturnFunctions.insert( inst->getParent()->getParent() );
				}
			}
		} else {
			CLOUD9_DEBUG( "      Found no spa_returns." );
		}
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			// Initialize costs of return instructions in functions without spa_returns.
			if ( ! distances[*it].second && cfg.getSuccessors( *it ).empty() && ! spaReturnFunctions.count( (*it)->getParent()->getParent() ) ) {
				distances[*it] = std::pair<double, bool>( 0, false );
				worklist.insert( cfg.getPredecessors( *it ).begin(), cfg.getPredecessors( *it ).end() );
			}
		}
		while ( ! worklist.empty() ) {
			std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
			llvm::Instruction *inst = *it;
			worklist.erase( it );

			if ( distances[inst].second )
				continue;

			// Check cost of successors.
			bool updated = false;
			for ( it = cfg.getSuccessors( inst ).begin(), ie = cfg.getSuccessors( inst ).end(); it != ie; it++ ) {
				double d = cfg.getSuccessors( inst ).size() > 1 ? distances[*it].first + 1 : distances[*it].first;
				if ( distances[inst].first > d ) {
					distances[inst] = std::pair<double, bool>( d, false );
					updated = true;
				}
			}
			if ( updated )
				// Propagate changes to predecessors.
				worklist.insert( cfg.getPredecessors( inst ).begin(), cfg.getPredecessors( inst ).end() );
		}
	}

	double TargetDistanceUtility::getUtility( klee::ExecutionState *state ) {
		assert( state );

		double result = - getDistance( state->pc()->inst );
		if ( isFinal( state->pc()->inst ) )
			return result > -INFINITY ? result : UTILITY_PROCESS_LAST;

		for ( klee::ExecutionState::stack_ty::const_reverse_iterator it = state->stack().rbegin(), ie = state->stack().rend(); it != ie && it->caller; it++ ) {
			result -= getDistance( it->caller->inst );
			if ( isFinal( it->caller->inst ) )
				return result > -INFINITY ? result : UTILITY_PROCESS_LAST;
		}
		// No final state was found.
		return UTILITY_PROCESS_LAST;
	}

	std::string TargetDistanceUtility::getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction ) {
		double minPartial = +INFINITY, maxPartial = -INFINITY,
			minFinal = +INFINITY, maxFinal = -INFINITY;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			if ( getDistance( *it ) == INFINITY )
				continue;
			if ( isFinal( *it ) ) {
				minFinal = std::min( minFinal, getDistance( *it ) );
				maxFinal = std::max( maxFinal, getDistance( *it ) );
			} else  if ( (*it)->getParent()->getParent() == instruction->getParent()->getParent() ) {
				minPartial = std::min( minPartial, getDistance( *it ) );
				maxPartial = std::max( maxPartial, getDistance( *it ) );
			}
		}

		std::stringstream result;
		if ( isFinal( instruction ) ) {
			// Min = White, Max = Green
			if ( minFinal < maxFinal && getDistance( instruction ) != INFINITY )
				result << "0.33+" << ((maxFinal - getDistance( instruction )) / (maxFinal - minFinal)) << "+1.0";
			else
				result << "0.33+0.0+1.0";
		} else {
			// Min = White, Max = Blue
			if ( minPartial < maxPartial && getDistance( instruction ) != INFINITY )
				result << "0.67+" << ((maxPartial - getDistance( instruction )) / (maxPartial - minPartial)) << "+1.0";
			else
				result << "0.67+0.0+1.0";
		}
		return result.str();
	}
}
