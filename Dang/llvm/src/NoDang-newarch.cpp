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
    bool insertPtrForValue(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins);
    bool insertPtrFromAlloc(Value *value);
    bool ifHasAlias(Value *value);
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
    void output(Value* value);
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
    DenseMap<Value*, bool> corMalloc;
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
    errs()<<"insert pointer:\n";
    //bool equals false means the malloc ins havn't been inserted, equals 1 means inserted.
    corMalloc.clear();
    for(SetVector<Value*>::iterator citb = mallocPointer.begin(), cite = mallocPointer.end(); citb != cite; ++citb)
    {
        corMalloc[*citb] = false;
    }
        
    //for each malloc instruction in corMalloc, find alloc instruction for the pointer and change it's all corresponding pointer.
    for(DenseMap<Value*, bool>::iterator ib = corMalloc.begin(), ie = corMalloc.end(); ib != ie; ++ib)
    {
        if(ib->second)
            continue;
        if(ib->first->getNumUses() > 1)
        {
            errs()<<"Exception: More than 1 use for malloc, ignored\n";
            ib->first->dump();
            ib->first->user_back()->dump();
            for(auto use:ib->first->users())
                use->dump();
            continue;
        }
        //Usually, %call = call @malloc and only store 'call' to var once.
        if(ib->first->use_empty())
            continue;
        Value* uito = ib->first->user_back();
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
        Value* valueAlloc;
        //Find the alloca instruction for the malloced pointer
        if(StoreInst *sitmp = dyn_cast<StoreInst>(uito))
        {
            Value *voptmp = sitmp->getPointerOperand();
            valueAlloc = getAllocValue(voptmp);
        }
        if(valueAlloc == nullptr)
        {
            errs()<<"Exception: No alloc for pointer, ignored\n";
            uito->dump();
            continue;
        }
        //ifInsModified.clear();
        insertPtrFromAlloc(valueAlloc);
        //if(insertPtrFromAlloc(valueAlloc))
        //    checkAndDeleteAlloc(valueAlloc);
    }
    return true;
}

bool NoDang::insertPtrFromAlloc(Value *value)
{
    if(ifModified.find(value) != ifModified.end())
        return false;
    output("------------------Modify begin--------------");
    Value *modifiedValue, *middleValue;
    Twine newValName1 = value->getName()+".1";
    int pointLevel = getLevelOfPointer(value);
    int ifNewStoreInst = 0;
    numOfMod++;
    if(isa<AllocaInst>(value))
    {
        AllocaInst *valueAlloc, *modifiedPointer, *middlePointer;
        valueAlloc = dyn_cast<AllocaInst>(value);
        Type *ttmp = valueAlloc->getAllocatedType();
        modifiedPointer = new AllocaInst(valueAlloc->getType(), newValName1, valueAlloc);
        modifiedPointer->setAlignment(valueAlloc->getAlignment());
        if(pointLevel == 1)
        {
            //middlePointer = new AllocaInst(AgTy, newValName2, valueAlloc);
            //middlePointer->setAlignment(valueAlloc->getAlignment());
            middlePointer = valueAlloc;
            ifNewStoreInst += 1;
        }
        else if(pointLevel > 1)
        {
            //two or more levels of pointer don't need to insert pointer, because it will point to other inserted pointer
            middlePointer = nullptr;
        }
        modifiedValue = modifiedPointer;
        middleValue = middlePointer;
    }
    else if(isa<GlobalVariable>(value))
    {
        GlobalVariable *valueGlobal, *modifiedPointer, *middlePointer;
        valueGlobal = dyn_cast<GlobalVariable>(value);
        Type *ttmp = valueGlobal->getType();
        ttmp = ttmp->getPointerElementType();
        modifiedPointer = new GlobalVariable(*(valueGlobal->getParent()), valueGlobal->getType(), valueGlobal->isConstant(), valueGlobal->getLinkage(), Constant::getNullValue(valueGlobal->getType()), newValName1, valueGlobal, valueGlobal->getThreadLocalMode(), 0, valueGlobal->isExternallyInitialized());
        modifiedPointer->setAlignment(valueGlobal->getAlignment());
        if(pointLevel == 1)
        {
            //middlePointer = new GlobalVariable(*(valueGlobal->getParent()), AgTy, valueGlobal->isConstant(), valueGlobal->getLinkage(), valueGlobal->getInitializer(), newValName2, valueGlobal, valueGlobal->getThreadLocalMode(), 0, valueGlobal->isExternallyInitialized());
            //middlePointer->setAlignment(valueGlobal->getAlignment());
            middlePointer = valueGlobal;
            //two globals have same initializer, the init for middlePointer should be in the first store ins in modifiedPointer.
            //errs()<<"GlobalInfo:\n";
            //valueGlobal->getParent()->dump();
            //modifiedPointer->getParent()->dump();
        }
        else if(pointLevel > 1)
        {
            middlePointer = nullptr;
            modifiedPointer = new GlobalVariable(*(valueGlobal->getParent()), valueGlobal->getType(), valueGlobal->isConstant(), valueGlobal->getLinkage(), Constant::getNullValue(valueGlobal->getType()), newValName1, valueGlobal, valueGlobal->getThreadLocalMode(), 0, valueGlobal->isExternallyInitialized());
            modifiedPointer->setAlignment(valueGlobal->getAlignment());
        }
        modifiedValue = modifiedPointer;
        middleValue = middlePointer;
 
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
    if(AllocaInst* AI = dyn_cast<AllocaInst>(value))
    {
        if(ifNewStoreInst == 1)
        {
            StoreInst* SI = new StoreInst(middleValue, modifiedValue);
            SI->insertAfter(AI);
            ifInsModified[SI] = nullptr;
        }
    }

    
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
        insertPtrForValue(*uib, modifiedValue, middleValue, value, nullptr);
    }
    /*while(!valueAlloc->use_empty()) 
    {
    }*/
    //valueAlloc->replaceAllUsesWith(modifiedPointer);
    //modifiedPointer->takeName(valueAlloc);
    //AA->replaceWithNewValue(valueAlloc,modifiedPointer);

    output("------------------Modify end--------------");
    return true;   
}

