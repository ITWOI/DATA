
#include "BasicChecker.h"

ASTResource* BasicChecker::resource = nullptr;
ASTManager* BasicChecker::manager = nullptr;
CallGraph* BasicChecker::call_graph = nullptr;
Config* BasicChecker::configure = nullptr;

BasicChecker::BasicChecker(ASTResource *resource, ASTManager *manager, CallGraph *call_graph, Config *configure) {
    
    this->resource = resource;
    this->manager = manager;
    this->call_graph = call_graph;
    this->configure = configure;
}

void BasicChecker::check() {

    for (auto F : resource->getFunctions()) {
        auto & it = BasicChecker::manager->getCFG(F);
        for (CFGBlock *block : *it) {
            std::cout << block->getBlockID() << std::endl;
            block->dump();
        }
        std::cout << std::endl;
    }


}
