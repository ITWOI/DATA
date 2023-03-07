
#include "datarace.h"

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

class GlobalVarDecl : public ASTConsumer, public RecursiveASTVisitor<GlobalVarDecl> {
public :

	void HandleTranslationUnit(ASTContext &Context) override {
		TranslationUnitDecl *decl = Context.getTranslationUnitDecl();
		TraverseDecl(decl);
	}
	bool VisitFunctionDecl(FunctionDecl *E)
	{
		functions[E->getQualifiedNameAsString()] = E;
		if(E->getQualifiedNameAsString() == "init_module")
		{
			const AliasAttr *AA = E->getAttr<AliasAttr>();
			if(AA == nullptr)
				return true;
			entryFun = AA->getAliasee().str();
		}
		else if(E->getQualifiedNameAsString() == "cleanup_module")
		{
			const AliasAttr *AA = E->getAttr<AliasAttr>();
			if(AA == nullptr)
				return true;
			exitFun = AA->getAliasee().str();

		}
		return true;
	}

	bool VisitVarDecl(VarDecl *D)
	{
        SourceManager *sm = &(D->getASTContext().getSourceManager());
        std::string loc = D->getLocStart().printToString(*sm);
        //if(D->getASTContext().getSourceManager().isInMainFile(D->getLocation()))
        //    return true;

        // TODO: This is ad-hoc.
        if(loc.find("/lib/modules/") != string::npos || loc.find("/usr/src/") != string::npos) 
            return true;
		if(D->isLocalVarDecl())
			return true;
		if(const ParmVarDecl* pv=dyn_cast<ParmVarDecl>(D))
			return true;
		SVs.insert(D);

        // This is used to handle device accesses.
        
        if(!D->hasInit())
            return true;
        const Expr *expr = D->getInit();
        if(const InitListExpr *ILE = dyn_cast<InitListExpr>(expr))
        {
            for(int i = 0; i<ILE->getNumInits(); i++)
            {
                const Expr *initEle = ILE->getInit(i);
                if(const ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(initEle))
                {
                    if(ICE->getCastKind() != CK_FunctionToPointerDecay)
                        continue;
                    // Note: We can not directly get the assigned function pointer, but the
                    // order of init is the same as the order of struct definition.
                    const Type *type = D->getType().getTypePtr();
                    if(const RecordType *RT = type->getAsStructureType())
                    {
                        const RecordDecl *RD = RT->getDecl();
                        int j = 0;
                        for(RecordDecl::field_iterator fit = RD->field_begin(); fit != RD->field_end(); fit++)
                        {
                            if(j == i)
                            {
                                if(const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(ICE->getSubExpr()))
                                {
                                    if(const FunctionDecl *fun = dyn_cast<FunctionDecl>(DRE->getDecl()))
                                        opers[fun] = *fit;
                                }
                            }
                            j++;
                        }
                    }
                }
            }
        }
		return true;
		
	}

	const FunctionDecl* getEntryFun(){
		std::unordered_map<std::string, const FunctionDecl*>::iterator got = functions.find(entryFun);
		if(got == functions.end())
			return nullptr;
		return got->second;
	}

	const FunctionDecl* getExitFun(){
		std::unordered_map<std::string, const FunctionDecl*>::iterator got = functions.find(exitFun);
		if(got == functions.end())
			return nullptr;
		return got->second;
	
	}
	void getSVs(std::set<VarDecl*> &extSVs){
		for(VarDecl* SV : SVs)
			extSVs.insert(SV);
	}

	std::unordered_map<const FunctionDecl*, const FieldDecl*> getOpers(){
        return opers;
	}

private:
	std::unordered_map<std::string, const FunctionDecl*> functions;
	std::string entryFun, exitFun;
	std::set<VarDecl*> SVs;
    std::unordered_map<const FunctionDecl*, const FieldDecl*> opers;
};

void DataRace::check(){
	fout.open("GuidedPath.txt");
	readConfig();
	getEntryFunc();
	getISRs();

	//get all functions including ISRs and their callee.
	std::vector<ASTFunction*> tarFuns;
    std::vector<ASTFunction*> ISRsVect;
    std::vector<ASTFunction*> desFuns;
	for(auto isr : ISRs)
	{
	    std::vector<ASTFunction*> tmpFuns;
        tmpFuns.push_back(isr.first);
        tarFuns = getNonTopoOrder(tmpFuns);
        std::string funName = manager->getFunctionDecl(isr.first)->getQualifiedNameAsString();
        ISRsMap[funName] = tarFuns;
        // The above codes can be optimized.

		ISRsVect.push_back(isr.first);
	}
    tarFuns = getNonTopoOrder(ISRsVect);
	analyzeISR(tarFuns);

	IIRCFGConstruct(funs);
    
    /*
	std::vector<ASTFunction*> topLevelFuncs = call_graph->getTopLevelFunctions();
    // analyze tasks
    for(std::set<ASTFunction*>::iterator it1 = tasks.begin(); it1 != tasks.end(); ++it1)
    {
        std::set<ASTFunction*>::iterator it2 = it1;
        it2++;
        while(it2 != tasks.end())
        {
            tarFuns.push_back(*it1);
            tarFuns = getNonTopoOrder(tarFuns);
            desFuns.push_back(*it2);
            desFuns = getNonTopoOrder(desFuns);
            //analyzeISR(tarFuns);
            //IIRCFGConstruct(desFuns);
            ++it2;
        }
    }
    */
    analyzeCallees();
	writeFunInfo();
}