bool NoDang::checkAndDeleteAlloc(Value *value)
{
    if(isa<AllocaInst>(value) || isa<GlobalVariable>(value))
    {
        if(value->use_empty())
        {
            errs()<<"Deleting all instructions\n";
            if(AllocaInst* tmp = dyn_cast<AllocaInst>(value))
                tmp->eraseFromParent();
            else if(GlobalVariable* tmp = dyn_cast<GlobalVariable>(value))
                tmp->eraseFromParent();
            return true;
        }
        else
        {
            errs()<<"Some instructions not delete\n";
            for(Value::user_iterator uib = value->user_begin(), uie = value->user_end(); uib != uie; ++uib)
                uib->dump();
            return false;
        }
    }
    else
        assert(0 && "Exception: Other ins");
}

bool NoDang::insertPtrForValue(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins)
{
    if(uib == nullptr || ifInsModified.find(uib) != ifInsModified.end())
    {
        return false;
    }
    if(lastins)
        output("Recursive insert");
    else
        output("begin to insert");
    output(uib);
    if(LoadInst* LI = dyn_cast<LoadInst>(uib))
    {
        LoadInst *newLI = new LoadInst(modifiedPointer, "", LI->isVolatile(), LI);
        ifInsModified[LI] = newLI;
        output(newLI);
        if(!insertPtrForValue(LI->user_back(), newLI, middlePointer, oriP, LI))
        {
            LoadInst *targetLI = new LoadInst(newLI, "", LI->isVolatile(), LI);
            LI->replaceAllUsesWith(targetLI);
            LI->eraseFromParent();
        }
    }
    else if(StoreInst* SI = dyn_cast<StoreInst>(uib))
    {
         Value* otherOper = nullptr;
         Value* curOper = nullptr;
         bool ifCurLeft = getOtherOper(SI, lastins, oriP, &curOper, &otherOper);

         Value* rootDef = getRootDef(otherOper);
         if(rootDef == nullptr)
         {
             return false;
         }
         output("rootdef is:");
         output(rootDef);
         output("otherOper is:");
         output(otherOper);
         if(isa<CallInst>(rootDef) || isa<Argument>(rootDef))
         {
             //we don't modify argument and regard it as call
             output("CallInst or Argument");
             StoreInst *middleSI = nullptr, *targetSI = nullptr;
             targetSI = new StoreInst(middlePointer, modifiedPointer, SI->isVolatile(), SI);
             output(targetSI);
         }
         else if(isa<AllocaInst>(rootDef) || isa<GlobalVariable>(rootDef))
         {
             if(ifModified.find(rootDef) == ifModified.end())
             {
                 insertPtrFromAlloc(rootDef);
             }
             else
             {
                 Value* storeInstValue = nullptr;
                 Value* storeInstPointer = nullptr;
                 if(rootDef == otherOper && lastins == nullptr)
                 {
                     DenseMap<Value*, Value*>::iterator newAlloc = ifModified.find(rootDef);
                     output("Branch4");
                     if(!ifCurLeft) 
                     {
                         storeInstValue = newAlloc->second;
                         storeInstPointer = modifiedPointer;
                     }
                     else
                     {
                         storeInstValue = modifiedPointer;
                         storeInstPointer = newAlloc->second;
                     }
                    StoreInst* newSI = new StoreInst(storeInstValue, storeInstPointer, SI->isVolatile(), SI);
                    output(newSI);
                    SI->eraseFromParent();
                 }
                 else
                    goto NotMod;
             }
         }
    }
    else
        goto NotMod;
    ifInsModified[uib] = nullptr;
    if(lastins)
        output("Recursive insert end.");
    else
        output("begin to insert end.");
    return true;
  NotMod:
    ifInsModified[uib] = nullptr;
    if(lastins)
        output("Recursive insert end.");
    else
        output("begin to insert end.");
    return false;
}

