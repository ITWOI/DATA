#include "NoDang.h"


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
    {
        if(!isa<CallInst>(tmp))
            tmp->dump();
    }
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
bool NoDang::doInitialization(Module &M)
{
    this->M = &M;
    pugi::xml_document doc;
    if (!doc.load_file("MOLINT.POINTER.NULL.SET.xml")) 
       return false;
    for (pugi::xml_node tool: doc.children("error"))
    {
        potentialDP.insert(tool.child("function").child_value());
        vulLoc[atoi(tool.child("line").child_value())] = true;
    }
    for(auto git = M.global_begin(); git != M.global_end(); git++)
    {
        pointerCount(git);
    }
    return false;
}

bool NoDang::doFinalization(Module &M)
{
    errs()<<"Num of modified vars: " << numOfMod << "\n";
    errs()<<"Num of modified instructions: " << numOfInsMod << "\n";
    errs()<<"Num of total Pointers: " << totalVar << "\n";
    return false;

}
bool NoDang::runOnFunction(Function &F)
{
    //guided by static warnning.
    bool ifPot = false;
    //for convenience....
    if(potentialDP.empty()) ifPot = true;
    for(auto str:potentialDP)
    {
        if(F.getName().str() == str)
            ifPot = true;
    }
    if(modifiedFun.find(&F) != modifiedFun.end() || (!ifPot && IFUSESTATIC))
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
    mallocPointer.clear();
    ASTracker->clear();
    errs()<<"------------------Function:"<< F.getName()<< "--------------\n";

    output("Variables to be mod:");
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
    {
        ASTracker->add(*BB);
        for(BasicBlock::iterator bbF=BB->begin(), bbE = BB->end(); bbF != bbE; ++bbF)
        {
            if (CallInst *ci = dyn_cast<CallInst>(&(*bbF)))
            {
                //The function may be indirect call, so use getCalledValue instead of getCalledFunction.
                Function* calledFunction = dyn_cast<Function>(ci->getCalledValue()->stripPointerCasts());
                /*TODO:
                  %call22 = tail call i8* %5(i8* %7, i32 55768, i32 1) #7
                  %5 = phi i8* (i8*, i32, i32)* [ @default_bzalloc, %if.then14 ], [ %4, %if.end9 ]
                  will result in F == null
                 */
                //assert(calledFunction && "F is nullptr, usually F is indirect function invocation");
                if(vulLoc.find(ci->getDebugLoc().getLine()) != vulLoc.end() && IFUSESTATIC)
                    continue;
                if(calledFunction == nullptr)
                    continue;
                if(calledFunction->getName() == "free")
                    freePointer.insert(ci);
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
    errs()<<"------------------Function end------------------------\n";
    output(&F);
    for(SetVector<Value*>::iterator citb = mallocPointer.begin(), cite = mallocPointer.end(); citb != cite; ++citb)
    {
        printVal(false, *citb);
    }
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
    {
        for(BasicBlock::iterator bbF=BB->begin(), bbE = BB->end(); bbF != bbE; ++bbF)
        {
            pointerCount(&(*bbF));
        }
    }
    return true;
}

void NoDang::printVal(bool ifAll, Value* val)
{
    if(!ifod)
        return;
    Printf = cast<Function>(M->getOrInsertFunction("printf",
                FunctionType::get(IntegerType::getInt32Ty(M->getContext()), 
                PointerType::get(Type::getInt8Ty(M->getContext()), 0), true )));
    if(isa<LoadInst>(val) ||isa<CallInst>(val))
    {
        if(CallInst *CI = dyn_cast<CallInst>(val))
            if(Function *F = CI->getCalledFunction())
                if(F->getName() == "printf")
                    return;
        Instruction* ins = cast<Instruction>(val);
        if(printedIns.find(ins) != printedIns.end())
            return;
        std::vector<Value *> ArgsV;
        std::vector<Constant*> const_ptr_5_indices;
        ConstantInt* const_int64_6 = ConstantInt::get(M->getContext(), APInt(64, StringRef("0"), 10));
        const_ptr_5_indices.push_back(const_int64_6);
        const_ptr_5_indices.push_back(const_int64_6);
        Constant* const_ptr_5 = nullptr;
        if(ifAll)
        {
            Constant *const_array_4 = ConstantDataArray::getString(M->getContext(), "A:%p;\0A\00", true);
            GlobalVariable *globalStrAll = new GlobalVariable(*M, const_array_4->getType(), 
                    true, GlobalValue::LinkOnceODRLinkage , nullptr, ".strALL");
            globalStrAll->setAlignment(1);
            globalStrAll->setInitializer(const_array_4);
            const_ptr_5 = ConstantExpr::getGetElementPtr(globalStrAll, const_ptr_5_indices);
        }
        else
        {
            Constant *const_array_4 = ConstantDataArray::getString(M->getContext(), "D:%p;\0A\00", true);
            GlobalVariable *globalStrDD = new GlobalVariable(*M, const_array_4->getType(), 
                    true, GlobalValue::LinkOnceODRLinkage , nullptr, ".strDD");
            globalStrDD->setAlignment(1);
            globalStrDD->setInitializer(const_array_4);

            const_ptr_5 = ConstantExpr::getGetElementPtr(globalStrDD, const_ptr_5_indices);
        }
        ArgsV.push_back(const_ptr_5);
        ArgsV.push_back(val);

        CallInst *CI = CallInst::Create(Printf, ArgsV, "printCall");
        CI->insertAfter(ins);
        printedIns.insert(ins);
    }
}

void NoDang::pointerCount(Value *val)
{
    unsigned num = totalVar;
    if(CallInst* LI = dyn_cast<CallInst>(val))
    {
        if(PointerType* PTY = dyn_cast<PointerType>(LI->getType()))
            printVal(true, LI);   
        for(int i = 0; i< LI->getNumArgOperands(); i++)
            if(LI->getArgOperand(i)->getType()->isPointerTy())
                printVal(true, LI->getArgOperand(i));   

    }

    if(AllocaInst *AI = dyn_cast<AllocaInst>(val))
    {
        if(AI->getName().find('.') == AI->getName().npos)
            totalVar++;
    }
    else if(GlobalVariable *GV = dyn_cast<GlobalVariable>(val))
    {
        totalVar++;
    }
    else
    {
        //if(val->hasName() && val->getType()->isPointerTy()) totalVar++;
    }
    if(num != totalVar)
    {
    }
    
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
                //tmp->dump();
                errs()<< "Exception: In getRootDef other Operator ins found.\n";
                return nullptr;
            }
        }
        else
        {
            //tmp->dump();
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
                //tmp->dump();
                errs()<< "Exception: In alloca other Operator ins found.\n";
                return nullptr;
            }
        }
        else
        {
            //tmp->dump();
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
            //uito->dump();
            continue;
        }
        //ifInsModified.clear();
        valueNeedToMod.push_back(valueAlloc);
        //if(insertPtrFromAlloc(valueAlloc))
        //    checkAndDeleteAlloc(valueAlloc);
    }
    while(!valueNeedToMod.empty())
    {
        Value* valueAlloc = valueNeedToMod.back();
        valueNeedToMod.pop_back();
        insertPtrFromAlloc(valueAlloc, F);
    }
    if(ifod)
        return true;
    //insert nullify instructions for dangling pointers.
    for(SetVector<CallInst*>::iterator citb = freePointer.begin(), cite = freePointer.end(); citb != cite; ++citb)
    {
        if(Function* calledFunction = dyn_cast<Function>((*citb)->getCalledValue()->stripPointerCasts()))
        {
            if(calledFunction->getName() == "free")
            {
                if(LoadInst* LI = dyn_cast<LoadInst>((*citb)->getArgOperand(0)))
                {
                    Value* tmpP = LI->getPointerOperand();
                    PointerType* tmpPT = dyn_cast<PointerType>(LI->getType());
                    if(tmpPT == nullptr)
                        continue;
                    ConstantPointerNull* CPN = ConstantPointerNull::get(tmpPT);
                    StoreInst* SI = new StoreInst(CPN, tmpP);
                    SI->insertAfter((*citb));
                }
            }
        }
    }
    return true;
}