void DataRace::writeFunInfo()
{
	std::ofstream isrFile, accessFile;
	isrFile.open("ISRInfo.txt");
	if(!isrFile)
        return;
    isrFile<<ISRsInfo<<endl;
	isrFile.close();
    accessFile.open("accessInfo.txt");
    if(!accessFile)
        return;
    for(auto eles:SVAccesses)
    {
        for(auto ele:eles.second)
        {
            std::string outS = eles.first + "," + ele.SV + "," + ele.accessType + ","\
                               + ele.func + "\n";
            accessFile<<outS;
        }
    }
    accessFile.close();
	fout.close();
}

void DataRace::getEntryFunc()
{
	std::vector<ASTFile *> astFiles = resource->getASTFiles();
	for(ASTFile * astFile : astFiles)
	{
		ASTUnit * astUnit = manager->getASTUnit(astFile);
		ASTContext& AST = astUnit->getASTContext();
		GlobalVarDecl globVar;
		globVar.HandleTranslationUnit(AST);
		if(globVar.getEntryFun() != nullptr)
			entryFunc = call_graph->getFunction(const_cast<FunctionDecl*>(globVar.getEntryFun()));
		if(globVar.getExitFun() != nullptr)
			exitFunc = call_graph->getFunction(const_cast<FunctionDecl*>(globVar.getExitFun()));
        opers = globVar.getOpers();
		//globVar.getSVs(SVs);
        std::set<VarDecl*> extSVs;
		globVar.getSVs(extSVs);
        numOfSVs = extSVs.size();
	}

}

void DataRace::getISRs()
{
	std::vector<ASTFunction*> topLevelFuncs = call_graph->getTopLevelFunctions();
	funs = getNonTopoOrder(topLevelFuncs);
	LangOptions LangOpts;
	LangOpts.CPlusPlus = true;
	PrintingPolicy Policy(LangOpts);
    for(auto fun:funs)
    {
	    std::unique_ptr<CFG>& cfg = manager->getCFG(fun);
        for(CFG::iterator iter=cfg->begin();iter!=cfg->end();++iter)
        {
            numOfBranches++;
            CFGBlock* block=*iter;
            for(CFGBlock::iterator iterblock=block->begin();iterblock!=block->end();++iterblock)
            {
                CFGElement element=(*iterblock);
                if(element.getKind()!=CFGElement::Kind::Statement)
                    continue;
                const Stmt* it=((CFGStmt*)&element)->getStmt();
                if(it == nullptr || !isa<CallExpr>(it))
                    continue;
                /*
                //duplicated
                if(const BinaryOperator* BO = dyn_cast<BinaryOperator>(it))
                {
                IRCFGConstruct()				it = BO->getRHS();
                }*/
                if(const CallExpr *tmp = dyn_cast<CallExpr>(it))
                {
                    string TypeS;
                    llvm::raw_string_ostream s(TypeS);
                    tmp->getCallee()->printPretty(s, 0, Policy);
                    if(s.str().find(thread) != std::string::npos)
                    {
                        // find tasks.
                        // if num of args is 1, then it cannot be a task reg fun. But not sure.
                        if(tmp->getNumArgs() <= threadArg+1)
                            continue;
                        const Expr* expr = tmp->getArg(threadArg)->IgnoreImpCasts();
                        if(const DeclRefExpr* dre=dyn_cast<DeclRefExpr>(expr))
                        {
                            const ValueDecl* res = dre->getDecl();
                            const FunctionDecl *task = dyn_cast<FunctionDecl>(res);
                            tasks.insert(call_graph->getFunction(const_cast<FunctionDecl*>(task)));
                        }

                    }
                    if(s.str() != reqFun)
                        continue;
                    const Expr* exprIRL = tmp->getArg(reqIRL)->IgnoreImpCasts();
                    const FunctionDecl* ISR = nullptr;
                    std::string IRL;
                    if(const DeclRefExpr* dre = dyn_cast<DeclRefExpr>(exprIRL))
                    {
                        const ValueDecl* res = dre->getDecl();
                        IRL = res->getQualifiedNameAsString();
                    }
                    else if(const IntegerLiteral* il = dyn_cast<IntegerLiteral>(exprIRL))
                    {
                        std::string TypeS;
                        llvm::raw_string_ostream s(TypeS);
                        il->printPretty(s, 0, Policy);
                        IRL = s.str();
                    }
                    cout<<IRL<<" IRL\n";
                    const Expr* cb = tmp->getArg(reqISR)->IgnoreImpCasts();
                    const Expr* falCod = nullptr;
                    if(const ConditionalOperator* CO = dyn_cast<ConditionalOperator>(cb))
                    {
                        cb = CO->getTrueExpr();
                        falCod = CO->getFalseExpr();
                    }
                    //This while stat is added to process ConditionalOperator
                    while(cb != nullptr)
                    {
                        while(const CastExpr* castExpr = dyn_cast<CastExpr>(cb))
                            cb = castExpr->getSubExpr();
                        if(const DeclRefExpr* dre=dyn_cast<DeclRefExpr>(cb))
                        {
                            const ValueDecl* res = dre->getDecl();
                            ISR = dyn_cast<FunctionDecl>(res);
                            cout<<res->getQualifiedNameAsString()<<" ISR\n";
                        }
                        if(ISR == nullptr)
                            return;
                        ISRsInfo += ISR->getQualifiedNameAsString() + " "+ IRL + "\n";
                        FunctionDecl* entryFunDecl = manager->getFunctionDecl(entryFunc);
                        sm = &(entryFunDecl->getASTContext().getSourceManager());
                        std::string loc = tmp->getLocStart().printToString(*sm);
                        ReqIRQInfo* reqIRQ = new ReqIRQInfo(IRL, entryFunc, loc);

                        ISRs[call_graph->getFunction(const_cast<FunctionDecl*>(ISR))] = reqIRQ;
                        cb = falCod;
                        falCod = nullptr;
                    }
                }
            }
        }
	}
}

