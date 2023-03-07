
#include "DanglingPointer.h"

#define GLOBALVAR "_GLOBAL_"

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.

VarInfo::VarInfo(string varName, bool ifFree, bool ifStatic, string loc, string fun)
{
    //cout<<"var:"<<varName<<" added as "<<ifStatic<<" in "<<loc<<endl;
	if(loc == "")
		loc = "ERR:0:0";
    this->varName = varName;
    this->ifFree = ifFree;
    this->ifStatic = ifStatic;
    this->loc = loc;
	this->fun = fun;
	int pos1 = loc.find_last_of('<');
    std::string str,tmpLine;
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
    tmpLine = str.substr(pos2+1);
    this->line = stoi(tmpLine);
    ifNulled = false;
    alias = NULL;
    freeLike = NULL;
	ifReported = false;
}

FunInfo::FunInfo(string name, int arg, int ttl, string loc, int line)
{
    //cout<<"fun:"<<name<<" added as arg:"<<arg << " in "<<loc<<endl;
	if(loc == "")
		loc = "ERR:0:0";
    refCount = 0;
    funName = name;
    argsNum = arg;
    TTL = ttl;
    parent = NULL;
    int pos1 = loc.find_last_of('<');
    std::string str;
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
	if(line == 0)
	{
        string tmpLine = str.substr(pos2+1);
        this->line = stoi(tmpLine);
	}
	else
		this->line = line;
}

FunInfo::FunInfo(string name, string varName, int ttl, string loc, int line)
{
    //cout<<"fun:"<<name<<" added as var:"<<varName << " in "<<loc<<endl;
    refCount = 0;
    funName = name;
    var = varName;
    argsNum = -1;
    TTL = ttl;
    this->loc = loc;
    parent = NULL;
	
    int pos1 = loc.find_last_of('<');
    std::string str;
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
	if(line == 0)
	{
        string tmpLine = str.substr(pos2+1);
        this->line = stoi(tmpLine);
	}
	else
		this->line = line;
}

/*
class GlobalVarDecl : public ASTConsumer, public RecursiveASTVisitor<GlobalVarDecl> {

    public :

    void HandleTranslationUnit(ASTContext &Context) override {
        TranslationUnitDecl *decl = Context.getTranslationUnitDecl();
        TraverseDecl(decl);
    }
    bool VisitDecl(Decl *D)
    {
        if(ParmVarDecl *E = dyn_cast<ParmVarDecl>(D))
            return true;
        if(VarDecl *E = dyn_cast<VarDecl>(D))
        {
            list<VarInfo>::iterator varIterator;
            if(ifStaticVar(E->getQualifiedNameAsString(), &varIterator) == -1 && !E->isLocalVarDecl())
            {
                SourceManager *sm;
                sm = &(D->getASTContext().getSourceManager());
                string loc = E->getLocStart().printToString(*sm);
				if(loc != "")
				{
					VarInfo tmp = VarInfo(E->getQualifiedNameAsString(), false, true, loc, GLOBALVAR);
					tmp.loc = E->getLocStart().printToString(*sm);
					//cout<<"VisitDecl-global:"<<tmp.varName<<endl;
					variables.push_front(tmp);
				}
            }

        }
        return true;
    }

    int ifStaticVar(string var, list<VarInfo>::iterator *iterator)
    {
        list<VarInfo>::iterator VarInfoIterator;
        for(VarInfoIterator = variables.begin(); VarInfoIterator != variables.end(); ++VarInfoIterator)
        {
            if(VarInfoIterator->varName == var)
            {
               *iterator = VarInfoIterator; 
               if(VarInfoIterator->ifStatic)
                   return 1;
               else
                   return 0;
            }
        }
        return -1;
    }
    list<VarInfo>* getVars(){
        return &variables;
    }

    private:
    list<VarInfo> variables;
};
*/
std::string DanglingPtr::replace_all(string str, const string old_value, const string new_value)
{     
    while(true){
		string::size_type pos(0);     
        if((pos=str.find(old_value))!=string::npos)     
            str.replace(pos,old_value.length(),new_value);     
        else 
			break;     
    }     
    return str;
} 