bool NoDang::insertNullify(Value *v)
{
    return true;
}


bool NoDang::insertPtrFromAlloc(Value *value, Function &F)
{
    if(ifModified.find(value) != ifModified.end())
        return false;
    output("------------------Modify begin--------------");
    AllocaInst *modifiedValue;
    Value *middleValue;
    Twine newValName1 = value->getName()+"_1";
    Instruction* insPos = nullptr;
    if(AllocaInst *valueAlloc = dyn_cast<AllocaInst>(value))
    {
        Type *ttmp = valueAlloc->getAllocatedType();
        insPos = valueAlloc;
        modifiedValue = new AllocaInst(valueAlloc->getType(), newValName1);
    }
    else if(GlobalVariable *valueGlobal = dyn_cast<GlobalVariable>(value))
    {
        Type *ttmp = valueGlobal->getType();
        ttmp = ttmp->getPointerElementType();
        insPos = F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
        modifiedValue = new AllocaInst(valueGlobal->getType(), newValName1);
 
    }
    else if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(value))
    {
        //not easy to modify struct element.
        if(GEPI->getNumIndices() < 3)
            return false;
        insPos = F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
        modifiedValue = new AllocaInst(GEPI->getType(), newValName1);
    }
    else
    {
        return false;
        //assert(0 && "Exception: Unknow type of value.");
    }
    modifiedValue->insertAfter(insPos);
    //begin to insert a pointer between them.
    middleValue = value;
    output(middleValue);
    output(modifiedValue);
    
    StoreInst* SI = new StoreInst(middleValue, modifiedValue);
    SI->insertAfter(modifiedValue);
    ifInsModified[SI] = nullptr;
    if(ifod)
        if(funIns.find(&F) == funIns.end())
            funIns[&F] = SI;

    
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
            {
                //uib->dump();
            }
            return false;
        }
    }
    else
        assert(0 && "Exception: Other ins");
}