//get SVs
void DataRace::analyzeISR(std::vector<ASTFunction*> &tarFuns){
	
	for(ASTFunction* fun : tarFuns)
	{
	    std::unordered_map<CFGBlock*, bool> ifVisited;
	    std::unordered_map<CFGBlock*, std::set<const ValueDecl*>> outBlockLockSet;
	    std::queue<CFGBlock*> cfgStack;
		std::unique_ptr<CFG>& cfg = manager->getCFG(fun);
	    std::set<const ValueDecl*> lockSet;
		LangOptions LangOpts;
		LangOpts.CPlusPlus = true;
        //cfg->dump(LangOpts, true);
		PrintingPolicy Policy(LangOpts);
        cfgStack.push(&(cfg->getEntry()));
        // Init entry block.
        outBlockLockSet[&(cfg->getEntry())] = lockSet;
        // Analyze blocks according to the control flow.
        while(!cfgStack.empty())
		{
            std::set<const ValueDecl*> inLockSet;

            CFGBlock* block = cfgStack.front();
            cfgStack.pop();
            // Skip analyzed functions.
            if(block == nullptr || ifVisited.find(block) != ifVisited.end())
                continue;
            ifVisited[block] = true;
            for(CFGBlock::succ_iterator succBLK = block->succ_begin(); succBLK != block->succ_end(); ++succBLK)
                cfgStack.push(*succBLK);
            addPara(fun);
            // Calculate in state for a block.
            calInState(block, inLockSet, outBlockLockSet);
			for(CFGBlock::iterator iterblock=block->begin();iterblock!=block->end();++iterblock)
			{
				CFGElement element=(*iterblock);
				if(element.getKind()!=CFGElement::Kind::Statement)
					continue;
				const Stmt* it=((CFGStmt*)&element)->getStmt();
                
                if(lockCal(it, inLockSet))
                    continue;

                outBlockLockSet[block] = inLockSet;
                analyzeFunPara(it, fun, block, inLockSet);
				for(auto exprIT: analyseExpr(it, false))
                {
					addGSV(exprIT.first, fun, block, inLockSet, exprIT.second);
					addPSV(exprIT.first, fun, block, inLockSet, exprIT.second);
                }
			}
		}
	}
}

void DataRace::addPara(ASTFunction *astFun)
{
    FunctionDecl *fun = manager->getFunctionDecl(astFun);
    for(auto paraIT = fun->param_begin(); paraIT != fun->param_end(); paraIT++)
    {

        const Type *declType = (*paraIT)->getType().getTypePtr();
        paraSVs[*paraIT] = declType;
    }
        
}

