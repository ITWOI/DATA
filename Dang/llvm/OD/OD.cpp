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
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include <llvm/Analysis/AliasSetTracker.h>
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "NoDang"
#define REPORTLEVEL 1

namespace {
  class NoDang : public FunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    NoDang() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
    bool doFinalization(Module &M) override;

  private:
    bool insertPointers(Function &F);
    bool insertPtrForValue(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI);
    bool insertPtrFromAlloc(Value *value, Function &F);
    bool ifHasAlias(Value *value);
    int getLevelOfPointer(Value *value);
    int getLevelOfPointer(Type *type);
    bool ifOneLevelPointer(Function *F);
    Value* getAllocValue(Value* val);
    Value* getRootDef(Value* val, bool ifreturnpre = false);
    void output(std::string str);
    bool getOtherOper(StoreInst* SI, Value* lastins, Value* oriP, Value** CurOper, Value** OtherOper);
    GetElementPtrInst* detNCreateNewGEPI(Value* targetOper, Value* modifiedPointer);
    void output(Value* value);
    bool ifNotPointer(Value *val);
    // return value used to determine whether this is take address oper or take memory oper, or alias oper
    //memory is malloced and freed

    AliasAnalysis *AA;
    AliasSetTracker *ASTracker;
    SetVector<Value*> pairedPointer;
    SetVector<Value*> mallocPointer;
    SetVector<Value*> pairedGlobalPointer;
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
    //This is used to store reverse Topological  order
    std::vector<Function*> revTopologicalOrder;
    //This is used to record whether an ins can't be deleted
    bool ifUsedAsExpect = false;
    unsigned numOfMod = 0;
    virtual void getAnalysisUsage(AnalysisUsage &AU) const override
    {
        //AU.addRequired<MemoryDependenceAnalysis>();
        AU.addRequired<AliasAnalysis>();
    }
  };
}

void NoDang::output(std::string str)
{
    if(REPORTLEVEL)
        errs()<<str<<"\n";
}

void NoDang::output(Value *tmp)
{
    if(REPORTLEVEL == 0)
        return;
    if(tmp != nullptr)
        tmp->dump();
    else
        errs()<<"output para is null.\n";
}

bool NoDang::ifOneLevelPointer(Function *F)
{
    if(F->getName() == "malloc")
        return true;
    DenseMap<Function*, int>::iterator it = funRetPL.find(F); 
    if(it != funRetPL.end() &&  it->second != 1)
        return false;
    else if(it == funRetPL.end())
    {
        int levelOfFun = getLevelOfPointer(F->getReturnType());
        if(levelOfFun > 0)
        {
            funRetPL[F] = levelOfFun;
        }
        if(levelOfFun != 1)
            return false;
    }
    return true;
}

bool NoDang::doFinalization(Module &M)
{
    errs()<<"Num of modified var:" << numOfMod << "\n";
    return false;

}
bool NoDang::runOnFunction(Function &F)
{
    if(modifiedFun.find(&F) != modifiedFun.end())
        return false;
    modifiedFun[&F] = true;
    output(&F);
    //CallGraph callGraph = CallGraph(M);
    //Notice: Not used currently
    //getRevTopo(M, callGraph);
    AA = &getAnalysis<AliasAnalysis>();
    ASTracker = new AliasSetTracker(*AA);
    if(F.isDeclaration())
        return false;
    pairedPointer.clear();
    mallocPointer.clear();
    ASTracker->clear();
    errs()<<"------------------Function:";
    errs().write_escaped(F.getName()) << "--------------\n";

    output("Variables to be mod:");
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
    {
        ASTracker->add(*BB);
        for(BasicBlock::iterator bbF=BB->begin(), bbE = BB->end(); bbF != bbE; ++bbF)
        {
            if (isa<CallInst>(&(*bbF)))
            {
                CallInst *ci = dyn_cast<CallInst>(&(*bbF));
                //The function may be indirect call, so use getCalledValue instead of getCalledFunction.
                Function* calledFunction = dyn_cast<Function>(ci->getCalledValue()->stripPointerCasts());
                /*TODO:
                  %call22 = tail call i8* %5(i8* %7, i32 55768, i32 1) #7
                  %5 = phi i8* (i8*, i32, i32)* [ @default_bzalloc, %if.then14 ], [ %4, %if.end9 ]
                  will result in F == null
                 */
                //assert(calledFunction && "F is nullptr, usually F is indirect function invocation");
                if(calledFunction == nullptr)
                    continue;
                if(ifOneLevelPointer(calledFunction))
                {
                    output(ci);
                    mallocPointer.insert(dyn_cast<Value>(ci));
                }
            }
        }
    }
    insertPointers(F);
    output("Modified F");
    output(&F);
    delete ASTracker;
    return true;
}

