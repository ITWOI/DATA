#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include <llvm/Analysis/AliasSetTracker.h>
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "./pugixml/pugixml.hpp"
#include <unordered_set>

using namespace llvm;

//static cl::opt<bool> ifod("ifOD", cl::desc("detect overhead"), cl::value_desc("filename"));

#define DEBUG_TYPE "NoDang"
#define REPORTLEVEL 0
#define ifod 1
#define IFUSESTATIC 1

namespace {
  class NoDang : public FunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    NoDang() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
    bool doInitialization(Module &M) override;
    bool doFinalization(Module &M) override;

  private:
    bool insertPointers(Function &F);
    bool insertNullify(Value *v);
    bool insertPtrForValue(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI);
    bool insertPtrForValueOD(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI);
    bool insertPtrForValueDang(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI);
    bool insertPtrFromAlloc(Value *value, Function &F);
    bool ifHasAlias(Value *value);
    Function* ifUseOutsideFunction(Value *value, Value** modifiedPointer, Value* middlePointer, Value* oriP, Function &F);
    int getLevelOfPointer(Value *value);
    int getLevelOfPointer(Type *type);
    bool checkAndDeleteAlloc(Value *value);
    void getRevTopo(Module &M, CallGraph& callGraph);
    void _DFSTopSort(Function *i, DenseMap<Function*, int>& colors, CallGraph& callGraph);
    bool ifOneLevelPointer(Function *F);
    Value* getAllocValue(Value* val);
    Value* getRootDef(Value* val, bool ifreturnpre = false);
    void output(std::string str);
    bool handleStructEle(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins);
    bool getOtherOper(StoreInst* SI, Value* lastins, Value* oriP, Value** CurOper, Value** OtherOper);
    GetElementPtrInst* detNCreateNewGEPI(Value* targetOper, Value* modifiedPointer);
    void output(Value* value);
    bool ifNotPointer(Value *val);
    void cloneIns(Instruction* ins, Function& F);
    void pointerCount(Value* val);
    void printVal(bool ifAll, Value* val);
    // return value used to determine whether this is take address oper or take memory oper, or alias oper
    //memory is malloced and freed

    AliasAnalysis *AA;
    AliasSetTracker *ASTracker;
    SetVector<Value*> mallocPointer;
    SetVector<CallInst*> freePointer;
    SetVector<Value*> pairedGlobalPointer;
    //store static information
    SetVector<std::string> potentialDP;
    DenseMap<unsigned int, bool> vulLoc;
    //ifModified stores two type of infs, first is whether the alloca pointer is modified, the second is whther some ins need to be modified.
    //second means the modified instruction.
    DenseMap<Value*, Value*> ifModified;
    //This is used for a variable, will be cleared after inserting.
    DenseMap<Value*, Value*> ifInsModified;
    SmallVector<Value*, 8> valueNeedToMod;
    //This is used to store the middle ins, the first is modified ins, the second is middle ins. 
    DenseMap<Value*, Value*> middleIns;
    //This is used to store the function's return value's pointer level.
    DenseMap<Function*, int> funRetPL;
    //Used to record wether the function is modified.
    DenseMap<Function*, bool> modifiedFun;
    //Used for OD, create function specific store ins.
    DenseMap<Function*, Instruction*> funIns;
    //This is used to store reverse Topological  order
    std::vector<Function*> revTopologicalOrder;
    //This is used to record whether an ins can't be deleted
    bool ifUsedAsExpect = false;
    unsigned numOfMod = 0;
    unsigned totalVar = 0;
    unsigned numOfInsMod = 0;
    Module *M;

    //used for print pointers
    Function *Printf;
    GlobalVariable *globalStrAll;
    GlobalVariable *globalStrDD;
    std::unordered_set<Instruction*> printedIns;
    virtual void getAnalysisUsage(AnalysisUsage &AU) const override
    {
        //AU.addRequired<MemoryDependenceAnalysis>();
        AU.addRequired<AliasAnalysis>();
    }
  };
}