int DanglingPtr::writingToXML(string fileName, string funName, string descr, string locLine)
{
	string report=fileName+funName+descr+locLine;
	if(find(reports.begin(),reports.end(),report)==reports.end()){
		common::printLog( "Not find in reports\n",common::CheckerName::arrayBound,0,*configure);
		reports.push_back(report);
	}
	else{
		common::printLog( "find in reports,return\n",common::CheckerName::arrayBound,0,*configure);
		return 0;
	}
	if(fileName.find("/usr/lib/")!=std::string::npos){
		common::printLog( "find /usr/lib/ in reports,return\n",common::CheckerName::arrayBound,0,*configure);
		return 0;
	}
	pugi::xml_node node = doc.append_child("error");

    pugi::xml_node checker = node.append_child("checker");
    checker.append_child(pugi::node_pcdata).set_value("MOLINT.POINTER.NULL.SET");

	pugi::xml_node domain = node.append_child("domain");
    domain.append_child(pugi::node_pcdata).set_value("STATIC_C");

	pugi::xml_node file = node.append_child("file");
	file.append_child(pugi::node_pcdata).set_value(replace_all(fileName, "\\", "/").c_str());

	pugi::xml_node function = node.append_child("function");
    function.append_child(pugi::node_pcdata).set_value(funName.c_str());

	pugi::xml_node score = node.append_child("score");
    score.append_child(pugi::node_pcdata).set_value("100");

	pugi::xml_node ordered = node.append_child("ordered");
    ordered.append_child(pugi::node_pcdata).set_value("false");

	pugi::xml_node event = node.append_child("event");

	pugi::xml_node main = event.append_child("main");
    main.append_child(pugi::node_pcdata).set_value("true");

	pugi::xml_node tag = event.append_child("tag");
    tag.append_child(pugi::node_pcdata).set_value("Error");

	pugi::xml_node description = event.append_child("description");
    description.append_child(pugi::node_pcdata).set_value(descr.c_str());

	pugi::xml_node line = event.append_child("line");
    line.append_child(pugi::node_pcdata).set_value(locLine.c_str());


	pugi::xml_node extra = node.append_child("extra");
    extra.append_child(pugi::node_pcdata).set_value("none");

	pugi::xml_node subcategory = node.append_child("subcategory");
    subcategory.append_child(pugi::node_pcdata).set_value("none");

	return 0;
}


int DanglingPtr::reportWarning(VarInfo &tmp, VarInfo* alias)
{
	string file = tmp.file, fun = tmp.fun;
    string desc;
	string a;
    a = to_string(tmp.line);
	tmp.ifReported = true;
	if(alias == nullptr)
    {
        desc = "Variable:" + tmp.varName;
    }
	else
	{
		desc = "Variable:" + tmp.varName +  " (alias as " + alias->varName  + ")";
	}
	desc += " is a dangling pointer";
	// Considering inaccurate alias analysis at line:533. Delete this and change code in line:533.
	if(tmp.freeLike != NULL && alias == nullptr) //This is not a accurate way to detect double free bugs.
    {
        //remove annotation to enable call trace
        FunInfo *trace = tmp.freeLike;
		//desc += ", its call graph is as follows:"+tmp.fun+":"+a;
        std::string a = to_string(trace->line);
        std::string fun = trace->funName;
        std::string file = trace->file;
        while(trace->parent)
        {
            trace = trace->parent;
            //string a;
			a = to_string(trace->line);
            //desc += "-》" + trace->funName + ":" + a;
			fun = trace->funName;
			file = trace->file;
			
        }
		if(reportedFun.find(trace) != reportedFun.end())
			return 0;
		reportedFun[trace] = true;
    }

	/*if(tmp.freeLike != NULL && alias == nullptr) //This is not a accurate way to detect double free bugs.
    {
        FunInfo *trace = tmp.freeLike;
		desc += ", its call graph is as follows:"+tmp.fun+":"+a;
        while(trace)
        {
            string a;
			a = to_string(trace->line);
            desc += "-》" + trace->funName + ":" + a;
            trace = trace->parent;
			
        }
    }*/
	desc += ".";
	warningCount++;
	writingToXML(file, fun, desc, a);
	desc += " In function: " + fun;
	common::printLog(desc + "\n", common::CheckerName::danglingPointer, 5, *configure);
    return 0;
}