Function* NoDang::ifUseOutsideFunction(Value *value, Value** modifiedPointer, Value* middlePointer, Value* oriP, Function &F)
{
    Function* newF = &F;
    if(Instruction *I = dyn_cast<Instruction>(value))
    {
        newF = I->getParent()->getParent();
        if(newF == &F)
        {
            return newF;
        }
        Value* valDef = oriP;
        AllocaInst *newAI = nullptr;
        if(GlobalVariable *valueGlobal = dyn_cast<GlobalVariable>(valDef))
        {
            output("ifUseOutsideFunction");
            Instruction* insPos = newF->getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
            Twine newValName1 = valueGlobal->getName()+"_1";
            newAI = new AllocaInst(valueGlobal->getType(), newValName1, insPos);
            StoreInst* SI = new StoreInst(middlePointer, newAI, insPos);
            *modifiedPointer = newAI;
        }
        else if(GEPOperator *GEPO = dyn_cast<GEPOperator>(valDef))
        {
            Instruction* insPos = newF->getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
            newAI = new AllocaInst(GEPO->getType(), "allocForGEPOIFO", insPos);
            StoreInst* SI = new StoreInst(GEPO, newAI, insPos);
            *modifiedPointer = newAI;
            
        }
        else if(AllocaInst *valueAlloc = dyn_cast<AllocaInst>(valDef))
        {
            return newF;
        }
        else
        {
            output(valDef);
            assert(0 && "aaaaaaaaaaaaaaa");
        }
        ifModified[newAI] = nullptr;
    }
    return newF;
}

bool NoDang::insertPtrForValue(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI)
{
    if(uib == nullptr || ifInsModified.find(uib) != ifInsModified.end())
    {
        return false;
    }
    if(lastins)
        output("Recursive insert");
    else
        output("begin to insert");
    bool res;
    Function *newF = ifUseOutsideFunction(uib, &modifiedPointer, middlePointer, oriP, F);

    if(ifod)
        res = insertPtrForValueOD(uib, modifiedPointer, middlePointer, oriP, lastins, *newF, ifGEPI);
    else
        res = insertPtrForValueDang(uib, modifiedPointer, middlePointer, oriP, lastins, *newF, ifGEPI);

    numOfInsMod++;
    if(lastins)
        output("Recursive insert end.");
    else
        output("begin to insert end.");
    return res;
}

