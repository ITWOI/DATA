
#include <fstream>
#include <iostream>

#include "Common.h"

#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/Frontend/CompilerInstance.h"

using namespace std;

namespace {

class ASTFunctionLoad : public ASTConsumer, public RecursiveASTVisitor<ASTFunctionLoad> {    

public:
    void HandleTranslationUnit(ASTContext &Context) override {
        TranslationUnitDecl *D = Context.getTranslationUnitDecl();
        TraverseDecl(D);
    }

    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (FD && FD->isThisDeclarationADefinition()) {
			// Add C++ method
			if (dyn_cast<CXXMethodDecl>(FD) != nullptr ) {
                functions.push_back(FD);
			}
            // Add C non-inline function 
            else if(!FD->isInlined()){
                functions.push_back(FD);
            }
        }
        return true;
    }

    const std::vector<FunctionDecl *> &getFunctions() const{
        return functions;
    }

private:
    std::vector<FunctionDecl *> functions;

};

class ASTVariableLoad : public RecursiveASTVisitor<ASTVariableLoad> {

public:
    bool VisitDeclStmt(DeclStmt *S) {
        for (auto D : S->decls()) {
            if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
                variables.push_back(VD);
            }
        }
        return true;
    }

    const std::vector<VarDecl *> &getVariables() {
        return variables;
    }

private:
    std::vector<VarDecl *> variables;
};

class ASTCalledFunctionLoad : public RecursiveASTVisitor<ASTCalledFunctionLoad> {
    
public:
    bool VisitCallExpr(CallExpr *E) {
        if (FunctionDecl *FD = E->getDirectCallee()) {
            functions.insert(FD);
        }
        return true;
    }

    const std::vector<FunctionDecl *> getFunctions() {
        return std::vector<FunctionDecl *>(functions.begin(), functions.end());
    }

private:
    std::set<FunctionDecl *> functions;
};

class ASTCallExprLoad: public RecursiveASTVisitor<ASTCallExprLoad> {
    
public:
    bool VisitCallExpr(CallExpr *E) {
        call_exprs.push_back(E);
        return true;
    }
    
    const std::vector<CallExpr *> getCallExprs() {
        return call_exprs;
    }

private:
    std::vector<CallExpr *> call_exprs;
};

} // end of anonymous namespace

namespace common {

/** 
 * load an ASTUnit from ast file.
 * AST : the name of the ast file.
 */
std::unique_ptr<ASTUnit> loadFromASTFile(std::string AST) {
    
    FileSystemOptions FileSystemOpts;
    IntrusiveRefCntPtr<DiagnosticsEngine> Diags = CompilerInstance::createDiagnostics(new DiagnosticOptions());
    return ASTUnit::LoadFromASTFile(AST, Diags, FileSystemOpts);
}

/**
 * get all functions's decl from an ast context.
 */
std::vector<FunctionDecl *> getFunctions(ASTContext &Context) {
    
    ASTFunctionLoad load;
    load.HandleTranslationUnit(Context);
    return load.getFunctions();
}

/**
 * get all variables' decl of a function
 * FD : the function decl.
 */
std::vector<VarDecl *> getVariables(FunctionDecl *FD) {
    
    std::vector<VarDecl *> variables;
    variables.insert(variables.end(), FD->params().begin(), FD->params().end());

    ASTVariableLoad load;
    load.TraverseStmt(FD->getBody());
    variables.insert(variables.end(), load.getVariables().begin(), load.getVariables().end());

    return variables;
}

std::vector<FunctionDecl *> getCalledFunctions(FunctionDecl *FD) {
    
    ASTCalledFunctionLoad load;
    load.TraverseStmt(FD->getBody());
    return load.getFunctions();
}

std::vector<CallExpr *> getCallExpr(FunctionDecl *FD) {
    
    ASTCallExprLoad load;
    load.TraverseStmt(FD->getBody());
    return load.getCallExprs();
}

std::string getParams(FunctionDecl *FD) {
    
    std::string params = "";
    for (auto param : FD->params()) {
        params = params + param->getOriginalType().getAsString() + "  ";
    }
    return params;
}

std::string getFullName(FunctionDecl *FD) {
	
	std::string name = FD->getQualifiedNameAsString();
	
	name = name + "  " + getParams(FD);
    return name;
}

} // end of namespace common


std::string trim(std::string s)
{
    std::string result = s;
    result.erase(0,result.find_first_not_of(" \t"));
    result.erase(result.find_last_not_of(" \t")+1);
    return result;
}

std::vector<std::string> initialize(std::string astList) {
		
	std::vector<std::string> astFiles;

    std::ifstream fin(astList);
    std::string line;
	while(getline(fin,line))
	{
		line = trim(line);
		if(line=="")
			continue;
        std::string fileName = line;
		astFiles.push_back(fileName);
	}
	fin.close();

	return astFiles;
}

void common::printLog(std::string logString, common::CheckerName cn, int level, Config &c)
{
	auto block = c.getOptionBlock("PrintLog");
	int l = atoi(block.find("level")->second.c_str());
	switch(cn)
	{
	case common::CheckerName::taintChecker: 
		if(block.find("taintChecker")->second == "true" && level>=l)
		{
			llvm::errs()<<logString;
		}
		break;
	case common::CheckerName::danglingPointer: 
		if(block.find("danglingPointer")->second == "true" && level>=l)
		{
			llvm::errs()<<logString;
		}
		break;
	case common::CheckerName::arrayBound: 
		if(block.find("arrayBound")->second == "true" && level>=l)
		{
			llvm::errs()<<logString;
		}
		break;
	case common::CheckerName::recursiveCall: 
		if(block.find("recursiveCall")->second == "true" && level>=l)
		{
			llvm::errs()<<logString;
		}
		break;
	case common::CheckerName::divideChecker: 
		if(block.find("divideChecker")->second == "true" && level>=l)
		{
			llvm::errs()<<logString;
		}
		break;
	case common::CheckerName::memoryOPChecker: 
		if(block.find("memoryOPChecker")->second == "true" && level>=l)
		{
			llvm::errs()<<logString;
		}
		break;
	}
}