void DanglingPtr::check(){
    //analyze all global variables.
    warningCount = 0;
    /*for (const auto &AST : r.getASTs()->getASTs()) {
        //construct global variables
        GlobalVarDecl globVar;
        globVar.HandleTranslationUnit(*AST);
        list<VarInfo>* tmp = globVar.getVars();
        list<VarInfo>::iterator tmpIte;
        for(tmpIte = tmp->begin(); tmpIte != tmp->end(); ++tmpIte)
        {
            VarInfo tmpVar = *tmpIte;
            vis.push_front(tmpVar);
        }
    }*/
    readConfig(*configure);
	std::vector<ASTFunction *> nonTopoOrder = getNonTopoOrder();
    for(auto astFunction:nonTopoOrder)
    {
        /*if(fNode == NULL)
        {
			
            //remove free like functions, because the caller exceeds 
            //CONFIG levels
            list<FunInfo>::iterator iterator;
            for(iterator = fis.begin(); iterator != fis.end(); ++iterator)
            {
                if(iterator->TTL == 0)
                {
                    reportWarning(*iterator);
                }
            }
            fis.remove_if([](const FunInfo & value)->bool{
                return value.TTL<=0;
                });
        }
        else*/
		if(astFunction != NULL)
        {
			FunctionDecl* fNode = manager->getFunctionDecl(astFunction);
			std::unique_ptr<CFG>& cfg = manager->getCFG(astFunction);
            std::string funName = fNode->getQualifiedNameAsString();
			if( cfg == NULL)
				continue;
		    common::printLog("Function: " + funName+ " begin\n", common::CheckerName::danglingPointer, 2, *configure);
            VisitFunDecl(fNode, levelOfDete, cfg);
	        common::printLog("Function: " + funName + " end\n", common::CheckerName::danglingPointer, 2, *configure);
        }
    }
    //deal with situation when a dangling pointer in F0, and F1 call F0. if detect level is 2, then F1 doesn't reach the last level of detecting. so we need to detect these situation at once.
    //FIXME: to fix following bugs. the algorism is in oneNote-HW-TODO1
    /*
    list<FunInfo>::iterator iterator;
    for(iterator = fis.begin(); iterator != fis.end(); iterator++)
    {
        FunInfo *tmp = iterator->parent;
        while(tmp)
        {
            tmp->refCount++;
            tmp = tmp->parent;
        }
    }
    for(iterator = fis.begin(); iterator != fis.end(); iterator++)
    {
        if(iterator->refCount != 0)
            continue;
        int refCount = 0;
        FunInfo *tmp = iterator->parent;
        while(tmp)
        {
            tmp->refCount++;
            refCount++;
            tmp = tmp->parent;
        }
        if(refCount != levelOfDete)
            reportWarning(*iterator);
    }*/
	finalCheck();
	common::printLog("TOTAL warning:" + std::to_string(warningCount) + "\n", common::CheckerName::danglingPointer, 5, *configure);
	if(warningCount != 0)
	{
		std::string xmlFile = configure->getOptionBlock("pathToReport")["path"] + "MOLINT.POINTER.NULL.SET.xml";
		doc.save_file(xmlFile.c_str(), "\t", pugi::format_no_declaration);
	}
}

int DanglingPtr::readConfig(Config &c){
	//set level of detect and ifRegardParAsFreelike as fixed value.
	//levelOfDete = 1;
	//ifRegardParAsFreelike = true;
	
	std::unordered_map<std::string, std::string> ptrConfig = c.getOptionBlock("DanglingPointer");
	std::unordered_map<std::string, std::string>::const_iterator got = ptrConfig.find("levelOfDete");
	if(got == ptrConfig.end())
	{
		common::printLog( "Config: levelOfDete not found, using default value:2\n", common::CheckerName::danglingPointer, 3, *configure);
		levelOfDete = 2;
	}
	else
		levelOfDete = stoi(got->second);
	got = ptrConfig.find("ifRegardParAsFreelike");
	if(got == ptrConfig.end())
	{
		common::printLog( "Config: ifRegardParAsFreelike not found, using default value:true\n", common::CheckerName::danglingPointer, 3, *configure);
		ifRegardParAsFreelike = true;
	}
	if(got->second == "true")
		ifRegardParAsFreelike = true;
	else
		ifRegardParAsFreelike = false;
	
	std::unordered_map<std::string, std::string> memFuns = c.getOptionBlock("MemoryReleaseFunction");
	std::unordered_map<std::string, std::string>::const_iterator memGot = memFuns.begin();
	for(;memGot != memFuns.end(); memGot++)
	{
		int tmp = stoi(memGot->second);
		memoryReleaseFuns[memGot->first] = tmp;
	}
    return 0;
}

