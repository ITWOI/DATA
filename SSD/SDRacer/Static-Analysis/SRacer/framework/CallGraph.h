
#ifndef CALL_GRAPH_H
#define CALL_GRAPH_H

#include <unordered_map>

#include "ASTManager.h"

class CallGraphNode {

public:
    
    CallGraphNode(ASTFunction *F) {
        this->F = F;
    }
    
    void addParent(ASTFunction *F) {
        parents.push_back(F);
    }

    void addChild(ASTFunction *F) {
        children.push_back(F);
    }
   
    ASTFunction *getFunction() const {
        return F;
    }

    const std::vector<ASTFunction *> &getParents() const {
        return parents;
    }

    const std::vector<ASTFunction *> &getChildren() const {
        return children;
    }
	
private:
	 
	friend class NonRecursiveCallGraph;
	 
    ASTFunction *F;

    std::vector<ASTFunction *> parents;
    std::vector<ASTFunction *> children;

};

class CallGraph {
    
public:
    CallGraph(ASTManager &manager, const ASTResource &resource);
    ~CallGraph();

    const std::vector<ASTFunction *> &getTopLevelFunctions() const;

    ASTFunction *getFunction(FunctionDecl *FD) const;

    const std::vector<ASTFunction *> &getParents(ASTFunction *F) const;
    const std::vector<ASTFunction *> &getChildren(ASTFunction *F) const;

protected:
    std::unordered_map<std::string, CallGraphNode *> nodes;
    std::vector<ASTFunction *> topLevelFunctions;
	CallGraphNode* getNode(ASTFunction* f);

};

#endif
