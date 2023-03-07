#include <sstream>
#include <iostream>
#include <string>
#include <list>
#include <queue>
#include <stack>
#include <algorithm>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Attr.h"
#include "clang/Tooling/CommonOptionsParser.h"

#include "../framework/BasicChecker.h"
#include "../pugixml/pugixml.hpp"

#include <iostream>
#include <fstream>

using namespace clang;
using namespace llvm;
using namespace clang::driver;
using namespace clang::tooling;
using namespace std;

enum Color{WHITE,BLACK,GRAY};

typedef struct FunInfo{
	const DeclRefExpr* expr;
    ValueDecl *memExpr;
	ASTFunction* fun;
	CFGBlock* CFB;
    std::set<const ValueDecl*> lockSet;
    std::string accessType;
	FunInfo(const DeclRefExpr *e, ASTFunction* a, CFGBlock* b, std::set<const ValueDecl*> curSet, ValueDecl *member, std::string at)
	{
		expr = e;
		fun = a;
		CFB = b;
        lockSet = curSet;
        memExpr = member;
        accessType = at;
	}
} ISRInfo;

typedef struct SVAccessInfo{
    std::string SV,accessType,func;
    SVAccessInfo(std::string sv, std::string type, std::string fun)
    {
        SV = sv;
        accessType = type;
        func = fun;
    }
} SVAI;

typedef struct callInfo{
	ASTFunction* fun;
	std::string IRL;
	int pos;
	callInfo(std::string s, ASTFunction* a, std::string loc)
	{
		fun = a;
		IRL = s;
		int pos1 = loc.find_last_of('<');
		std::string str, tmpLine;
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
		tmpLine = str.substr(pos2+1);
		pos = atoi(tmpLine.c_str());
	}
} ReqIRQInfo;

class DataRace:public BasicChecker{
public:
	DataRace(ASTResource *resource, ASTManager *manager, CallGraph *call_graph, Config *configure):BasicChecker(resource, manager, call_graph, configure){};
	void check();
private:
	ASTFunction* entryFunc, *exitFunc;

	SourceManager *sm;

	std::string enableFun, disableFun, reqFun, thread;
	int enableArg, disableArg, reqIRL, reqISR, threadArg;

	//the second value is IRL
	std::unordered_map<ASTFunction*, ReqIRQInfo*> ISRs;
    //{line:SV,accessType,func}
	std::unordered_map<std::string, std::vector<SVAI>> SVAccesses;
	std::unordered_map<std::string, std::string> lockInfos;
	std::unordered_map<std::string, std::string> unlockInfos;
    // This is used for store global variables.
	std::unordered_map<const VarDecl*, std::vector<ISRInfo>> SVs;
    // This is used for store parameter variables.
    // Note that although the first element is only a type, the type is the parameter type.
	std::unordered_map<const Type*, std::vector<ISRInfo>> SVPs;
    std::unordered_map<const VarDecl*, const Type*> paraSVs;
    // Used to store tasks and their corresponding operations.
    std::unordered_map<const FunctionDecl*, const FieldDecl*> opers;
    // std::unordered_map<const Type*, std::vector<ISRInfo>> pSVs;
    std::vector<ASTFunction*> ISRCallees;
    // Stroe list of functions.
    std::vector<ASTFunction*> funs;
    // Store ISRs' name and their callees
    std::unordered_map<std::string, std::vector<ASTFunction*>> ISRsMap;
    std::set<ASTFunction*> tasks;
    std::set<const CallExpr*> lockExprs;

	int warningCount = 0;
	int multiCount = 0;
    int numOfBranches = 0;
    int numOfSVs = 0;
	std::ofstream fout;
	std::string ISRsInfo;


	int readConfig();
	void getEntryFunc();
	void getISRs();
	//get IRL, functionDecl for ISR and shared resource
	void analyzeISR(std::vector<ASTFunction*> &tarFuns);
	void IIRCFGConstruct(std::vector<ASTFunction*> &tarFuns);
	//analyze data race
	void serialize();
	void writeFunInfo();

    void analyzeFunPara(const Stmt* it, ASTFunction* astFun, CFGBlock* CFB, std::set<const ValueDecl*>& curSet);
	int SVPAnalysis(const Expr *expr, CFGBlock* CFB, unsigned int index, ASTFunction* fun, std::string accessType, std::set<const ValueDecl*> curSet);
	void addGSV(const Expr *expr, ASTFunction* astFun, CFGBlock* CFB, std::set<const ValueDecl*>& curSet, std::string accessType);
	void addPSV(const Expr *expr, ASTFunction* astFun, CFGBlock* CFB, std::set<const ValueDecl*>& curSet, std::string accessType);
	int SVAnalysis(const Expr *expr, CFGBlock* CFB, unsigned int index, ASTFunction* fun, std::string accessType, std::set<const ValueDecl*> curSet);
	std::string getExePath(CFGBlock* CFB, const DeclRefExpr* declRefExpr);
	std::string getStmtLoc(const Stmt* stmt);
    void analyzeCallees();
    bool ifMultiPar(ASTFunction* root, std::set<ASTFunction*> &reportedFunc);

	bool ifRaceBeforeReqIRQ(const Expr *expr, ASTFunction* fun,  ReqIRQInfo* reqInfo);
	std::unordered_map<const Expr*, std::string> analyseExpr(const Stmt* it, bool ifWriteOnly);

	const DeclRefExpr* getVarDecl(const Expr *expr, bool ifNeedGlobal, ValueDecl **member);

	void _DFSTopSort(ASTFunction *i, std::unordered_map<ASTFunction*, Color>& colors, std::vector<ASTFunction*> &NonTopoOrder);
	std::vector<ASTFunction *> getNonTopoOrder(std::vector<ASTFunction *> topLevelFuncs);
	void writingFile(std::string str, ASTFunction *astFunc, ASTFunction *isrFun);
    void recordAccessInfo(const Expr *expr, const VarDecl* sv, std::string accessType, ASTFunction* func);

    void calInState(CFGBlock* block,  std::set<const ValueDecl*>& inLockSet,  std::unordered_map<CFGBlock*, std::set<const ValueDecl*>> outBlockLockSet);
    void analyzeOnePair(const Expr *expr, ASTFunction* fun, std::string accessType, ISRInfo&pair, std::string out, std::set<const ValueDecl*> &curSet, ValueDecl *valDecl, const DeclRefExpr *declRef);
    bool lockCal(const Stmt* it, std::set<const ValueDecl*>& inLockSet);
    void addPara(ASTFunction *astFun);
    std::string getISRName(ASTFunction *fun);
    bool ifReported(const DeclRefExpr *expr1, const DeclRefExpr *expr2);
};