void NoDang::cloneIns(Instruction* ins, Function& F)
{
    Instruction* newIns = ins->clone();
    newIns->insertAfter(ins);
    DenseMap<Function*, Instruction*>::iterator it = funIns.find(&F);
    if(it == funIns.end())
    {
        return;
        assert(0 &&"wwwwwwwwwwwwwwwoooooooooooooooooowwwwwwwwwwwww");
    }
    else
    {
        Instruction* newIns2 = it->second->clone();
        newIns2->insertAfter(newIns);
    }
    ifInsModified[newIns] = newIns;
}

bool NoDang::insertPtrForValueOD(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI)
{
    output(uib);
    Value* targetOper = uib;
    Value *newLI = nullptr;
    if(LoadInst* LI = dyn_cast<LoadInst>(targetOper))
    {
        cloneIns(LI, F);
        insertPtrForValue(LI->user_back(),modifiedPointer,middlePointer,oriP,lastins, F, true);
        newLI = new LoadInst(modifiedPointer, "newaddLoad", LI);
    }
    else if(StoreInst* SI = dyn_cast<StoreInst>(targetOper))
    {
        Value* otherOper = nullptr;
        Value* curOper = nullptr;
        bool ifCurLeft = getOtherOper(SI, lastins, oriP, &curOper, &otherOper);
        Value* rootDef = getRootDef(otherOper);
        bool ifClo = true;
        
        if(rootDef == nullptr)
        {
            return false;
        }
        if(isa<CallInst>(rootDef) || isa<Argument>(rootDef))
        {
            return false;
        }
        if(ifCurLeft)
        {
            if(!isa<PointerType>(curOper->getType()))
                ifClo = false;
        }
        else
        {
            if(!isa<PointerType>(otherOper->getType()))
                ifClo = false;
        }
        if(ifClo)
            cloneIns(SI, F);
        else
            newLI = new LoadInst(modifiedPointer, "newaddStore", SI);
            
        if(isa<AllocaInst>(rootDef) || isa<GlobalVariable>(rootDef))
        {
            if(ifModified.find(rootDef) == ifModified.end())
            {
                valueNeedToMod.push_back(rootDef);
            }
        }
        ifInsModified[SI] = SI;

    }
    else if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(targetOper))
    {
        cloneIns(GEPI, F);
        insertPtrForValue(GEPI->user_back(),modifiedPointer,middlePointer,oriP,lastins, F, true);
    }
    else if(GEPOperator* GEPO = dyn_cast<GEPOperator>(targetOper))
    {
        Instruction *insPos = F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
        AllocaInst *newAI = new AllocaInst(GEPO->getType(), "allocForGEPO");
        ifModified[newAI] = nullptr;
        newAI->insertAfter(insPos);
        StoreInst* SI = new StoreInst(GEPO, newAI);
        SI->insertAfter(newAI);
        ifInsModified[SI] = nullptr;
        newLI = SI;
        for(Value::user_iterator uib = GEPO->user_begin(), uie = GEPO->user_end(); uib != uie; ++uib)
        {
            Instruction *gepub = dyn_cast<Instruction>(*uib);
            insertPtrForValue(gepub, newAI, gepub, GEPO, nullptr, F, false);
        }
    }
    if(newLI != nullptr)
        ifInsModified[newLI] = newLI;
    return true;
}