void DataRace::analyzeFunPara(const Stmt* it, ASTFunction* astFun, CFGBlock* CFB, std::set<const ValueDecl*>& curSet)
{
    /**
     * We only need to know the type of a variable, instead of it cast oper.
     * i.e., struct dev *tmp = device; is possible.
     * Therefore, we find these stmt and obtain the type of lhs and determine
     * whether rhs is parameter.
     */

    // Get the casted var.
    const Expr *castExpr = nullptr;
    const Type *declType = nullptr;
    const DeclRefExpr *DRE = nullptr;
    const VarDecl *varDecl = nullptr;
    const VarDecl *VD = nullptr;
    // dev *a; a=(dev *)device;
    if(const BinaryOperator *tmp = dyn_cast<BinaryOperator>(it))
    {
        castExpr = tmp->getRHS();
        const Expr *varDecl = tmp->getLHS();
        if(const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(varDecl))
	        if(VD = dyn_cast<VarDecl>(DRE->getDecl()))
                declType = VD->getType().getTypePtr();

    }
    if(const DeclStmt *tmp = dyn_cast<DeclStmt>(it))
    {
        if(!tmp->isSingleDecl())
            return;
        if(VD = dyn_cast<VarDecl>(tmp->getSingleDecl()))
        {
            declType = VD->getType().getTypePtr();
            castExpr = VD->getInit();
        }
    }
    if(castExpr == nullptr || declType == nullptr)
        return;
    DRE = getVarDecl(castExpr, false, nullptr);
	if(DRE == nullptr)
		return;
    if(varDecl = dyn_cast<VarDecl>(DRE->getDecl()))
    {
        if(const ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(varDecl))
        {
            paraSVs[VD] = declType;
            /*
               ISRInfo tmp(DRE, astFun, CFB, curSet);
               std::vector<ISRInfo> DREV;
               DREV.push_back(tmp);
               SVPs[declType] = DREV;
               */
        }
        else
            return;
    }
}

void DataRace::calInState(CFGBlock* block,  std::set<const ValueDecl*>& inLockSet, std::unordered_map<CFGBlock*, std::set<const ValueDecl*>> outBlockLockSet)
{
    for(CFGBlock::pred_iterator predBLK = block->pred_begin(); predBLK != block->pred_end(); ++predBLK)
    {
        std::set<const ValueDecl*> tmpSet =  outBlockLockSet[*predBLK];
        if(predBLK == block->pred_begin())
        {
            inLockSet = tmpSet;
            continue;
        }
        set_intersection(inLockSet.begin(), inLockSet.end(), tmpSet.begin(), tmpSet.end(), std::inserter(inLockSet,inLockSet.begin()));
    }
}

int DataRace::readConfig(){
	std::unordered_map<std::string, std::string> ptrConfig = configure->getOptionBlock("DataRace");
	enableFun = ptrConfig.find("enable")->second;
	disableFun = ptrConfig.find("disable")->second;
	reqFun = ptrConfig.find("request")->second;
	enableArg = stoi(ptrConfig.find("enable_arg")->second);
	disableArg = stoi(ptrConfig.find("disable_arg")->second);
    // thread is a part of name, e.g., kthread_run, kthread_create
    thread = ptrConfig.find("thread")->second;
    threadArg = stoi(ptrConfig.find("thread_arg")->second);

	reqIRL = stoi(ptrConfig.find("request_IRL")->second);
	reqISR = stoi(ptrConfig.find("request_ISR")->second);
    lockInfos = configure->getOptionBlock("LockInfo");
    for(auto tmp:lockInfos)
    {
        unlockInfos[tmp.second] = tmp.first;
    }
	return 0;
}

const DeclRefExpr* DataRace::getVarDecl(const Expr *expr, bool ifNeedGlobal, ValueDecl **member)
{
	const Expr *exprTmp=expr;
    while(true)
    {
        while(isa<CastExpr>(exprTmp) || isa<ParenExpr>(exprTmp))
        {
            if(const CastExpr * it = dyn_cast<CastExpr>(exprTmp))
                exprTmp=it->getSubExpr();
            if(const ParenExpr* it = dyn_cast<ParenExpr>(exprTmp))
                exprTmp=it->getSubExpr();
        }
        if(const MemberExpr* ME = dyn_cast<MemberExpr>(exprTmp))
        {
            if(member != nullptr)
                *member = ME->getMemberDecl();
            exprTmp = ME->getBase();
        }
        else if(const ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(exprTmp))
        {
            exprTmp = AS->getBase();
        }
        else if(const UnaryOperator* UO = dyn_cast<UnaryOperator>(exprTmp))
        {
            exprTmp = UO->getSubExpr();
        }
        else if(const DeclRefExpr* declRef=dyn_cast<DeclRefExpr>(exprTmp))
        {
            if(!ifNeedGlobal)
                return declRef;
            const ValueDecl *valueDecl = declRef->getDecl();
            if(const VarDecl* varDecl = dyn_cast<VarDecl>(valueDecl))
            {
                if(varDecl->isLocalVarDecl() )
                    return nullptr;
                if(const ParmVarDecl* pv=dyn_cast<ParmVarDecl>(valueDecl))
                    return nullptr;
                return declRef;
            }
            else
                return nullptr;
        }
        else
            return nullptr;
    }
	return nullptr;

}

void DataRace::addPSV(const Expr *expr, ASTFunction* astFun, CFGBlock* CFB, std::set<const ValueDecl*>& curSet, std::string accessType)
{
    // This is used to handle parameter SVs.
    ValueDecl *valDecl = nullptr;
	const DeclRefExpr* declRef = getVarDecl(expr, false, &valDecl);
    // expr should be a struct and should has a member.
    if(declRef == nullptr || valDecl == nullptr)
        return;
	const VarDecl* varDecl = cast<VarDecl>(declRef->getDecl());
    auto paraSVsIt = paraSVs.find(varDecl);
    if(paraSVsIt == paraSVs.end())
        return;

	ISRInfo tmp(declRef, astFun, CFB, curSet, valDecl, accessType);

    auto SVPsIt = SVPs.find(paraSVsIt->second);
	if(SVPsIt == SVPs.end())
    {
		std::vector<ISRInfo> DREV;
		DREV.push_back(tmp);
		SVPs[paraSVsIt->second] = DREV;
    }
    else
    {
        SVPsIt->second.push_back(tmp);
    }

}

