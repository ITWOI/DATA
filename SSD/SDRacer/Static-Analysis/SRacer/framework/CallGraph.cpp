
#include <iostream>
#include "CallGraph.h"

using namespace std;

CallGraph::CallGraph(ASTManager &manager, const ASTResource &resource) {
    
    for (ASTFunction *F : resource.getFunctions()) {
        CallGraphNode *node = new CallGraphNode(F);
        nodes.insert(std::make_pair(F->getFullName(), node));
	}
    
    for (auto &content : nodes) {
        CallGraphNode *node = content.second;
        FunctionDecl *FD = manager.getFunctionDecl(node->getFunction());
        auto v = common::getCalledFunctions(FD);

        for (FunctionDecl *called_func : v) {
            string name = common::getFullName(called_func);
			auto it = nodes.find(name);
			
            if (it != nodes.end()) {
				node->addChild(it->second->getFunction());
                it->second->addParent(node->getFunction());
            }
        }
    }
    
    for (ASTFunction *F : resource.getFunctions()) {
        if (nodes[F->getFullName()]->getParents().size() == 0) {
            topLevelFunctions.push_back(F);
        }
    }

}

CallGraph::~CallGraph() {
    for (auto &content : nodes) {
        delete content.second;
    }
}

const std::vector<ASTFunction *> &CallGraph::getTopLevelFunctions() const {
    return topLevelFunctions;
}

ASTFunction *CallGraph::getFunction(FunctionDecl *FD) const {
	std::string fullName = common::getFullName(FD);
    auto it = nodes.find(fullName);
    if (it != nodes.end()) {
        return it->second->getFunction();
    }
    return nullptr;
}

const std::vector<ASTFunction *> &CallGraph::getParents(ASTFunction *F) const {
    auto it = nodes.find(F->getFullName());
    return it->second->getParents();
}

const std::vector<ASTFunction *> &CallGraph::getChildren(ASTFunction *F) const {
    auto it = nodes.find(F->getFullName());
    return it->second->getChildren();
}

CallGraphNode* CallGraph::getNode(ASTFunction* f) {

	if(f == nullptr){
		return nullptr;
	}
    auto it = nodes.find(f->getFullName());
	
    if (it == nodes.end()) {
        return nullptr;
    }
    return it->second;

}