bool NoDang::ifHasAlias(Value *value)
{
    errs()<<"IfHasAlias\n";
    AliasSet *tmp = ASTracker->getAliasSetForPointerIfExists(value,AA->getTypeStoreSize(value->getType()), AAMDNodes());
    if(tmp)
        return true;
    errs()<<"doesn't has alias";
    return false;
}

Value* NoDang::getRootDef(Value* val, bool ifreturnpre)
{
    Value* tmp = val;
    Value* pre = nullptr;
    if(BitCastInst *BCI = dyn_cast<BitCastInst>(tmp))
    {
        if(BCI->getNumOperands() != 1)
        {
            output(BCI);
            assert(0 && "Exception: Num of operand in BitCastInst is large than 1");
        }
        tmp = BCI->getOperand(0);
    }
    while(!isa<AllocaInst>(tmp) && !isa<GlobalVariable>(tmp)
        && !(isa<ConstantPointerNull>(tmp) || isa<ConstantFP>(tmp) || isa<ConstantInt>(tmp)) && !isa<CallInst>(tmp) && !isa<Argument>(tmp))
    {
        pre = tmp;
        if(LoadInst *LI = dyn_cast<LoadInst>(tmp))
        {
            tmp = LI->getPointerOperand();
        }
        else if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(tmp))
        {
            tmp = GEPI->getPointerOperand();
        }
        else if(Operator* otmp = dyn_cast<Operator>(tmp))
        {
            if(GEPOperator* geto = dyn_cast<GEPOperator>(otmp))
            {
                tmp = geto->getPointerOperand();
            }
            else
            {
                tmp->dump();
                errs()<< "Exception: In getRootDef other Operator ins found.\n";
                return nullptr;
            }
        }
        else
        {
            tmp->dump();
            errs()<< "Exception: In getRootdef other value ins found.\n";
            return nullptr;
        }
        if(BitCastInst *BCI = dyn_cast<BitCastInst>(tmp))
        {
            if(BCI->getNumOperands() != 1)
                assert(0 && "Exception: Num of operand in BitCastInst is large than 1");
            tmp = BCI->getOperand(0);
        }
    }
    if(ifreturnpre)
        return pre;
    else
        return tmp;
}

Value* NoDang::getAllocValue(Value* val)
{
    Value* tmp = val;
    while(!isa<AllocaInst>(tmp) && !isa<GlobalVariable>(tmp))
    {
        output(tmp);
        if(BitCastInst *BCI = dyn_cast<BitCastInst>(tmp))
        {
            if(BCI->getNumOperands() != 1)
                assert(0 && "Exception: Num of operand in BitCastInst is large than 1");
            tmp = BCI->getOperand(0);
        }
        if(LoadInst *LI = dyn_cast<LoadInst>(tmp))
        {
            tmp = LI->getPointerOperand();
        }
        else if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(tmp))
        {
            tmp = GEPI->getPointerOperand();
        }
        else if(Operator* otmp = dyn_cast<Operator>(tmp))
        {
            if(GEPOperator* geto = dyn_cast<GEPOperator>(otmp))
            {
                tmp = geto->getPointerOperand();
            }
            else
            {
                tmp->dump();
                errs()<< "Exception: In alloca other Operator ins found.\n";
                return nullptr;
            }
        }
        else
        {
            tmp->dump();
            errs()<< "Exception: In alloca other value ins found.\n";
            return nullptr;
        }
    }
    return tmp;
}