void DataRace::addGSV(const Expr *expr, ASTFunction* astFun, CFGBlock* CFB, std::set<const ValueDecl*>& curSet, std::string accessType)
{
    // This is used to handle global SVs.
    ValueDecl *valDecl = nullptr;
	const DeclRefExpr* declRef = getVarDecl(expr, true, &valDecl);
	if(declRef == nullptr)
		return;
	const VarDecl* varDecl = cast<VarDecl>(declRef->getDecl());
	ISRInfo tmp(declRef, astFun, CFB, curSet, valDecl, accessType);
	auto it = SVs.find(varDecl);
	if(it == SVs.end())
	{
		std::vector<ISRInfo> DREV;
		DREV.push_back(tmp);
		SVs[varDecl] = DREV;
	}
	else
	{
		it->second.push_back(tmp);
	}
}


void DataRace::analyzeOnePair(const Expr *expr, ASTFunction* fun, std::string accessType, ISRInfo &pair, std::string out, std::set<const ValueDecl*>& curSet, ValueDecl *valDecl, const DeclRefExpr *declRef)
{
    //if the IRL of premeted ISR is large than current ISR, it's possbile for race.
    std::set<const ValueDecl*> interSet;
    auto SVit = ISRs.find(fun);
    auto ISRit = ISRs.find(pair.fun);
    if(ISRit != ISRs.end())
    {
        if(SVit != ISRs.end())
        {
            int irl1 = atoi(SVit->second->IRL.c_str());
            int irl2 = atoi((ISRit->second)->IRL.c_str());
            if(irl1 != 0 && irl2 != 0 && irl1 <= irl2)
                return;
        }
        // Consider the situation when ISR is happend befor requestIRQ, which is impossible for race.
        if(ifRaceBeforeReqIRQ(expr, fun, ISRit->second))
            return;
    }
    // One of the two accesses must be write.
    if(accessType == "read" && pair.accessType == "read")
        return;
    // If the member is right. i.e., ele->mem.
    if(valDecl != nullptr && valDecl != pair.memExpr)
        return;
    if(fun == pair.fun)
        return;
    // Lock set intersection.
    set_intersection(curSet.begin(), curSet.end(), pair.lockSet.begin(), pair.lockSet.end(), std::inserter(interSet,interSet.begin()));
    if(!interSet.empty())
        return;

    std::string output = out;
    output += getExePath(pair.CFB, pair.expr) + "\n";
    writingFile(output, fun, pair.fun);
}

std::string DataRace::getISRName(ASTFunction *fun)
{
    for(auto isr:ISRsMap)
    {
        if(find(isr.second.begin(), isr.second.end(), fun) != isr.second.end())
            return isr.first;
    }
    return manager->getFunctionDecl(fun)->getQualifiedNameAsString();
}

int DataRace::SVPAnalysis(const Expr *expr, CFGBlock* CFB, unsigned int index, ASTFunction* fun, std::string accessType, std::set<const ValueDecl*> curSet)
{
    /*
     * Compare with variables in tasks, we determine whether the var is SV
     * by determine its type. i.e., if the type is the same as the type in SVP,
     * then it's SV.
     */
    ValueDecl *valDecl = nullptr;
	const DeclRefExpr* declRef = getVarDecl(expr, false, &valDecl);
    if(declRef == nullptr)
		return 0;
    if(valDecl == nullptr)
        return 0;
	const VarDecl* varDecl = dyn_cast<VarDecl>(declRef->getDecl());
    if(varDecl == nullptr)
        return 0;
    const Type * declType = varDecl->getType().getTypePtr();

	auto it = SVPs.find(declType);
	if(it == SVPs.end())
		return 0;

    std::string out;
    // Function getExePath may not compatitable with clang 3.6.2.
	out = varDecl->getQualifiedNameAsString() + ": SVP\n" + getExePath(CFB, declRef) + "\na.c:0\n";
	//std::string path = getStmtLoc(declRef);
	//out = varDecl->getQualifiedNameAsString() + "\n";
    recordAccessInfo(expr, varDecl, accessType, fun);

	for(auto pair : it->second)
    {
        analyzeOnePair(expr, fun, accessType, pair, out, curSet, valDecl, declRef);
    }
}

