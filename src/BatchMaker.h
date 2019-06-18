#ifndef TAS_BATCHMAKER_H
#define TAS_BATCHMAKER_H

#include <string>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

namespace tas {

class TASForLoop;

class BatchMaker {
  llvm::Function * OldFunc;
  llvm::Function * NewFunc;

  public:
  BatchMaker(llvm::Function * F_) :
    OldFunc(F_), NewFunc(nullptr) {}

  bool run();
  void createBatchedFormFn();
  llvm::SmallVector<llvm::Argument *, 4> getBatchArgs();

}; // tas namespace

}

#endif