int DanglingPtr::clearLocalVar()
{
    list<VarInfo>::iterator iterator;
    for(iterator = vis.begin(); iterator != vis.end(); ++iterator)
    {
        if(iterator->alias != NULL && !iterator->alias->ifStatic)
            iterator->alias = NULL;
    }
    vis.remove_if([](const VarInfo & value)->bool{
        return !value.ifStatic;
        });
    return 0;
}

int DanglingPtr::ifMemoryFun(string funName)
{
	std::unordered_map<std::string, int>::iterator got = memoryReleaseFuns.find(funName);
	if(got == memoryReleaseFuns.end())
        return -1;
    else
        return got->second;
}

int DanglingPtr::getOperLine(string loc)
{
    int pos1 = loc.find_last_of(':');
    string str;
    str.assign(loc.c_str(),pos1);
    int pos2=str.find_last_of(':');
    string tmpLine = str.substr(pos2+1);
    return stoi(tmpLine);
}

int DanglingPtr::varStateTransform(string lfh, string rfh, FunctionDecl *f, string loc, string fun, Expr* rhs)
{
        list<VarInfo>::iterator iterator;
        list<VarInfo>::iterator rfhIterator;
		const Expr* rhsV = rhs;
		string rhsS = rfh;
        //cout<<lfh<<" = "<<rfh<<endl;
        int ifStatic = ifStaticVar(lfh, &iterator, loc);
        int ifRfhStatic = ifStaticVar(rfh, &rfhIterator, loc);
		//excludes condition like "int a=0; a=a;"
		if(lfh == rfh)
			return 0;
        if(ifStatic == -1)
        {
			//The pattern is like "p = q" while free(q) but p isn't in vis.
			if(ifRfhStatic != -1)
			{
				VarInfo tmp = VarInfo(lfh, rfhIterator->ifFree, false, loc, fun);
				tmp.ifNulled = rfhIterator->ifNulled;
				vis.push_front(tmp);
				iterator = vis.begin();
				//alias is q points to p
				iterator->alias = &(*rfhIterator);
				rfhIterator->preAlias.push_back(&(*iterator));
			}
            return 0;
        }

		//TODO: analyze whether RHS is null
		while(const ImplicitCastExpr * it = dyn_cast<ImplicitCastExpr>(rhsV)){
			rhsV=it->IgnoreImpCasts();
		}
		
		Expr::NullPointerConstantKind npk = rhsV->isNullPointerConstant(f->getASTContext(),Expr::NPC_ValueDependentIsNull);
        //cout<<"binary oper:"<<lfh<<rfh<<" "<<ifStatic<<endl;
		/*int pos = rhsS.find_first_of(")");
		rhsS.assign(rhsS.c_str(), pos+1, rhsS.size());
		pos = rhsS.find_last_of("0");
		rhsS.assign(rhsS.c_str(), pos+1);*/
        if(ifRfhStatic != -1)
        {
            //if it is alias
            //The pattern is like "p = q" while free(q) and p is in vis.
			for(list<VarInfo*>::iterator aliasIte=iterator->preAlias.begin();aliasIte != iterator->preAlias.end();++ aliasIte)
			{
				(*aliasIte)->alias = NULL;
			}
            VarInfo tmp = VarInfo(lfh, rfhIterator->ifFree, false, loc, fun);
            iterator->ifFree = rfhIterator->ifFree;
            iterator->ifNulled = rfhIterator->ifNulled;
            iterator->loc = tmp.loc;
            iterator->line = tmp.line;
            iterator->file = tmp.file;
            iterator->alias = &(*rfhIterator);
			rfhIterator->preAlias.push_back(&(*iterator));
        }
		// NOTE: This is used to determine whether a freed pointer is nullified.
		//else if(rhs->isNullPointerConstant(f->getASTContext(),Expr::NPC_NeverValueDependent) != Expr::NPCK_NotNull || rfh == "0")
		else if((rhsS.find("0") != -1 && rhsS.find("\'") == -1 && rhsS.find("\"") == -1) && rhs->isRValue() && !isa<CallExpr>(rhsV))
        {
            //if(rfh == "((void *)0)")    we assume if left oper contain freed var, then this var is nulled
            //if it is nulled, iterator->line means the position of free fun.
            if(getOperLine(loc) > iterator->line)
            {
			    common::printLog(lfh + " is nulled at " + loc + "\n", common::CheckerName::danglingPointer, 2, *configure);
                iterator->ifNulled = true;
            }
            //to deal with this example: {free(gp); F0(gp);gp=null;} F0 is freelike
            //FIXME: not distinguish global and local var
            for(;iterator != vis.end(); ++ iterator)
            {
                if(iterator->varName == lfh)
                {
                    if(getOperLine(loc) > iterator->line)
                        iterator->ifNulled = true;
                }
            }
        }
        return 0;
}