bool NoDang::handleStructEle(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins)
{
    if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(uib))
    {
        if(cast<PointerType>(GEPI->getPointerOperandType())->getElementType()->isStructTy())
        {
            LoadInst *uselessLI = new LoadInst(modifiedPointer, "useless", GEPI);
            ifInsModified[uib] = nullptr;
            return true;
        }
    }
    return false;
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

void NoDang::_DFSTopSort(Function *i, DenseMap<Function*, int>& colors, CallGraph &callGraph)
{
    if(i == nullptr && i->empty()&& i->isDeclaration())
        return;
    colors[i] = 1;//vISIT i

    CallGraphNode *children = callGraph[i];
    if(children->size() != 0) 
    {
        for(CallGraphNode::iterator F = children->begin(), E = children->end(); F!=E; F++) 
        {
            if(F->second->empty())
                continue;
            Function *tmp = F->second->getFunction();
            //children have not been visited
            DenseMap<Function*, int>::iterator it = colors.find(tmp);
            if(it == colors.end())
                tmp->dump();
            if(it != colors.end() && it->second == 0)
            { 
                _DFSTopSort(tmp, colors, callGraph);
            }
        }
    }
    revTopologicalOrder.push_back(i);
    colors[i] = 2;
}

void NoDang::getRevTopo(Module &M, CallGraph &callGraph)
{
    //Function *mainFun = M.getFunction("main");
    SetVector<Function*> topLevelFun;
    DenseMap<Function*, int> colors;
    for(Module::iterator tmp = M.begin(), E = M.end(); tmp!= E; ++tmp)
    {
        CallGraphNode *cgn=callGraph[tmp];
        if(cgn->getNumReferences() == 1)
        {
            topLevelFun.insert(tmp);
        }
        if(cgn->getNumReferences() != 0)
            colors[tmp] = 0;
    }
    for (auto topLevelF : topLevelFun){
        _DFSTopSort(topLevelF, colors, callGraph);
    }
}

char NoDang::ID = 0;
static RegisterPass<NoDang>Y("NoDang", "Add one level pointer to pointers");

/*
static void registerNoDangPass(const PassManagerBuilder &,
                           PassManagerBase &PM) {
    PM.add(new NoDang());
}

static RegisterStandardPasses RegisterNoDangPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerNoDangPass);
*/
//static RegisterStandardPasses RegisterNoDangPass0(
//    PassManagerBuilder::EP_EnabledOnOptLevel0, registerNoDangPass);