int DataRace::SVAnalysis(const Expr *expr, CFGBlock* CFB, unsigned int index, ASTFunction* fun, std::string accessType, std::set<const ValueDecl*> curSet)
{
    ValueDecl *valDecl = nullptr;
	const DeclRefExpr* declRef = getVarDecl(expr, true, &valDecl);
	if(declRef == nullptr)
		return 0;
	const VarDecl* varDecl = cast<VarDecl>(declRef->getDecl());
	auto it = SVs.find(varDecl);
	if(it == SVs.end())
		return 0;
	//ignore races in the same function
	
	std::string out;
	out = varDecl->getQualifiedNameAsString() + ": SVS\n" + getExePath(CFB, declRef) + "\na.c:0\n";
    recordAccessInfo(expr, varDecl, accessType, fun);
	for(auto pair : it->second)
	{
        analyzeOnePair(expr, fun, accessType, pair, out, curSet, valDecl, declRef);
    }

	return 1;
}

void DataRace::recordAccessInfo(const Expr *expr, const VarDecl* sv, std::string accessType, ASTFunction* func)
{
    std::string line = getStmtLoc(expr);
    line = line.substr(line.find(" ")+1);
    std::string svName = sv->getQualifiedNameAsString();
    std::string funName = manager->getFunctionDecl(func)->getQualifiedNameAsString();
    auto it = SVAccesses.find(line);
    if(it == SVAccesses.end())
    {
        std::vector<SVAI> tmp;
        SVAI tmpInfo(svName, accessType, funName);
        tmp.push_back(tmpInfo);
        SVAccesses[line] = tmp;
    }
    else
    {
        for(auto ele:it->second)
        {
            //only support single file....
            if(ele.SV == svName && ele.accessType == accessType && ele.func == funName)
                return;
        }
        SVAI tmpInfo(svName, accessType, funName);
        it->second.push_back(tmpInfo);
    }
}

bool DataRace::ifRaceBeforeReqIRQ(const Expr *expr, ASTFunction* fun, ReqIRQInfo* reqInfo)
{
	//FIXME: should consider call graph begin from entryfun instead of only considering entryfun
	if(fun == reqInfo->fun)
		return true;
	return false;
}

void DataRace::writingFile(std::string str, ASTFunction *astFunc, ASTFunction *isrFun)
{
    FunctionDecl *funDecl = manager->getFunctionDecl(astFunc);
    // Find the corresponding accesses name if exist.
    auto opersIt = opers.find(funDecl);
    if(opersIt != opers.end())
    {
        str += opersIt->second->getNameAsString() + "\n";
    }
    str += getISRName(isrFun) + "\n";

	std::ostringstream oss;
	oss<<warningCount;
	std::string file = "GuideSrc.txt" + oss.str();
	cout<<file<<endl;
	/*std::ofstream fout1;
	fout1.open(file);
	if(!fout1)
		return;
	//change SV name to Info:
	int pos = str.find('\n');
	std::string str1 = "Info:" + str.substr(pos);
	fout1<<str1<<endl;
	fout1.close();*/
	if(!fout)
		return;
	fout<<str<<endl;
	warningCount++;
}

std::string DataRace::getStmtLoc(const Stmt* stmt)
{
	std::string loc = stmt->getLocStart().printToString(*sm);
	int pos1 = loc.find_last_of('<');
	std::string str, tmpLine, file;
	if(pos1 == -1)
	{
		pos1 = loc.find_last_of(':');
	}
	else
	{
		loc.assign(loc.c_str(),pos1);
		pos1 = loc.find_last_of(':');
	}
	str.assign(loc.c_str(),pos1);
	int pos2=str.find_last_of(':');
	file.assign(loc.c_str(),pos2);
	int pos3 = str.find_last_of('/');
	file.assign(file.c_str(), pos3 + 1, file.size());
	tmpLine = str.substr(pos2+1);
	return file + " " + tmpLine;
}


std::string DataRace::getExePath(CFGBlock* CFB, const DeclRefExpr* declRefExpr)
{
	std::string path = getStmtLoc(declRefExpr);
	CFGBlock* CFBTmp = CFB;
	std::unordered_map<CFGBlock*, Color> colors;
	std::vector<CFGBlock*> blocks;
	for(CFGBlock::iterator iterblock = CFB->begin(); iterblock != CFB->end();++iterblock)
	{
		CFGElement element=(*iterblock);
		if(element.getKind()!=CFGElement::Kind::Statement)
			continue;
		const Stmt* it = ((CFGStmt*)&element)->getStmt();
		std::string tmp = getStmtLoc(it);
		if(path != tmp)
		path += "\n" + tmp;
		break;
	}
	blocks.push_back(CFB);
	while(!blocks.empty())
	{
		CFBTmp = blocks.back();
		blocks.pop_back();
		if(colors.find(CFBTmp) != colors.end())
			continue;
		colors[CFBTmp] = WHITE;
		for(CFGBlock::pred_iterator pit = CFBTmp->pred_begin(); pit != CFBTmp->pred_end(); pit++)
		{
			blocks.push_back(*pit);
			for(CFGBlock::iterator iterblock = (*pit)->begin(); iterblock != (*pit)->end();++iterblock)
			{
				CFGElement element=(*iterblock);
				if(element.getKind()!=CFGElement::Kind::Statement)
					continue;
				const Stmt* it = ((CFGStmt*)&element)->getStmt();
				path += "\n" + getStmtLoc(it);
				break;
			}
		}
	}
	return path;
}