int DanglingPtr::finalCheck(){
	list<VarInfo>::iterator iterator;
    for(iterator = vis.begin(); iterator != vis.end(); ++iterator)
	{
		if(iterator->freeLike != NULL && iterator->ifReported == false)
		{
			reportWarning(*iterator);
		}
	}
    return 0;
}

int DanglingPtr::checkVar(string funName)
{
    //printList();
    list<VarInfo>::iterator iterator;
	VarInfo * ifAliasFreed = nullptr;
    for(iterator = vis.begin(); iterator != vis.end(); ++iterator)
    {
		if(iterator->fun == GLOBALVAR)
			iterator->fun = funName;
        if(iterator->freeLike == NULL)
        {
            if(iterator->ifFree && !iterator->ifNulled)
            {
                if(!iterator->ifStatic)
				{
					iterator->fun = funName;
                    reportWarning(*iterator);
				}
                else
                {
                    //add global dangling var's info into FunInfo
                    iterator->ifFree = false;
                    fis.push_front(FunInfo(funName,iterator->varName, levelOfDete, iterator->loc, iterator->line));
                    iterator->freeLike = &(*fis.begin());
                }
            }
        }
        else
        {
            std::unordered_map<std::string, int>::const_iterator got = ifCalled.find(iterator->freeLike->funName);
            if(got == ifCalled.end())
                continue;
			//it means this function called freelike function.
            if(!iterator->ifNulled)
            {
                if(iterator->freeLike->TTL == 1)
                {
					iterator->fun = funName;
					reportWarning(*iterator);
                }
                else
                {
					iterator->fun = funName;
                    FunInfo tmp = FunInfo(funName,  iterator->freeLike->var, iterator->freeLike->TTL - 1, iterator->loc, got->second);
                    tmp.parent = iterator->freeLike;
                    fis.push_front(tmp);
                }
            }
        }
        //FIXME: there is a situation duplicated report a var: one time is local and the other is alias
        if(iterator->alias != NULL)
        {
            VarInfo *tmp = iterator->alias;
			set<VarInfo *> loopDetect;
            while(tmp != NULL)
            {
				if(loopDetect.find(tmp) != loopDetect.end())
					break;
				loopDetect.insert(tmp);
                if(tmp->ifFree)
                    ifAliasFreed = tmp;
				
                tmp = tmp->alias;
            }
            loopDetect.clear();
            if(ifAliasFreed != nullptr)
            {
                VarInfo *tmp = iterator->alias;
                while(tmp != NULL)
                {
                    if(!tmp->ifNulled)
					{
						tmp->fun = funName;
                        reportWarning(*iterator, tmp);
					}
                    tmp = tmp->alias;
                }
            }
        }
    }

    ifCalled.erase(ifCalled.begin(), ifCalled.end());
    
    return 0;
}

int DanglingPtr::addStaticVar(const Expr *expr)
{
	const Expr *exprTmp=expr;
	//expr->dump();
	common::printLog("addStaticVar: "+DPprintStmt((Stmt*)expr)+"\n",common::CheckerName::danglingPointer,0,*configure);
	while(const ImplicitCastExpr * it = dyn_cast<ImplicitCastExpr>(exprTmp)){
		exprTmp=it->IgnoreImpCasts();
	}
	QualType qt = exprTmp->getType();
	//cout<<"QualType"<<endl;
	//qt->dump();
	//cout<<qt->isPointerType()<<endl;
	if(!qt->isPointerType()){
		common::printLog("not PointerType\n",common::CheckerName::danglingPointer,0,*configure);
		return 0;
	}
	common::printLog("IS PointerType\n",common::CheckerName::danglingPointer,0,*configure);
	if(const DeclRefExpr* declRef=dyn_cast<DeclRefExpr>(exprTmp)){
			const ValueDecl *valueDecl = declRef->getDecl();
			if(const VarDecl* varDecl=dyn_cast<VarDecl>(valueDecl))
			{
				list<VarInfo>::iterator iterator;
				if(varDecl->isLocalVarDecl() )
					return 0;
				if(const ParmVarDecl* pv=dyn_cast<ParmVarDecl>(valueDecl))
					return 0;
				int ifStatic = ifStaticVar(varDecl->getQualifiedNameAsString(),&iterator,"");
				if(ifStatic != -1)
					return 0;
				SourceManager *sm;
                sm = &(varDecl->getASTContext().getSourceManager());
                string loc = varDecl->getLocStart().printToString(*sm);
				if(loc != "")
				{
					VarInfo tmp = VarInfo(valueDecl->getQualifiedNameAsString(), false, true, loc, GLOBALVAR);
					tmp.loc = valueDecl->getLocStart().printToString(*sm);
					//cout<<"VisitDecl-global:"<<tmp.varName<<endl;
					vis.push_front(tmp);
				}
			}
	}
	return 0;
}

