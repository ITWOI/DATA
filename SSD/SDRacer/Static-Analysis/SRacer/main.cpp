
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

#include "data-race/datarace.h"

using namespace clang;
using namespace llvm;
using namespace clang::tooling;

int main(int argc, const char *argv[]) {
    ofstream process_file("time.txt");
	if(!process_file.is_open()){	
		cerr << "can't open time.txt\n";
        return -1;
	}
	clock_t startCTime , endCTime; 
	startCTime = clock();
	

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    
    std::vector<std::string> ASTs = initialize(argv[1]);
    
    Config configure(argv[2]);
    
    ASTResource resource;
    ASTManager manager(ASTs, resource, configure);
    CallGraph call_graph(manager, resource);

	auto enable = configure.getOptionBlock("CheckerEnable");
	
    Logger::configure(configure);


	if(enable.find("danglingPointer")->second == "true"){		 
		process_file<<"Starting DataRace check"<<endl;
		clock_t start,end;
		start = clock();

		DataRace checker(&resource, &manager, &call_graph, &configure);
		checker.check();

		end = clock();
		unsigned sec = unsigned((end-start)/CLOCKS_PER_SEC);
		unsigned min = sec/60;
		process_file<<"Time: "<<min<<"min"<<sec%60<<"sec"<<endl;
		process_file<<"End of DataRace check\n-----------------------------------------------------------"<<endl;
	}

	endCTime = clock();
	unsigned sec = unsigned((endCTime-startCTime)/CLOCKS_PER_SEC);
	unsigned min = sec/60;
	process_file<<"-----------------------------------------------------------\nTotal time: "<<min<<"min"<<sec%60<<"sec"<<endl;
    return 0;
}