bool DataRace::lockCal(const Stmt* it, std::set<const ValueDecl*>& inLockSet)
{
    if(const CallExpr *tmp = dyn_cast<CallExpr>(it))
    {
		LangOptions LangOpts;
		LangOpts.CPlusPlus = true;
        PrintingPolicy Policy(LangOpts);
        // Update lock set information.
        string TypeS;
        llvm::raw_string_ostream s(TypeS);
        tmp->getCallee()->printPretty(s, 0, Policy);
        if(lockInfos.find(s.str()) != lockInfos.end())
        {
            const Expr *lockEle = tmp->getArg(0);
            lockEle = cast<UnaryOperator>(lockEle)->getSubExpr();
            const ValueDecl* VD = getVarDecl(lockEle, false, nullptr)->getDecl();
            inLockSet.insert(VD);
            lockExprs.insert(tmp);
            return true;
        }
        else if(unlockInfos.find(s.str()) != unlockInfos.end())
        {
            const Expr *lockEle = tmp->getArg(0);
            lockEle = cast<UnaryOperator>(lockEle)->getSubExpr();
            const ValueDecl* VD = getVarDecl(lockEle, false, nullptr)->getDecl();
            inLockSet.erase(VD);
            lockExprs.insert(tmp);
            return true;
        }
    }
    return false;
}

void DataRace::IIRCFGConstruct(std::vector<ASTFunction*> &tarFuns)
{
	for(auto fun : tarFuns)
	{
	    std::queue<CFGBlock*> cfgStack;
        std::unordered_map<CFGBlock*, bool> ifVisited;
        std::unordered_map<CFGBlock*, std::set<const ValueDecl*>> outBlockLockSet;
        std::set<const ValueDecl*> lockSet;

		FunctionDecl* tmp = manager->getFunctionDecl(fun);
		sm = &(tmp->getASTContext().getSourceManager());
		//cout<<tmp->getQualifiedNameAsString()<<endl;
		std::unique_ptr<CFG>& cfg = manager->getCFG(fun);
		LangOptions LangOpts;
		LangOpts.CPlusPlus = true;
		PrintingPolicy Policy(LangOpts);
        cfgStack.push(&(cfg->getEntry()));
        // Init entry block.
        outBlockLockSet[&(cfg->getEntry())] = lockSet;

        while(!cfgStack.empty())
		{
            std::set<const ValueDecl*> inLockSet;
            
            CFGBlock* block = cfgStack.front();
            cfgStack.pop();
            // Skip analyzed functions.
            if(block == nullptr || ifVisited.find(block) != ifVisited.end())
                continue;
            ifVisited[block] = true;
            for(CFGBlock::succ_iterator succBLK = block->succ_begin(); succBLK != block->succ_end(); ++succBLK)
                cfgStack.push(*succBLK);
            // Calculate in state for a block.
            calInState(block, inLockSet, outBlockLockSet);

			for(CFGBlock::iterator iterblock=block->begin();iterblock!=block->end();++iterblock)
			{
				CFGElement element=(*iterblock);
				if(element.getKind()!=CFGElement::Kind::Statement)
					continue;
				const Stmt* it=((CFGStmt*)&element)->getStmt();
                if(lockCal(it, inLockSet))
                    continue;

                outBlockLockSet[block] = inLockSet;

				//The i-th variable in the function
				unsigned int index = 0;
				for(auto exprIT: analyseExpr(it, false))
                {
					index += SVAnalysis(exprIT.first, block, index, fun, exprIT.second, inLockSet);
					index += SVPAnalysis(exprIT.first, block, index, fun, exprIT.second, inLockSet);
                }
			}
		}
	
	}

}