bool NoDang::insertPointers(Function &F)
{
    output("insert pointer:");
    //bool equals false means the malloc ins havn't been inserted, equals 1 means inserted.
    for(SetVector<Value*>::iterator citb = mallocPointer.begin(), cite = mallocPointer.end(); citb != cite; ++citb)
    {
        if((*citb)->getNumUses() > 1)
        {
            errs()<<"Exception: More than 1 use for malloc, ignored\n";
            (*citb)->dump();
            (*citb)->user_back()->dump();
            for(auto use:(*citb)->users())
                use->dump();
            continue;
        }
        //Usually, %call = call @malloc and only store 'call' to var once.
        if((*citb)->use_empty())
            continue;
        Value* uito = (*citb)->user_back();
        if(BitCastInst *BCI = dyn_cast<BitCastInst>(uito))
        {
            if(BCI->getNumUses() != 1)
            {
                output("Exception: Num of uses in BitCastInst is large than 1");
                continue;
                //assert(0 && "Exception: Num of uses in BitCastInst is large than 1");
            }
            uito = BCI->user_back();
        }
        Value* valueAlloc = nullptr;
        //Find the alloca instruction for the malloced pointer
        if(StoreInst *sitmp = dyn_cast<StoreInst>(uito))
        {
            Value *voptmp = sitmp->getPointerOperand();
            valueAlloc = getAllocValue(voptmp);
        }
        else if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(uito))
        {
            valueAlloc = GEPI;
        }
        if(valueAlloc == nullptr)
        {
            errs()<<"Exception: No alloc for pointer, ignored\n";
            uito->dump();
            continue;
        }
        //ifInsModified.clear();
        valueNeedToMod.push_back(valueAlloc);
        //if(insertPtrFromAlloc(valueAlloc))
    }
    while(!valueNeedToMod.empty())
    {
        Value* valueAlloc = valueNeedToMod.back();
        valueNeedToMod.pop_back();
        insertPtrFromAlloc(valueAlloc, F);
    }
    return true;
}