int DanglingPtr::ifStaticVar(string var, list<VarInfo>::iterator *iterator, string loc)
{
    list<VarInfo>::iterator VarInfoIterator;
    for(VarInfoIterator = vis.begin(); VarInfoIterator != vis.end(); ++VarInfoIterator)
    {
        if(VarInfoIterator->varName == var)
        {
            *iterator = VarInfoIterator; 
            if(VarInfoIterator->ifStatic)
                return 1;
            else
                return 0;
        }
    }
    return -1;
}
int DanglingPtr::handleMemoryReleaseFun(FunctionDecl *f, string varName, string loc)
{
    list<VarInfo>::iterator varIterator;
	string funName = f->getQualifiedNameAsString();
    int ifStatic = ifStaticVar(varName, &varIterator, loc); 
    if(ifStatic == -1)
    {
        //if variable is Params
        int i = 0, numOfParams = f->getNumParams();
        for(; i<numOfParams;i++)
        {
            string arg =  f->parameters()[i]->getQualifiedNameAsString();;
            if(arg == varName) break;
        }
        if(i == numOfParams)
        { 

            VarInfo tmpVar = VarInfo(varName, true, false, loc, funName);
            vis.push_front(tmpVar);
            varIterator = vis.begin();
        }
        else
        {
            //it means call exper is free fun and the args is parameters. Namely, it's local var
            VarInfo tmpVar = VarInfo(varName, true,  false, loc, funName);
            vis.push_front(tmpVar);
            varIterator = vis.begin();
            //if fun free it's parameter, we just verify whether the parameter is set as null.
            //regard it as a free like function but ttl is 1
            //FIXME: ttl set as 1 means it should be nulled in caller.
            if(ifRegardParAsFreelike)
            {
                std::unordered_map<std::string, int>::iterator got = ifCalled.find(funName);
                if(got == ifCalled.end())
                    fis.push_front(FunInfo(funName,i, 1, loc));
                else
                    fis.push_front(FunInfo(funName,i, 1, loc,got->second));
            }
        }
    }
    else
    {
        //handle situation when {free(a); a=maoloc;free(a)}
        int curPos = getOperLine(loc);
        if(curPos > varIterator->line)
            varIterator->line = curPos;
    }
    varIterator->ifFree = true;
	return 0;
}
std::string DPprintStmt(Stmt* stmt)
{
	LangOptions L0;
	L0.CPlusPlus=1;
	std::string buffer1;
	llvm::raw_string_ostream strout1(buffer1);
	stmt->printPretty(strout1,nullptr,PrintingPolicy(L0));
	return ""+strout1.str()+"";
}
bool DanglingPtr::VisitFunDecl(FunctionDecl *f, int ttl, const std::unique_ptr<clang::CFG> &myCFG)
{
    if (f->hasBody()) {
        string funName = f->getQualifiedNameAsString();
		std::unordered_map<std::string, int>::iterator got = memoryReleaseFuns.find(funName);
		if(got != memoryReleaseFuns.end())
		{
			common::printLog(funName + " is memory release function, skipping.", common::CheckerName::danglingPointer, 2, *configure);
			return false;
		}
        for(unsigned int i = 0; i<f->getNumParams();i++)
        {
            string arg =  f->parameters()[i]->getQualifiedNameAsString();
        }
        clearLocalVar();
        Stmt *FuncBody = f->getBody();
        int i = 0;
        LangOptions LangOpts;
        LangOpts.CPlusPlus = true;
        PrintingPolicy Policy(LangOpts);
        LangOptions LO;
        LO.CPlusPlus=1; 
        //myCFG->dump(LO,true);

        for(CFG::iterator iter=myCFG->begin();iter!=myCFG->end();++iter)
        {
            CFGBlock* block=*iter;
            for(CFGBlock::iterator iterblock=block->begin();iterblock!=block->end();++iterblock)
            {
                CFGElement element=(*iterblock);
                if(element.getKind()!=CFGElement::Kind::Statement)
                    continue;
                const Stmt* it=((CFGStmt*)&element)->getStmt();
				//it->dump();
				//cout<<it->getStmtClass()<<endl;
                if(it->getStmtClass() == Stmt::CallExprClass)
                {
                    const CallExpr *tmp = cast<CallExpr>(it);
                    string TypeS;
                    llvm::raw_string_ostream s(TypeS);
                    tmp->getCallee()->printPretty(s, 0, Policy);
                    //if it's a memory free call
                    int argNum = ifMemoryFun(s.str());
                    if(argNum != -1)
                    {
                        string argS;
                        llvm::raw_string_ostream args(argS);
                        const Expr * freeArg = tmp->getArg(argNum);
						const Expr * freeArg2=freeArg->IgnoreCasts()->IgnoreParenCasts();
						freeArg2->printPretty(args, 0, Policy);						
						
						//add static variables before these variables used
						addStaticVar(freeArg2);

                        list<VarInfo>::iterator varIterator;
                        SourceManager *sm;
                        sm = &(f->getASTContext().getSourceManager()); 
                        string loc = tmp->getLocStart().printToString(*sm);
						handleMemoryReleaseFun(f, args.str(), loc);
                    }
                    else
                    {
                        //if the callee is free like function.
                        SourceManager *sm;
                        sm = &(f->getASTContext().getSourceManager()); 
                        string loc = tmp->getLocStart().printToString(*sm);
                        list<FunInfo>::iterator iterator;
                        //printList();
                        for(iterator = fis.begin(); iterator != fis.end(); ++iterator)
                        {
                            if(s.str() == iterator->funName)
                            {
                                ifCalled[iterator->funName] = getOperLine(loc);
                                if(iterator->argsNum!= -1)
                                {
                                    //callee's parameters is freed
                                    string argName;
                                    llvm::raw_string_ostream args(argName);
                                    const Expr * freeArg = tmp->getArg(iterator->argsNum);
									const Expr * freeArg2=freeArg->IgnoreCasts()->IgnoreParenCasts();
									freeArg2->printPretty(args, 0, Policy);
									addStaticVar(freeArg2);

                                    VarInfo tmpVar = VarInfo(args.str(), true,  false, loc, funName);
                                    tmpVar.freeLike = &(*iterator);
                                    vis.push_front(tmpVar);
                                }
                                else
                                {
                                    //global variable is freed
                                    //two or more global var's free is not handlled, as I don't know whether this var is the first time handlled or not.
                                    list<VarInfo>::iterator varIterator;
                                    int ifStatic = ifStaticVar(iterator->var, &varIterator, loc); 
                                    if(ifStatic == -1 || (ifStatic != -1 && varIterator->freeLike != &(*iterator)))
                                    {
                                        VarInfo tmpVar = VarInfo(iterator->var, true,  true, loc, funName);
                                        tmpVar.freeLike = &(*iterator);
                                        vis.push_front(tmpVar);
                                    }
                                }
                            } 
                        }
                    }
                }
				else if(it->getStmtClass() == Stmt::CXXDeleteExprClass)
				{
					if(ifMemoryFun("delete") == -1)
						continue;
					//handle delete p
					const CXXDeleteExpr *tmp = cast<CXXDeleteExpr>(it);
                    string argS;
                    llvm::raw_string_ostream args(argS);
                    const Expr * freeArg = tmp->getArgument();
					//freeArg->dump();
					const Expr * freeArg2=freeArg->IgnoreCasts()->IgnoreParenCasts();
					//freeArg2->dump();
					freeArg2->printPretty(args, 0, Policy);
					addStaticVar(freeArg2);

                    list<VarInfo>::iterator varIterator;
                    SourceManager *sm;
                    sm = &(f->getASTContext().getSourceManager()); 
                    string loc = tmp->getLocStart().printToString(*sm);
					handleMemoryReleaseFun(f, args.str(), loc);
				}
            }
        }
        for(CFG::iterator iter=myCFG->begin();iter!=myCFG->end();++iter)
        {
            CFGBlock* block=*iter;
            for(CFGBlock::iterator iterblock=block->begin();iterblock!=block->end();++iterblock)
            {
                CFGElement element=(*iterblock);
                if(element.getKind()!=CFGElement::Kind::Statement)
                    continue;
                const Stmt* it=((CFGStmt*)&element)->getStmt();
                if(it->getStmtClass() == Stmt::BinaryOperatorClass)
                {
                    const BinaryOperator *E = cast<BinaryOperator>(it);
					if(E->getOpcode() != BO_Assign)
						continue;
                    Expr* lhs = E->getLHS();  
                    Expr* rhs = E->getRHS();
					//cout<<"lhs=rhs"<<endl;
					//lhs->IgnoreCasts()->IgnoreParenCasts()->dump();
					//rhs->IgnoreCasts()->IgnoreParenCasts()->dump();
					addStaticVar(lhs->IgnoreCasts()->IgnoreParenCasts());
					addStaticVar(rhs->IgnoreCasts()->IgnoreParenCasts());
					if (MemberExpr *MRE = dyn_cast<MemberExpr>(lhs->IgnoreCasts()->IgnoreParenCasts()))
					{
						string slhs, srhs;
						llvm::raw_string_ostream s1(slhs);
						MRE->IgnoreCasts()->IgnoreParenCasts()->printPretty(s1, 0, Policy);
						llvm::raw_string_ostream s2(srhs);
						rhs->IgnoreCasts()->IgnoreParenCasts()->printPretty(s2, 0, Policy);
						SourceManager *sm;
						sm = &(f->getASTContext().getSourceManager()); 
						string loc = E->getLocStart().printToString(*sm);
                        varStateTransform(s1.str(),s2.str(),f, loc, funName, rhs);
					}
                    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(lhs->IgnoreCasts()->IgnoreParenCasts())) {
                        if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
                            string TypeS;
                            llvm::raw_string_ostream s(TypeS);
                            rhs->IgnoreCasts()->IgnoreParenCasts()->printPretty(s, 0, Policy);
                            SourceManager *sm;
                            sm = &(f->getASTContext().getSourceManager()); 
                            string loc = E->getLocStart().printToString(*sm);
                            varStateTransform(VD->getQualifiedNameAsString(),s.str(),f, loc, funName, rhs);
                        }
                    }
                }
            }
        }
    checkVar(funName);
    }
    return true;
}