bool NoDang::insertPtrForValueDang(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins, Function &F, bool ifGEPI)
{
    output(uib);
    //TODO: modifiy struct code
    Value* targetOper = uib;
    output("modifiedPointer is:");
    output(modifiedPointer);
    output("middlePointer is:");
    output(middlePointer);
    if(LoadInst* LI = dyn_cast<LoadInst>(targetOper))
    {
        LoadInst *newLI = new LoadInst(modifiedPointer, "", LI->isVolatile(), LI);
        ifInsModified[LI] = newLI;
        if(!insertPtrForValue(LI->user_back(), newLI, middlePointer, oriP, LI, F, ifGEPI))
        {
            if(ifGEPI)
                LI->replaceAllUsesWith(newLI);
            else
            {
                LoadInst *targetLI = new LoadInst(newLI, "", LI->isVolatile(), LI);
                output(targetLI);
                LI->replaceAllUsesWith(targetLI);
            }
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
        if(rootDef == nullptr)
        {
            return false;
        }
        output("rootdef is:");
        output(rootDef);
        output("otherOper is:");
        output(otherOper);
        if(handleStructEle(otherOper, rootDef, middlePointer, oriP, lastins))
            return true;
        if(isa<CallInst>(rootDef) || isa<Argument>(rootDef))
        {
            //we don't modify argument and regard it as call
            output("CallInst or Argument");
            StoreInst *middleSI = nullptr, *targetSI = nullptr;
            if(!ifGEPI)
            {
                Value* middleP = middlePointer;
                if(lastins != nullptr)
                {
                    middleP = new LoadInst(middlePointer, "", SI);
                }
                output(otherOper);
                output(middleP);
                middleSI = new StoreInst(otherOper, middleP, SI->isVolatile(), SI);
                //targetSI = new StoreInst(middlePointer, modifiedPointer, SI->isVolatile(), SI);
             
                output(middleSI);
                output(targetSI);
                SI->eraseFromParent();
            }
            else
            {
                /*
                %call = call noalias i8* @malloc(i64 10) #3
                %arrayidx = getelementptr inbounds [10 x i8*]* %ap, i32 0, i64 0
                store i8* %call, i8** %arrayidx, align 8
                */
                lastins->replaceAllUsesWith(modifiedPointer);
                ifUsedAsExpect = true;
            }
        }
        else if(ifNotPointer(rootDef))
        {
            output("Constant");
            if(ifGEPI)
                lastins->replaceAllUsesWith(modifiedPointer);
            else
            {
                StoreInst* targetSI = nullptr;
                if(!ifCurLeft)
                {
                    LoadInst* middleLI = new LoadInst(modifiedPointer, "", SI->isVolatile(), SI);
                    output(middleLI);
                    targetSI = new StoreInst(otherOper, middleLI, SI->isVolatile(), SI);
                }
                else
                {
                    /*
                    %30 = load i32** %idx, align 8, !tbaa !1
                    %32 = load i32* %30, align 4, !tbaa !6
                    store i32 %32, i32* %i, align 4, !tbaa !6
                    */
                    LoadInst* LItmp = new LoadInst(modifiedPointer, "", SI);
                    targetSI = new StoreInst(LItmp, otherOper, SI->isVolatile(), SI);
                }
                output(targetSI);
                SI->eraseFromParent();
            }
        }
        else if(isa<AllocaInst>(rootDef) || isa<GlobalVariable>(rootDef))
        {
            if(ifModified.find(rootDef) == ifModified.end())
            {
                valueNeedToMod.push_back(rootDef);
                output("add rootdef to stack");
                return true;
                //insertPtrFromAlloc(rootDef, F);
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
                        Value* newLI1 = nullptr, *newLI2 = nullptr;
                        if(isa<GetElementPtrInst>(it->second))
                        {
                            newLI1 = it->second;
                        }
                        else
                        {
                            newLI1 = new LoadInst(it->second, "", SI->isVolatile(), SI);
                        }
                        if(ifGEPI)
                            newLI2 = modifiedLastIns;
                        else
                            newLI2 = new LoadInst(modifiedLastIns, "", SI->isVolatile(), SI);
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
                        insertPtrForValue(backUse, ifAllocModified->second, rootDef, rootDef, nullptr, F, false);
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
                        insertPtrForValue(backUse, ifAllocModified->second, rootDef, rootDef, nullptr, F, false);

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
                    //storeInstValue->getType()->dump();
                    //cast<PointerType>(storeInstPointer->getType())->getElementType()->dump();
                    if(storeInstValue->getType() == cast<PointerType>(storeInstPointer->getType())->getElementType())
                    {
                        StoreInst* newSI = new StoreInst(storeInstValue, storeInstPointer, SI->isVolatile(), SI);
                        output(newSI);
                        //TODO: This store instruction is not removed in order to keep variable scope.
                        //SI->eraseFromParent();
                    }
                    else
                    {
                        /*TODO: only handle this situation
                          %39 = load i8** %command, align 8
                          %add.ptr = getelementptr inbounds i8* %39, i64 10
                          store i8* %add.ptr, i8** %command, align 8
                        */
                        LoadInst* tmpLI = new LoadInst(storeInstPointer, "", SI);
                        StoreInst* newSI = new StoreInst(storeInstValue, tmpLI, SI->isVolatile(), SI);
                        output(newSI);
                        SI->eraseFromParent();
                    }

                }
            }
        }
        else
        {
            //rootDef->dump();
            assert(0 && "Exception: rootdef unhandled");
        }
    }
    else if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(targetOper))
    {
        //Used to skip GetElementPtrInst for struct.
        //TODO: this should be improved to really modify element in struct instead of just inserting a loadinst.
        if(cast<PointerType>(GEPI->getPointerOperandType())->getElementType()->isStructTy())
        {
            errs()<<"Skip this ins\n";
            LoadInst *uselessLI = new LoadInst(modifiedPointer, "useless", GEPI);
            //StoreInst *uselessSI = new StoreInst(uselessLI, modifiedPointer, false, GEPI);
            ifInsModified[uib] = nullptr;
            ifInsModified[uselessLI] = nullptr;
            output(uselessLI);
            return true;
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
        Value* newV = modifiedPointer;
        if(!ifGEPI)
        {
            newV = new LoadInst(modifiedPointer, "", GEPI);
        }
        GetElementPtrInst* newGEPI = detNCreateNewGEPI(GEPI, newV);
        ifInsModified[GEPI] = newGEPI;
        if(!insertPtrForValue(GEPI->user_back(), newGEPI, middlePointer, oriP, GEPI, F, true))
        {
            GEPI->replaceAllUsesWith(newGEPI);
        }
        if(GEPI->use_empty())
            GEPI->eraseFromParent();
        else if(!ifUsedAsExpect)
        {
            output(GEPI);
            output(GEPI->user_back());
            output("Exception:  LI should be able to erase in GEPInst.");
            //assert(0 && "Exception: LI should be able to erase in GEPInst.");
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
                    tarPointer = new LoadInst(modifiedPointer, "unused", gepub);
                    output(tarPointer);
                    ifSkip = true;
                    continue;
                }
            }
            LoadInst *newLI = new LoadInst(modifiedPointer, "unused", gepub);
            /*
            GetElementPtrInst* middleGEPI =  GetElementPtrInst::Create(newLI, makeArrayRef(idextmp), "GlobalArrayidx", gepub);
            middleGEPI->setIsInBounds(GEPO->isInBounds());
            if(pre == gepub)
            {
                assert(0 && "Exception: Dead loop in GEP.");
            }
            //use ifuseooutsideF to create the alloc,
            insertPtrForValue(gepub, modifiedGEPI, middleGEPI, GEPO, nullptr, nullptr, ifGEPI);
            //insertPtrForValue(gepub, modifiedGepo, middleGepo, pointLevel);
            pre = gepub;
            */
        }
        //if(!ifSkip)
        //    GEPO->dropAllReferences();
    }
    else
        return false;
    if(ifInsModified.find(uib) == ifInsModified.end())
        ifInsModified[uib] = nullptr;
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

bool NoDang::handleStructEle(Value *uib, Value* modifiedPointer, Value* middlePointer, Value* oriP, Value* lastins)
{
    if(GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(uib))
    {
        if(cast<PointerType>(GEPI->getPointerOperandType())->getElementType()->isStructTy())
        {
            output("Skip this ins");
            LoadInst *uselessLI = new LoadInst(modifiedPointer, "useless", GEPI);
            ifInsModified[uib] = nullptr;
            ifInsModified[uselessLI] = nullptr;
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
//static RegisterPass<NoDang>Y("NoDang", "Add one level pointer to pointers");

static void registerNoDangPass(const PassManagerBuilder &,
                           PassManagerBase &PM) {
    PM.add(new NoDang());
}

static RegisterStandardPasses RegisterNoDangPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerNoDangPass);

//static RegisterStandardPasses RegisterNoDangPass0(
//    PassManagerBuilder::EP_EnabledOnOptLevel0, registerNoDangPass);