bool NoDang::insertPtrFromAlloc(Value *value, Function &F)
{
    if(ifModified.find(value) != ifModified.end())
        return false;
    output("------------------Modify begin--------------");
    Value *modifiedValue, *middleValue;
    Twine newValName1 = value->getName()+"_1";
    Instruction* insPos = nullptr;
    if(AllocaInst *valueAlloc = dyn_cast<AllocaInst>(value))
    {
        valueAlloc = dyn_cast<AllocaInst>(value);
        Type *ttmp = valueAlloc->getAllocatedType();
        insPos = valueAlloc;
        modifiedValue = new AllocaInst(valueAlloc->getType(), newValName1, insPos);
    }
    else if(GlobalVariable *valueGlobal = dyn_cast<GlobalVariable>(value))
    {
        valueGlobal = dyn_cast<GlobalVariable>(value);
        Type *ttmp = valueGlobal->getType();
        ttmp = ttmp->getPointerElementType();
        insPos = F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
        modifiedValue = new AllocaInst(valueGlobal->getType(), newValName1, insPos);
    }
    else if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(value))
    {
        //not easy to modify struct element.
        //modifiedValue = new AllocaInst(GEPI->getType(), newValName1, insPos);
        return false;
    }
    else
    {
        return false;
        //assert(0 && "Exception: Unknow type of value.");
    }
    //begin to insert a pointer between them.
    middleValue = value;
    output(middleValue);
    output(modifiedValue);
    StoreInst* SI = new StoreInst(middleValue, modifiedValue);
    SI->insertAfter(insPos);
    ifInsModified[SI] = nullptr;

    
    ifModified[value] = modifiedValue;
    //This is to avoid indirect use: ie. store %p1, %p2; store %p2, %p3; store %p3, %p1;
    ifModified[modifiedValue] = nullptr;
    middleIns[modifiedValue] = middleValue;
    SmallVector<Value*, 16> users;
    for(Value::user_iterator uib = value->user_begin(), uie = value->user_end(); uib != uie; ++uib)
        users.push_back(*uib);
    //while (!value->use_empty()) 
    //Value *uib = value->user_back();
    for(SmallVector<Value*, 16>::reverse_iterator uib = users.rbegin(), uie = users.rend(); uib != uie; ++uib)
    //for(SmallVector<Value*, 16>::iterator uib = users.begin(), uie = users.end(); uib != uie; ++uib)
    {
        insertPtrForValue(*uib, modifiedValue, middleValue, value, nullptr, F, false);
    }
    /*while(!valueAlloc->use_empty()) 
    {
    }*/
    //valueAlloc->replaceAllUsesWith(modifiedPointer);
    //modifiedPointer->takeName(valueAlloc);
    //AA->replaceWithNewValue(valueAlloc,modifiedPointer);
    numOfMod++;

    output("------------------Modify end--------------");
    return true;   
}


bool NoDang::insertPtrForValue(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI)
{
    if(!ifGEPI)
        output("insert begin");
    else
        output("recrusive begin");
    output(uib);
    if(Instruction *I = dyn_cast<Instruction>(uib))
    {
        if(I->getParent()->getParent() != &F)
            ;//return false;
    }
    Value* targetOper = uib;
    if(BitCastInst *BCI = dyn_cast<BitCastInst>(uib))
    {
        targetOper =  BCI->user_back();
    }
    if(LoadInst* LI = dyn_cast<LoadInst>(targetOper))
    {
        insertPtrForValue(LI->user_back(),modifiedPointer,middlePointer,oriP,lastins, F, true);
        if(!ifGEPI)
        {
            LoadInst *newLI = new LoadInst(modifiedPointer, "newaddLoad", LI);
            output(newLI);
        }
    }
    else if(StoreInst* SI = dyn_cast<StoreInst>(targetOper))
    {
        Value* otherOper = nullptr;
        Value* curOper = nullptr;
        bool ifCurLeft = getOtherOper(SI, lastins, oriP, &curOper, &otherOper);

        Value* rootDef = getRootDef(otherOper);
        if(rootDef == nullptr)
        {
            return false;
        }
        if(isa<CallInst>(rootDef) || isa<Argument>(rootDef))
        {
            return false;
        }
        if(isa<AllocaInst>(rootDef) || isa<GlobalVariable>(rootDef))
        {
            if(ifModified.find(rootDef) == ifModified.end())
            {
                 valueNeedToMod.push_back(rootDef);
            }
        }
        if(!ifGEPI)
        {
            LoadInst *newLI = new LoadInst(modifiedPointer, "newaddStore", SI);
            output(newLI);
        }
    }
    else if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(targetOper))
    {
        insertPtrForValue(GEPI->user_back(),modifiedPointer,middlePointer,oriP,lastins, F, true);
        if(cast<PointerType>(GEPI->getPointerOperandType())->getElementType()->isStructTy())
        {
            //Value* newLIStr = new LoadInst(modifiedPointer, "newaddstruct", GEPI);
            //output(newLIStr);
        }
        if(!ifGEPI)
        {
            Value* newLIGEPI = new LoadInst(modifiedPointer, "newaddGEPI", GEPI);
            output(newLIGEPI);
        }
    }
    else if(GEPOperator* GEPO = dyn_cast<GEPOperator>(targetOper))
    {
        AllocaInst *newAI = new AllocaInst(GEPO->getType(), "allocForGEPO", F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
        for(Value::user_iterator uib = GEPO->user_begin(), uie = GEPO->user_end(); uib != uie; ++uib)
        {
            Instruction *gepub = dyn_cast<Instruction>(*uib);
            insertPtrForValue(gepub, newAI, gepub, GEPO, nullptr, F, false);
        }
    }
    if(!ifGEPI)
        output("insert end");
    else
        output("recrusive end");
    return true;
}