void DanglingPtr::printList()
{
    bool ifVar = true;
    bool ifFun = true;
    if(ifVar)
    {
		common::printLog("-------------VarInfo--------", common::CheckerName::danglingPointer, 1, *configure);
        list<VarInfo>::iterator iterator;
        for(iterator = vis.begin(); iterator != vis.end(); ++iterator)
        {
            if(iterator->freeLike)
				common::printLog(iterator->varName + "->" + iterator->freeLike->funName + "\n", common::CheckerName::danglingPointer, 1, *configure);
            else
				common::printLog(iterator->varName + "->" + iterator->freeLike->funName + "\n", common::CheckerName::danglingPointer, 1, *configure);
        }
    }
    if(ifFun)
    {
		common::printLog("-------------FunInfo--------", common::CheckerName::danglingPointer, 1, *configure);
        list<FunInfo>::iterator fiterator;
        for(fiterator = fis.begin(); fiterator != fis.end(); ++fiterator)
        {
            if(fiterator->parent)
				common::printLog(fiterator->funName + "->" + fiterator->parent->funName + " var:" + fiterator->var + "\n", common::CheckerName::danglingPointer, 1, *configure);
            else
				common::printLog(fiterator->funName + "->" + fiterator->parent->funName + " var:" + fiterator->var + "\n", common::CheckerName::danglingPointer, 1, *configure);
                //cout<<fiterator->funName<<"->"<<fiterator->parent<<" var:"<<fiterator->var<<endl;

        }
    }
	common::printLog("--------------END-------", common::CheckerName::danglingPointer, 1, *configure);

}


//inner recursive dfs function 
void DanglingPtr::_DFSTopSort(ASTFunction *i, 
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


std::vector<ASTFunction *> DanglingPtr::getNonTopoOrder() {
    
	std::vector<ASTFunction *> NonTopoOrder;
    
	std::vector<ASTFunction *> topLevelFuncs = call_graph->getTopLevelFunctions();

    std::unordered_map<ASTFunction *, Color> colors; 

	//init colors
	for (auto func : resource->getFunctions()) {  
		colors[func] = WHITE;
	}

	for (auto topLevelF : topLevelFuncs){
		_DFSTopSort(topLevelF, colors, NonTopoOrder);
	}

    return NonTopoOrder;
}
