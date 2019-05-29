#include "BatchProcess.h"
#include "Util.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>

#include <iostream>
#include <string>

using namespace llvm;

static const std::string fn_mark = "tas_batch";

namespace tas {

bool BatchProcess::run() {
/* Pre-condition checks:
 * 1. Function has atleast one loop and loop is in canonical form.
 *
 * Step 1: Detect and store all variables marked with prefetch annotation.
 * Step 2: Store trip count for loop. 
 * Step 3: Find store instructions whose operand is prefetch annotated variables and save them.
 *         These instructions act as loop split boundary.
 * Step 4: Identify variables whose values are defined in one loop and used in other loop. We need to store such variables
 *         in an array of size trip count.
 * Step 5: Replace use of variables in step 4 with indexed array variable.
 * Step 5: Split loop into multiple loops with same trip count.
 * Step 6: Add new loop above first loop, which has prefetch instruction as it's body.
 * Step 7: Handle control flow.
 * Step 8: In each loop insert prefetch instruction for memory access of next loop.
 */

  detectAnnotatedVariableDefs();

  auto * L0 = *LI->begin();
  auto * OldIndexVariable = L0->getCanonicalInductionVariable();
  auto * PN1 = &*(L0->getHeader()->phis().begin());
  PN1->removeIncomingValue(L0->getLoopPreheader());
  unsigned int i = 0;
  // Split basic block at annotated variable def points.
  for (auto & DP : AnnotatedVariableDefPoints) {
    
    // Assume Loop contains single entry edge.
    auto * L0_Head = L0->getHeader();
    BasicBlock * L0_PreHeader;
    for (auto * B : predecessors(L0_Head)) {
      if (!L0->contains(B)) {
        L0_PreHeader = B;
        break;
      }
    }

    auto * TL0 = TASForLoop::Create(F->getContext(), L0_PreHeader, L0_Head, "tas.loop." + std::to_string(i), F);

    auto * ParentBody = DP->getParent();
    auto * NewBody = ParentBody->splitBasicBlock(DP->getNextNode(), "batch_edge_" + std::to_string(i));
    ParentBody->replaceAllUsesWith(NewBody);

    replaceUsesWithinBB(OldIndexVariable, TL0->getIndexVariable(), ParentBody);
    TL0->setLoopBody(ParentBody);
    ++i;
  }

  insertPrefetchCalls();
  //F->print(errs());
 
  return true;
}

void BatchProcess::detectAnnotatedVariableDefs() {
  // XXX Checking only entry basic block for annotated variables.
  for (auto & I : F->front()) {
    if (auto * CI = dyn_cast<CallInst>(&I)) {
      auto * Callee = CI->getCalledFunction();
      if (!Callee->isIntrinsic()) continue;

      AnnotatedVariables.push_back(cast<BitCastInst>(CI->getArgOperand(0))->getOperand(0));
      for (auto * U : AnnotatedVariables.back()->users()) {
        if (auto * ST = dyn_cast<StoreInst>(U))
          AnnotatedVariableDefPoints.push_back(ST);
      }
    }
  }
}

void BatchProcess::insertPrefetchCalls() {
  // Insert Prefetch call.
  for (auto & V : AnnotatedVariables) {
    for (auto * U : V->users()) {
      if (auto * ST = dyn_cast<StoreInst>(U)) {
        insertLLVMPrefetchIntrinsic(F, ST);
      }
    }
  }
}

}

namespace {

void TASBatchProcess::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  //AU.setPreservesAll();
}

bool TASBatchProcess::doInitialization(Module &M) {
  tas::setAnnotationInFunctionObject(&M);
  return true;
}

bool TASBatchProcess::runOnFunction(Function &F) {
  if (!F.hasFnAttribute(fn_mark)) 
    return false;

  LLVM_DEBUG(errs() << "BatchProcess pass: " << F.getName() << "\n");
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  tas::BatchProcess BP(&F, &LI);
  return BP.run();
}

char TASBatchProcess::ID = 0;
static RegisterPass<TASBatchProcess> X("tas-batch-process", "Pass to convert sequential process to batch process of packets",
                                   false,
                                     false);
} // Anonymous namespace