std::unordered_map<const Expr*, std::string> DataRace::analyseExpr(const Stmt* it, bool ifWriteOnly)
{
	std::unordered_map<const Expr*, std::string> exprs;
	std::set<const VarDecl*> tmpRef;
	std::unordered_map<const Expr*, std::string> strippedExprs;
	if(const CallExpr* CE = dyn_cast<CallExpr>(it))
	{
		for(unsigned int i = 0; i< CE->getNumArgs();i++)
		{
			exprs[CE->getArg(i)]="write";
		}
	}
	else if(const BinaryOperator* BO = dyn_cast<BinaryOperator>(it))
	{
		if(!ifWriteOnly)
		{
			const BinaryOperator* BOt = nullptr;
			const Expr* tmpexpr = BO->getRHS();
			while(const CastExpr * it = dyn_cast<CastExpr>(tmpexpr))
				tmpexpr=it->getSubExpr();

			while(BOt = dyn_cast<BinaryOperator>(tmpexpr))
			{
			    exprs[BOt->getLHS()] = "read";
				tmpexpr =  BOt->getRHS();
			}
			exprs[tmpexpr] = "read";
			exprs[BO->getLHS()] = "write";
		}
		//if(ifWriteOnly && !BO->isComparisonOp())
		if(ifWriteOnly && BO->isAssignmentOp())
			exprs[BO->getLHS()] = "write";
	}
	else if(const UnaryOperator* UO = dyn_cast<UnaryOperator>(it))
	{
		if(UO->isIncrementDecrementOp())
			exprs[UO->getSubExpr()] = "write";
	}
    int count = 0;
    // return exprs
    // not sure what's the meaning of the following codes. 
    // it initially designed for global var, but not correct.
    for(auto exprIT: exprs)
    {
        ValueDecl *valDecl = nullptr;
        const DeclRefExpr* declRef = getVarDecl(exprIT.first, true, &valDecl);
        if(declRef == nullptr)
        {
            declRef = getVarDecl(exprIT.first, false, &valDecl);
            if(declRef == nullptr)
                continue;
        }
        if(const VarDecl* varDecl = dyn_cast<VarDecl>(declRef->getDecl()))
        {
            auto setIT = tmpRef.find(varDecl);
            if(setIT == tmpRef.end())
            {
                tmpRef.insert(varDecl);
                strippedExprs[exprIT.first] = exprIT.second;
            }
            else if(exprIT.second == "write")
                strippedExprs[exprIT.first] = exprIT.second;
        }
    }

	return strippedExprs;
}

void DataRace::analyzeCallees()
{
    /*
     * This is used to handle the 8250 situation:
     * isr(){transmitchar();}
     * task(){transmitchar();}
     */
    /*std::set<ASTFunction*> ifFind;
	for(auto isr : ISRs)
	{
        for(auto child : call_graph->getChildren(isr.first))
        {
            ifMultiPar(child, ifFind);
        }
	}*/
    // Other functionalities.
	std::vector<ASTFunction*> topLevelFuncs = call_graph->getTopLevelFunctions();
    int num = 0;
    for(auto funcs: topLevelFuncs)
    {
        num++;
    }
    num = num - 2 - ISRs.size();
    cout<<"Num Of Tasks: "<<num<<endl;
    cout<<"Num Of Branches: "<<numOfBranches<<endl;
    cout<<"Num Of SVs: "<<numOfSVs<<endl;
}

bool DataRace::ifReported(const DeclRefExpr *expr1, const DeclRefExpr *expr2)
{
    // Trick way to reduce duplicated race warnings. Not sure whether it's OK.
    // However, we can store smaller expr to map, this will be much easier.
    if(expr1 < expr2)
        return false;
    return true;
}

bool DataRace::ifMultiPar(ASTFunction* root, std::set<ASTFunction*> &reportedFunc)
{
    if(reportedFunc.find(root) != reportedFunc.end())
        return true;
    std::string rootName = manager->getFunctionDecl(root)->getQualifiedNameAsString();
    std::vector<ASTFunction*> parents = call_graph->getParents(root);
    bool ifMulti = false;
    for(auto parent : parents)
    {
        FunctionDecl* tmp = manager->getFunctionDecl(parent);
        bool ifFind = false;
        for(auto fun : ISRCallees)
        {
            if(fun == parent)
                //skip the parent called at ISR.
                ifFind = true;
        }
        if(ifFind)
            continue;
        ifMulti = true;
        if(!fout)
            continue;
        reportedFunc.insert(root);
        fout<<tmp->getQualifiedNameAsString()+"->"+rootName<<endl;
	    cout<<"GuideSrc.txt" << warningCount<<endl;
        warningCount++;
    }
    if(ifMulti)
        return true;
    std::vector<ASTFunction*> children = call_graph->getChildren(root);
    if(children.empty() == false)
    {
        // If the 'root' doesn't find a multipar, then test its children.
        // i.e., the children of the children of the ISR may also have multipar.
        for(auto child : children)
            ifMultiPar(child, reportedFunc);
    }
    return false;
}

void DataRace::_DFSTopSort(ASTFunction *i,
		std::unordered_map<ASTFunction *, Color>& colors, std::vector<ASTFunction*> &NonTopoOrder){
	colors[i] = GRAY;//vISIT i

	const std::vector<ASTFunction *> children = call_graph->getChildren(i);
	if (children.size() != 0) {

		for (auto child : children) {
			//children have not been visited
			if(colors[child] == WHITE){
				_DFSTopSort(child, colors, NonTopoOrder);
			}
		}
	}
	NonTopoOrder.push_back(i);
	colors[i] = BLACK;
}

std::vector<ASTFunction *> DataRace::getNonTopoOrder(std::vector<ASTFunction *> topLevelFuncs) {

	std::vector<ASTFunction *> NonTopoOrder;


	std::unordered_map<ASTFunction *, Color> colors; 

	//init colors
	//for (auto func : ISRs) {  
	for (auto func : resource->getFunctions()) {  
		colors[func] = WHITE;
	}

	//for (auto topLevelF : ISRs){
	for (auto topLevelF : topLevelFuncs){
		_DFSTopSort(topLevelF, colors, NonTopoOrder);
	}

	return NonTopoOrder;
}
