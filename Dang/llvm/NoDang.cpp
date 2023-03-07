#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include <llvm/Analysis/AliasSetTracker.h>

using namespace llvm;

#define DEBUG_TYPE "NoDang"
#define REPORTLEVEL 1

namespace {
  struct NoDang : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    NoDang() : ModulePass(ID) {}
    bool runOnModule(Module &M) override;

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
    //This is used to store reverse Topological  order
    std::vector<Function*> revTopologicalOrder;
    //This is used to record whether an ins can't be deleted
    bool ifUsedAsExpect = false;
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
    if(REPORTLEVEL && tmp != nullptr)
        tmp->dump();
}

bool NoDang::ifOneLevelPointer(Function *F)
{
    assert(F && "F is nullptr, usually F is indirect function invocation");
    if(F->getName() == "malloc")
        return true;
    DenseMap<Function*, int>::iterator it = funRetPL.find(F); 
    if(it == funRetPL.end() || it->second != 1)
        return false;
    return true;
}

bool NoDang::runOnModule(Module &M)
{
    CallGraph callGraph = CallGraph(M);
    //Notice: Not used currently
    //getRevTopo(M, callGraph);
    errs()<<"Global:";
    AA = &getAnalysis<AliasAnalysis>();
    ASTracker = new AliasSetTracker(*AA);
    for(Module::global_iterator F = M.global_begin(), E = M.global_end(); F!= E; ++F)
    {
        
        if (!F->isConstant() && F->getType()->isPointerTy())
        {
            F->dump();
            errs() << " is as expected\n";
            for(Value::user_iterator uib = F->user_begin(), uie = F->user_end(); uib != uie; ++uib)
                uib->dump();
        }
    }
    for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F)
    //for(auto F:revTopologicalOrder)
    {
        if(F->isDeclaration())
            continue;
        pairedPointer.clear();
        mallocPointer.clear();
        ASTracker->clear();
        errs()<<"Function:";
        errs().write_escaped(F->getName()) << '\n';
        int levelOfFun = getLevelOfPointer(F->getReturnType());
        if(levelOfFun > 0)
        {
            funRetPL[F] = levelOfFun;
        }

        for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; ++I)
        {
            if (I->getType()->isPointerTy())
            {
                errs().write_escaped(I->getName()) << '\n';
            }
        }
        errs()<<"local variable:";
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            ASTracker->add(*BB);
            for(BasicBlock::iterator bbF=BB->begin(), bbE = BB->end(); bbF != bbE; ++bbF)
            {
                if (isa<AllocaInst>(&(*bbF)))
                {
                    errs().write_escaped(bbF->getName()) << '\n';
                }
                if (isa<CallInst>(&(*bbF)))
                {
                    CallInst *ci = dyn_cast<CallInst>(&(*bbF));
                    //The function may be indirect call, so use getCalledValue instead of getCalledFunction.
                    Function* calledFunction = dyn_cast<Function>(ci->getCalledValue()->stripPointerCasts());
                    if(ifOneLevelPointer(calledFunction))
                    {
                        mallocPointer.insert(dyn_cast<Value>(ci));
                        continue;
                    }
                    if(calledFunction->getName() != "free")
                        continue;
                    Value *v = getAllocValue(ci->getArgOperand(0));
                    assert(v && "Exception: v is null");
                    pairedPointer.insert(v);
                }
            }
        }
        insertPointers(*F);
    }
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
            assert(0 && "Exception: Num of operand in BitCastInst is large than 1");
        tmp = BCI->getOperand(0);
    }
    while(!isa<AllocaInst>(tmp) && !isa<GlobalVariable>(tmp)
        && !isa<ConstantPointerNull>(tmp) && !isa<CallInst>(tmp) && !isa<Argument>(tmp))
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
                errs()<< "Exception: other Operator ins found.\n";
                return nullptr;
            }
        }
        else
        {
            tmp->dump();
            errs()<< "Exception: other value ins found.\n";
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
                errs()<< "Exception: other Operator ins found.\n";
                return nullptr;
            }
        }
        else
        {
            tmp->dump();
            errs()<< "Exception: other value ins found.\n";
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
    for(SetVector<Value*>::iterator ib = pairedPointer.begin(), ie = pairedPointer.end(); ib != ie; ++ib)
    {
        //if(!ifHasAlias(*ib))
        //    continue;
        
        if(!(*ib)->hasName())
            continue;
        for(SetVector<Value*>::iterator citb = mallocPointer.begin(), cite = mallocPointer.end(); citb != cite; ++citb)
        {
            if(AA->alias(*ib,*citb) == AliasAnalysis::MayAlias)
            {
                errs()<<"alias:\n"; 
                //find all coresponding malloc for the pointer
                //If we want to modify all alloc like value, just move it outside of the block.
                corMalloc[*citb] = false;
            }
        }
        
    }
    //for each malloc instruction in corMalloc, find alloc instruction for the pointer and change it's all corresponding pointer.
    for(DenseMap<Value*, bool>::iterator ib = corMalloc.begin(), ie = corMalloc.end(); ib != ie; ++ib)
    {
        if(ib->second)
            continue;
        if(ib->first->getNumUses() > 1)
        {
            errs()<<"Exception: More than 1 use for malloc, ignored\n";
            continue;
        }
        //Usually, %call = call @malloc and only store 'call' to var once.
        Value* uito = ib->first->user_back();
        if(isa<BitCastInst>(uito))
        {
            BitCastInst *BCI = dyn_cast<BitCastInst>(uito);
            if(BCI->getNumUses() != 1)
                assert(0 && "Exception: Num of operand in BitCastInst is large than 1");
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
    errs()<<"Modifing:\n";
    if(ifModified.find(value) != ifModified.end())
        return false;
    Value *modifiedValue, *middleValue;
    Twine newValName1 = value->getName()+".1";
    Twine newValName2 = value->getName()+".2";
    int pointLevel = getLevelOfPointer(value);
    if(isa<AllocaInst>(value))
    {
        AllocaInst *valueAlloc, *modifiedPointer, *middlePointer;
        valueAlloc = dyn_cast<AllocaInst>(value);
        Type *ttmp = valueAlloc->getAllocatedType();
        if(ttmp->isArrayTy())
        {
            Type* arrayTy = ttmp->getArrayElementType();
            unsigned addSpace = 0;
            if(arrayTy->isPointerTy())
                addSpace = arrayTy->getPointerAddressSpace();
            PointerType *pt = PointerType::get(arrayTy, addSpace);
            ArrayType *at = ArrayType::get(pt, ttmp->getArrayNumElements());
            modifiedPointer= new AllocaInst(at, newValName1, valueAlloc);
        }
        else
        {
            modifiedPointer = new AllocaInst(valueAlloc->getType(), newValName1, valueAlloc);
        }
        modifiedPointer->setAlignment(valueAlloc->getAlignment());
        if(pointLevel == 1)
        {
            Type *AgTy = cast<PointerType>(valueAlloc->getType())->getElementType();
            //middlePointer = new AllocaInst(AgTy, newValName2, valueAlloc);
            //middlePointer->setAlignment(valueAlloc->getAlignment());
            middlePointer = valueAlloc;
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
        if(ttmp->isArrayTy())
        {
            Type* arrayTy = ttmp->getArrayElementType();
            unsigned addSpace = 0;
            if(arrayTy->isPointerTy())
                addSpace = arrayTy->getPointerAddressSpace();
            PointerType *pt = PointerType::get(arrayTy, addSpace);
            ArrayType *at = ArrayType::get(pt, ttmp->getArrayNumElements());
            modifiedPointer = new GlobalVariable(*(valueGlobal->getParent()), at, valueGlobal->isConstant(), valueGlobal->getLinkage(), Constant::getNullValue(at), newValName1, valueGlobal, valueGlobal->getThreadLocalMode(), 0, valueGlobal->isExternallyInitialized());
        }
        else
        {
            modifiedPointer = new GlobalVariable(*(valueGlobal->getParent()), valueGlobal->getType(), valueGlobal->isConstant(), valueGlobal->getLinkage(), Constant::getNullValue(valueGlobal->getType()), newValName1, valueGlobal, valueGlobal->getThreadLocalMode(), 0, valueGlobal->isExternallyInitialized());
        }
        modifiedPointer->setAlignment(valueGlobal->getAlignment());
        if(pointLevel == 1)
        {
            Type *AgTy = cast<PointerType>(valueGlobal->getType())->getElementType();
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
        output(value);
        assert(0 && "Exception: Unknow type of value.");
    }
    //begin to insert a pointer between them.
    output(value);
    output(modifiedValue);
    output(middleValue);
    
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
    if(lastins)
        output("Recursive insert");
    else
        output("begin to insert");
    if(uib == nullptr || ifInsModified.find(uib) != ifInsModified.end())
    {
        return false;
    }
    output(uib);
    //TODO: modifiy struct code
    Value* targetOper = uib;
    if(LoadInst* LI = dyn_cast<LoadInst>(targetOper))
    {
        Value* realModPtr = modifiedPointer;
        Value* POV = LI->getPointerOperand();
        /*
        won't be happened
        if(isa<GEPOperator>(POV) && !isa<GetElementPtrInst>(POV))
        {
            GEPOperator* geto = dyn_cast<GEPOperator>(POV);
            std::vector<Value*> idextmp;
            for(GetElementPtrInst::op_iterator OI = geto->idx_begin(), OE = geto->idx_end(); OI != OE; ++OI)
                idextmp.push_back(*OI);
            GetElementPtrInst* modifiedGEPI = GetElementPtrInst::Create(modifiedPointer, makeArrayRef(idextmp), "arrayidx", LI);
            modifiedGEPI->setIsInBounds(geto->isInBounds());
            realModPtr = modifiedGEPI;
        }*/
        LoadInst *newLI = new LoadInst(realModPtr, "", LI->isVolatile(), LI);
        ifInsModified[LI] = newLI;
        if(!insertPtrForValue(LI->user_back(), newLI, middlePointer, oriP, LI))
        {
            LoadInst *targetLI = new LoadInst(newLI, "", LI->isVolatile(), LI);
            LI->replaceAllUsesWith(targetLI);
        }
        if(LI->use_empty())
            LI->eraseFromParent();
        else if(!ifUsedAsExpect)
        {
            output(LI);
            output(LI->user_back());
            //assert(0 && "Exception: LI should be able to erase in LoadInst.");
        }
        else
            ifUsedAsExpect = false;
    }
    else if(StoreInst* SI = dyn_cast<StoreInst>(targetOper))
    {
        //get the other operand in this store inst
        Value* otherOper = nullptr;
        Value* curOper = nullptr;
        bool ifCurLeft = getOtherOper(SI, lastins, oriP, &curOper, &otherOper);

        Value* rootDef = getRootDef(otherOper);
        output("rootdef is:");
        output(rootDef);
        output("otherOper is:");
        output(otherOper);
        if(isa<CallInst>(rootDef) || isa<Argument>(rootDef))
        {
            //we don't modify argument and regard it as call
            output("CallInst or Argument");
            StoreInst *middleSI = nullptr, *targetSI = nullptr;
            if(lastins == nullptr)
            {
                middleSI = new StoreInst(otherOper, middlePointer, SI->isVolatile(), SI);
                targetSI = new StoreInst(middlePointer, modifiedPointer, SI->isVolatile(), SI);
            }
            else
            {
                /*
                %call = call noalias i8* @malloc(i64 10) #3
                %arrayidx = getelementptr inbounds [10 x i8*]* %ap, i32 0, i64 0
                store i8* %call, i8** %arrayidx, align 8
                */
                middleSI = new StoreInst(otherOper, lastins, SI->isVolatile(), SI);
                targetSI = new StoreInst(lastins, modifiedPointer, SI->isVolatile(), SI);
                ifUsedAsExpect = true;
            }
            output(middleSI);
            output(targetSI);
            SI->eraseFromParent();
        }
        else if(isa<ConstantPointerNull>(rootDef))
        {
            output("Constant");
            LoadInst* middleLI = new LoadInst(modifiedPointer, "", SI->isVolatile(), SI);
            output(middleLI);
            StoreInst* targetSI = new StoreInst(otherOper, middleLI, SI->isVolatile(), SI);
            output(targetSI);
            SI->eraseFromParent();
        }
        else if(isa<AllocaInst>(rootDef) || isa<GlobalVariable>(rootDef))
        {
            if(ifModified.find(rootDef) == ifModified.end())
            {
                insertPtrFromAlloc(rootDef);
            }
            else
            {
                DenseMap<Value*, Value*>::iterator it = ifInsModified.find(otherOper);
                DenseMap<Value*, Value*>::iterator ifAllocModified = ifModified.find(rootDef);
                Value* targetOper = curOper;
                Value* storeInstValue = nullptr;
                Value* storeInstPointer = nullptr;
                if(isa<GEPOperator>(otherOper) && !isa<GetElementPtrInst>(otherOper))
                {
                    GEPOperator* geto = dyn_cast<GEPOperator>(otherOper);
                    //This block only handles GEPOperator, as  GetElementPtrInst* can be casted to GEPOperator, so the condition is different.
                    output("Branch0");
                    output(otherOper);
                    std::vector<Value*> idextmp;
                    for(GetElementPtrInst::op_iterator OI = geto->idx_begin(), OE = geto->idx_end(); OI != OE; ++OI)
                        idextmp.push_back(*OI);
                    DenseMap<Value*, Value*>::iterator ifModifiedIt = ifModified.find(rootDef);
                    GetElementPtrInst* modifiedGEPI = GetElementPtrInst::Create(ifModifiedIt->second, makeArrayRef(idextmp), "", SI);
                    modifiedGEPI->setIsInBounds(geto->isInBounds());
                    targetOper = modifiedGEPI;
                }
                if(ifAllocModified != ifModified.end() && lastins != nullptr && isa<LoadInst>(lastins))
                {
                    //isa<LoadInst>(lastins) is used to avoid lastins is GEPins
                    output("Branch1");
                    if(it != ifInsModified.end())
                    {
                        Value* modifiedLastIns = ifInsModified.find(lastins)->second;
                        if(it->second == nullptr)
                            return true;
                        output("aaaaaaaaaaaaaaaaaaa");
                        LoadInst* newLI1 = new LoadInst(it->second, "", SI->isVolatile(), SI);
                        LoadInst* newLI2 = new LoadInst(modifiedLastIns, "", SI->isVolatile(), SI);
                        if(ifCurLeft)
                        {
                            storeInstValue = newLI2;
                            storeInstPointer = newLI1;
                        }
                        else
                        {
                            storeInstValue = newLI1;
                            storeInstPointer = newLI2;
                        }
                    }
                    else
                    {
                        /*
                        %1 = load i8** %p1, align 8
                        store i8* %1, i8** %p2, align 8
                        %2 = load i8** %p2, align 8
                        use %2
                        %4 = load i8** %p2, align 8
                        %5 = load i8* %4, align 1
                        %6 = load i8** %p1, align 8
                        store i8 %5, i8* %6, align 1
                        Note that no new load ins for %6.
                        */
                        Value* backUse = getRootDef(otherOper, true);
                        insertPtrForValue(backUse, ifAllocModified->second, rootDef, rootDef, nullptr);
                    }
                }
                else if(rootDef == otherOper)
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
                }
                else if(ifAllocModified != ifModified.end())
                {
                    output("Branch2");
                    if(it != ifInsModified.end())
                    {
                        if(!ifCurLeft)
                        {
                            storeInstValue = it->second;
                            storeInstPointer = modifiedPointer;
                        }
                        else
                        {
                            storeInstValue = modifiedPointer;
                            storeInstPointer = it->second;
                        }
                    }
                    else
                    {
                        //TOBE improvemented
                        Value* backUse = getRootDef(otherOper, true);
                        insertPtrForValue(backUse, ifAllocModified->second, rootDef, rootDef, nullptr);

                    }
                }
                else if(lastins != nullptr)
                {
                    Value* modifiedLastIns = ifInsModified.find(lastins)->second;
                    output("Branch3");
                    if(ifCurLeft)
                    {
                        storeInstValue = modifiedLastIns;
                        storeInstPointer = modifiedPointer;
                    }
                    else
                    {
                        storeInstValue = modifiedPointer;
                        storeInstPointer = modifiedLastIns;
                    }
                }
                else
                {
                    DenseMap<Value*, Value*>::iterator newAlloc = ifModified.find(rootDef);
                    output("Branch5");
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
                }
                if(storeInstValue != nullptr)
                {
                    output(storeInstValue);
                    output(storeInstPointer);
                    StoreInst* newSI = new StoreInst(storeInstValue, storeInstPointer, SI->isVolatile(), SI);
                    output(newSI);
                    SI->eraseFromParent();
                }
            }
        }
        else
        {
            rootDef->dump();
            assert(0 && "Exception: rootdef unhandled");
        }
    }
    else if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(targetOper))
    {
        //Used to skip GetElementPtrInst for struct.
        //TODO: this should be improved to really modify element in struct instead of just inserting a loadinst.
        if(cast<PointerType>(GEPI->getPointerOperandType())->getElementType()->isStructTy())
        {
            output("Skip this ins");
            LoadInst *uselessLI = new LoadInst(modifiedPointer, "useless", GEPI);
            ifInsModified[uib] = nullptr;
            return false;
        }
        Value* GEPptr = modifiedPointer;
        if(lastins != nullptr)
        {
            /*
            %3 = load %struct.ts** %tsp2, align 8
            %buf4 = getelementptr inbounds %struct.ts* %3, i32 0, i32 1
            */
            if(LoadInst* lastLI = dyn_cast<LoadInst>(lastins))
                GEPptr = new LoadInst(modifiedPointer, "", lastLI->isVolatile(),GEPI);
            else
                assert(0 && "Exception: last ins not handled.");
        }
        std::vector<Value*> idextmp;
        for(GetElementPtrInst::op_iterator OI = GEPI->idx_begin(), OE = GEPI->idx_end(); OI != OE; ++OI)
            idextmp.push_back(*OI);
        GetElementPtrInst* newGEPI =  GetElementPtrInst::Create(GEPptr, makeArrayRef(idextmp), GEPI->getName(), GEPI);

        ifInsModified[GEPI] = newGEPI;
        if(!insertPtrForValue(GEPI->user_back(), newGEPI, middlePointer, oriP, GEPI))
        {
            LoadInst *targetLI = new LoadInst(newGEPI, "", GEPI);
            GEPI->replaceAllUsesWith(targetLI);
        }
        if(GEPI->use_empty())
            GEPI->eraseFromParent();
        else if(!ifUsedAsExpect)
        {
            output(GEPI);
            output(GEPI->user_back());
            assert(0 && "Exception: LI should be able to erase in GEPInst.");
        }
        else
            ifUsedAsExpect = false;
    }
    else if(GEPOperator* GEPO = dyn_cast<GEPOperator>(targetOper))
    {
        output("GEPO begin");
        std::vector<Value*> idextmp;
        for(GetElementPtrInst::op_iterator OI = GEPO->idx_begin(), OE = GEPO->idx_end(); OI != OE; ++OI)
            idextmp.push_back(*OI);
        Value *pre;
        //GEPOperator* middleGepo = cast<GEPOperator>(middleGEPI);
        //GEPOperator* modifiedGepo = cast<GEPOperator>(modifiedGEPI);
        SmallVector<Value*, 16> gepoUsers;
        Value* tarPointer = modifiedPointer;
        for(Value::user_iterator uib = GEPO->user_begin(), uie = GEPO->user_end(); uib != uie; ++uib)
            gepoUsers.push_back(*uib);
        bool ifSkip = false;
        
        for(SmallVector<Value*, 16>::reverse_iterator uib = gepoUsers.rbegin(), uie = gepoUsers.rend(); uib != uie; ++uib)
        {
            if(*uib == nullptr)
                output("aaaaaaaaaa");
            assert(*uib && "Exception: uib is null");
            /*TODO: in example 
            char *ap[10];
            char **ap1[10];
            ap1 will has two modfications, 
            @ap = common global [10 x i8*] zeroinitializer, align 16
            @ap1.1 = common global [10 x i8***] zeroinitializer, align 16
            @ap1.11 = common global [10 x i8**]* null, align 16
            This is not right
            */
            Instruction *gepub = dyn_cast<Instruction>(*uib);

            if(PointerType* PT = dyn_cast<PointerType>(modifiedPointer->getType()))
            {
                if(PT->getElementType()->isPointerTy())
                {
                    tarPointer = new LoadInst(modifiedPointer, "unused", *uib);
                    output(tarPointer);
                    ifSkip = true;
                    continue;
                }
            }
            GetElementPtrInst* modifiedGEPI =  GetElementPtrInst::Create(tarPointer, makeArrayRef(idextmp), "GlobalArrayidx", gepub);
            GetElementPtrInst* middleGEPI =  GetElementPtrInst::Create(middlePointer, makeArrayRef(idextmp), "GlobalArrayidx", gepub);
            modifiedGEPI->setIsInBounds(GEPO->isInBounds());
            middleGEPI->setIsInBounds(GEPO->isInBounds());
            //TODO: The instruction should be replaced by operation.
            //GEPOperator* middleGepo = cast<GEPOperator>(middleGEPI);
            //GEPOperator* modifiedGepo = cast<GEPOperator>(modifiedGEPI);
            if(pre == gepub)
            {
                assert(0 && "Exception: Dead loop in GEP.");
            }
            insertPtrForValue(gepub, modifiedGEPI, middleGEPI, GEPO, nullptr);
            //insertPtrForValue(gepub, modifiedGepo, middleGepo, pointLevel);
            pre = gepub;
        }
        //if(!ifSkip)
        //    GEPO->dropAllReferences();
    }
    else
        return false;
    ifInsModified[uib] = nullptr;
    return true;
}

bool NoDang::handleStructEle(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins)
{
    if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(uib))
    {
        if(cast<PointerType>(GEPI->getPointerOperandType())->getElementType()->isStructTy())
        {
            output("aaaaaaaaaaaaaaaaaaaa");
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
    while((pttmp = dyn_cast<PointerType>(ttmp)))
    {
        level++;
        ttmp = pttmp->getElementType();
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
static RegisterPass<NoDang>
Y("NoDang", "Add one level pointer to pointers");
