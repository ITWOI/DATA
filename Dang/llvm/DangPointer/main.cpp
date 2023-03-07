
#include <string>
#include <iostream>
#include <fstream>
#include <ctime>
 

#include "llvm-c/Target.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/CommonOptionsParser.h"

#include "framework/ASTManager.h"
#include "framework/CallGraph.h"
#include "framework/Config.h"
#include "framework/BasicChecker.h"
#include "framework/Logger.h"

#include "dangling-pointer/DanglingPointer.h"
using namespace clang;
using namespace llvm;
using namespace clang::tooling;

int main(int argc, const char *argv[]) {

    if (argc != 4 && argc != 5) {
        std::cout << "usage: huawei-checker astList.txt config.txt pathtoBlackWhiteList [pathToReport]" << std::endl;
    }
    
    std::string pathToReport = "./";
    if (argc == 5) {
        pathToReport = argv[4];
        if (pathToReport[pathToReport.size() - 1] != '\\' && pathToReport[pathToReport.size() - 1] != '/')
            pathToReport += "/";
    }

    ofstream process_file(pathToReport + "time.txt");
	if(!process_file.is_open()){	
		cerr << "output path: " + pathToReport + " does not exist\n";
        return -1;
	}
	clock_t startCTime , endCTime; 
	startCTime = clock();

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    
    std::vector<std::string> ASTs = initialize(argv[1]);
    
    Config configure(argv[2]);
    
    std::unordered_map<std::string, std::string> content;
    content["path"] = pathToReport;
    configure.add("pathToReport", content);

	string blackWhiteListDir="./";

	if(argc >= 4){
		blackWhiteListDir=argv[3];
        if (blackWhiteListDir[blackWhiteListDir.size() - 1] != '\\' && blackWhiteListDir[blackWhiteListDir.size() - 1] != '/')
            blackWhiteListDir += "/";
	}
	
	process_file<<"Starting initialize AST"<<endl;
    clock_t start, end;
    start = clock();

    ASTResource resource;
    ASTManager manager(ASTs, resource, configure);
    CallGraph call_graph(manager, resource);
    end = clock();
    unsigned sec = unsigned((end-start)/CLOCKS_PER_SEC);
    unsigned min = sec/60;
    process_file<<"Time: "<<min<<"min"<<sec%60<<"sec"<<endl;
    process_file<<"End of initialize AST\n-----------------------------------------------------------"<<endl;

	auto enable = configure.getOptionBlock("CheckerEnable");
	
    Logger::configure(configure);
	if(enable.find("danglingPointer")->second == "true"){		 
		process_file<<"Starting danglingPointer check"<<endl;
		clock_t start,end;
		start = clock();

		DanglingPtr checker(&resource, &manager, &call_graph, &configure);
		checker.check();

		end = clock();
		unsigned sec = unsigned((end-start)/CLOCKS_PER_SEC);
		unsigned min = sec/60;
		process_file<<"Time: "<<min<<"min"<<sec%60<<"sec"<<endl;
		process_file<<"End of danglingPointer check\n-----------------------------------------------------------"<<endl;
	}
	endCTime = clock();
	sec = unsigned((endCTime-startCTime)/CLOCKS_PER_SEC);
	min = sec/60;
	process_file<<"-----------------------------------------------------------\nTotal time: "<<min<<"min"<<sec%60<<"sec"<<endl;
    return 0;
}