bool NoDang::ifNotPointer(Value *val)
{
    if(isa<ConstantPointerNull>(val) || isa<ConstantFP>(val) || isa<ConstantInt>(val))
        return true;
    Type* eleType = nullptr;
    if(AllocaInst* AI = dyn_cast<AllocaInst>(val))
    {
        eleType = AI->getAllocatedType();
    }
    else if(GlobalVariable* GV = dyn_cast<GlobalVariable>(val))
    {
        if(PointerType* pType = dyn_cast<PointerType>(GV->getType()))
        {
            eleType = pType->getElementType();
        }
    }
    if(eleType->isPointerTy())
        return false;
    return true;;
}

GetElementPtrInst* NoDang::detNCreateNewGEPI(Value* targetOper, Value* modifiedPointer)
{
    if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(targetOper))
    {
        std::vector<Value*> idextmp;
        for(GetElementPtrInst::op_iterator OI = GEPI->idx_begin(), OE = GEPI->idx_end(); OI != OE; ++OI)
            idextmp.push_back(*OI);
        GetElementPtrInst* newGEPI =  GetElementPtrInst::Create(modifiedPointer, makeArrayRef(idextmp), GEPI->getName(), GEPI);
        return newGEPI;
    }
    return nullptr;
}

bool NoDang::getOtherOper(StoreInst* SI, Value* lastins, Value* oriP, Value** CurOper, Value** OtherOper)
{
    Value* otherOper = SI->getValueOperand();
    Value* curOper = SI->getPointerOperand();
    bool ifCurLeft = false;
    if((lastins != nullptr && curOper != lastins) || (lastins == nullptr
                            && curOper != oriP))
    {
        Value* tmp = otherOper;
        otherOper = curOper;
        curOper = tmp;
        ifCurLeft = true;
    }
    *CurOper = curOper;
    *OtherOper = otherOper;
    return ifCurLeft;
}

int NoDang::getLevelOfPointer(Type* type)
{
    //Note: The variable level is init to 0, this is different from the below function but the return result is the same.
    int level = 0;
    Type *ttmp = type;
    PointerType *pttmp;
    while( ttmp != nullptr&& (pttmp = dyn_cast<PointerType>(ttmp)))
    {
        level++;
        ttmp = pttmp->getElementType();
    }
    if(ttmp == nullptr)
    {
        return -1;
    }
    return level;
}

int NoDang::getLevelOfPointer(Value *value)
{
    //ret the level of pointers, ie. char *p return 1, char p returns 0.
    int level = -1;
    Type *ttmp = value->getType();
    PointerType *pttmp;
    while((pttmp = dyn_cast<PointerType>(ttmp)))
    {
        level++;
        ttmp = pttmp->getElementType();
        if(ttmp->isArrayTy())
            ttmp = ttmp->getArrayElementType();
            
    }
    return level;
}

char NoDang::ID = 0;
//static RegisterPass<NoDang>Y("NoDang", "Add one level pointer to pointers");


static void registerNoDangPass(const PassManagerBuilder &,
                           PassManagerBase &PM) {
    PM.add(new NoDang());
}

static RegisterStandardPasses RegisterNoDangPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerNoDangPass);

//static RegisterStandardPasses RegisterNoDangPass0(
//    PassManagerBuilder::EP_EnabledOnOptLevel0, registerNoDangPass);

