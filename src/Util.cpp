#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Debug.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include "Util.h"

using namespace llvm;

namespace tas {

Intrinsic::ID VarAnnotationId = static_cast<Intrinsic::ID>(177);

void setAnnotationInFunctionObject(Module * M) {
  if (auto annotationList = M->getNamedGlobal("llvm.global.annotations")) {
    auto ca = cast<ConstantArray>(annotationList->getOperand(0));
    for (unsigned int i = 0; i < ca->getNumOperands(); ++i) {
      auto ca_struct =cast<ConstantStruct>(ca->getOperand(i));
      if (auto ca_func = dyn_cast<Function>(ca_struct->getOperand(0)->getOperand(0))) {
        auto ca_annotation = cast<ConstantDataArray>(
            cast<GlobalVariable>(ca_struct->getOperand(1)->getOperand(0))->getOperand(0));

        ca_func->addFnAttr(ca_annotation->getAsCString());
      }
    }
  }
}

  void detectVarAnnotation(Function * F) {
  SmallVector<Instruction *, 8> ExpPtrUseList;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    if (auto * CI = dyn_cast<CallInst>(&*I)) {
      if (CI->getCalledFunction()->getIntrinsicID() == VarAnnotationId) {
        LLVM_DEBUG(dbgs() << "Use of annotated ptr in load instruction\n");
        auto * ExprPtr = (cast<BitCastInst>(CI->getOperand(0)))->getOperand(0);
        for (User * U : ExprPtr->users()) {
          if (auto * Inst = dyn_cast<LoadInst>(U)) {
            LLVM_DEBUG(dbgs() << *Inst << "\n");
            ExpPtrUseList.push_back(Inst);
          }
        }
      }
    }
  }
}

void cloneLoopBasicBlocks(Function * F, Loop * L, ValueToValueMapTy & VMap) {
  SmallVector<BasicBlock *, 16> ClonedBlocks;
  for (auto * BB : L->blocks()) {
    auto * ClonedBlock = CloneBasicBlock(BB, VMap, "clone_");
    VMap[BB] = ClonedBlock;
    ClonedBlocks.push_back(ClonedBlock);

    auto ExitBlock = L->getExitBlock();
    assert(ExitBlock != nullptr && "Support only loop with single exit edge");

    ClonedBlock->insertInto(F, ExitBlock);
  }
  remapInstructionsInBlocks(ClonedBlocks, VMap);

  // Set cloned loop as successor of old loop
  auto LoopExitingBlock = L->getExitingBlock();
  assert(LoopExitingBlock != nullptr && "Support only loop with single exit edge");
  auto LoopTerminator = LoopExitingBlock->getTerminator();
  assert(LoopTerminator != nullptr && "Loop Basicblock is not well formed");
  LoopTerminator->setSuccessor(1, ClonedBlocks.front());
}
}
