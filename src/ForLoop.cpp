#include "ForLoop.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>

using namespace llvm;

namespace tas {

TASForLoop::TASForLoop(LLVMContext & Ctx, BasicBlock * Prev,
    BasicBlock * Next, std::string & Name, Function * F)
  : F(F), Name (std::move(Name))
{
  addEmptyLoop(Ctx, Prev, Next);
}

void TASForLoop::addEmptyLoop(LLVMContext & Ctx, BasicBlock * Prev, BasicBlock * Next) {
  PreHeader = BasicBlock::Create(Ctx, Name + ".preheader", F, Next);
  Header = BasicBlock::Create(Ctx, Name + ".header", F, Next);
  Latch = BasicBlock::Create(Ctx, Name + ".latch", F, Next);

  // Update phi node edge if any
  IRBuilder<> Builder(Next);
  auto * PN = &*(Next->phis().begin());
  if (!PN)
    PN->addIncoming(Builder.getInt16(0), Header);

  Builder.SetInsertPoint(PreHeader);
  Builder.CreateBr(Header);

  Builder.SetInsertPoint(Header);
  IndexVar = Builder.CreatePHI(Type::getInt16Ty(Ctx), 2, "indV");

  Builder.SetInsertPoint(Latch);
  auto *IVNext = Builder.CreateAdd(IndexVar, Builder.getInt16(1));
  Builder.CreateBr(Header);

  Builder.SetInsertPoint(Header);
  IndexVar->addIncoming(Builder.getInt16(0), PreHeader);
  IndexVar->addIncoming(IVNext, Latch);
  auto * icmp = Builder.CreateICmpSLT(IndexVar, Builder.getInt16(32), "loop-predicate");
  
  // Stitch entry point in control flow.
  if (Prev->getTerminator()->getNumOperands() == 3) 
    Prev->getTerminator()->setSuccessor(1, PreHeader);
  else
    Prev->getTerminator()->setSuccessor(0, PreHeader);

  /// FIXME If Exit block is not specified, set to latch.
  /// This would be invalid loop, but works for now.
  ExitInst = Builder.CreateCondBr(icmp, Latch, Next);
}

void TASForLoop::setLoopBody(BasicBlock * BodyBB) {
  Header->getTerminator()->setSuccessor(0, BodyBB);
  BodyBB->getTerminator()->setSuccessor(0, Latch);
}

}