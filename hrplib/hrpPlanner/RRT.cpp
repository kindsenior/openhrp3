// -*- mode: c++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
#include "Roadmap.h"
#include "RoadmapNode.h"
#include "ConfigurationSpace.h"
#include "RRT.h"

using namespace PathEngine;

static bool debug=false;
//static bool debug=true;

RRT::RRT(PathPlanner* plan) : Algorithm(plan) 
{
    // set default properties
    properties_["max-trials"] = "10000";
    properties_["eps"] = "0.1";

    Tstart_ = Ta_ = roadmap_;
    Tgoal_  = Tb_ = new Roadmap(planner_);

    extendFromStart_ = true;
    extendFromGoal_ = false;
}

RRT::~RRT() 
{
    delete Tgoal_;
}

int RRT::extend(Roadmap *tree, Configuration& qRand, bool reverse) {
    if (debug) std::cout << "RRT::extend("<< qRand << ", " << reverse << ")" 
                         << std::endl;

    RoadmapNode* minNode;
    double min;
    tree->findNearestNode(qRand, minNode, min);
    if (debug) std::cout << "nearest : pos = (" << minNode->position() 
                         << "), d = " << min << std::endl;

    if (minNode != NULL) {
        Mobility* mobility = planner_->getMobility();

        if (min > eps_){
            Configuration qRandOrg = qRand;
            qRand = mobility->interpolate(minNode->position(), qRand, eps_/min);
            if (debug) std::cout << "qRand = (" << qRand << ")" << std::endl;
            if (mobility->distance(minNode->position(), qRand) > min){
                std::cout << "distance didn't decrease" << std::endl;
                std::cout << "qRandOrg : (" << qRandOrg << "), d = " << min
                          << std::endl;
                std::cout << "qRand    : (" << qRand << "), d = " 
                          << mobility->distance(minNode->position(), qRand)
                          << std::endl;
                getchar();
            }
        }

        if (reverse){
            if (mobility->isReachable(qRand, minNode->position())){
                RoadmapNode* newNode = new RoadmapNode(qRand);
                tree->addNode(newNode);
                tree->addEdge(newNode, minNode);
                if (min <= eps_) {
                    if (debug) std::cout << "reached(" << qRand << ")"<< std::endl;
                    return Reached;
                }
                else {
                    if (debug) std::cout << "advanced(" << qRand << ")" << std::endl;
                    return Advanced;
                }
            } else {
                if (debug) std::cout << "trapped" << std::endl;
                return Trapped;
            }
        }else{
            if (mobility->isReachable(minNode->position(), qRand)){
                RoadmapNode* newNode = new RoadmapNode(qRand);
                tree->addNode(newNode);
                tree->addEdge(minNode, newNode);
                if (min <= eps_) {
                    if (debug) std::cout << "reached(" << qRand << ")"<< std::endl;
                    return Reached;
                }
                else {
                    if (debug) std::cout << "advanced(" << qRand << ")" << std::endl;
                    return Advanced;
                }
            } else {
                if (debug) std::cout << "trapped" << std::endl;
                return Trapped;
            }
        }
    }

    return Trapped;
}

int RRT::connect(Roadmap *tree,const Configuration &qNew, bool reverse) {
    if (debug) std::cout << "RRT::connect(" << qNew << ")" << std::endl;

    int ret = Reached;
    Configuration q = qNew;

    do {
        ret = extend(tree, q, reverse);
        q = qNew;
    } while (ret == Advanced);
    return ret;
}

void RRT::extractPath() {
    extractPath(path_);
}

void RRT::extractPath(std::vector<Configuration>& o_path) {
    //std::cout << "RRT::path" << std::endl;
    RoadmapNode* startMidNode = Tstart_->lastAddedNode();
    RoadmapNode* goalMidNode  = Tgoal_ ->lastAddedNode();

    o_path.clear();

    RoadmapNode* node;

    if (extendFromStart_){
        node = startMidNode;
        do {
            o_path.insert(o_path.begin(), node->position());
            node = node->parent(0);
        } while (node != NULL);
    }

    if (extendFromGoal_){
        node = goalMidNode;
        do {
            o_path.push_back(node->position());
            node = node->child(0);
        } while (node != NULL);
    }
#if 0
    startMidNode->children_.push_back(goalMidNode);
    goalMidNode->parent_ = startMidNode;
#endif
}

bool RRT::extendOneStep()
{
    Configuration qNew = planner_->getConfigurationSpace()->random();
    if (extendFromStart_ && extendFromGoal_){
        
        if (extend(Ta_, qNew, Tb_ == Tstart_) != Trapped) {
            if (connect(Tb_, qNew, Ta_ == Tstart_) == Reached) {
                return true;
            }
        }
        swapTrees();
    }else if (extendFromStart_ && !extendFromGoal_){
        if (extend(Tstart_, qNew) != Trapped) {
            Configuration p = goal_;
            int ret;
            do {
                ret = extend(Tstart_, p);
                p = goal_;
            }while (ret == Advanced);
            if (ret == Reached) return true;
        }
    }else if (!extendFromStart_ && extendFromGoal_){
        std::cout << "this case is not implemented" << std::endl;
        return false;
    }
    return false;
}

bool RRT::calcPath() 
{
    if (verbose_) std::cout << "RRT::calcPath" << std::endl;

    // 回数
    times_ = atoi(properties_["max-trials"].c_str());

    // eps
    eps_ = atof(properties_["eps"].c_str());

    if (verbose_){
        std::cout << "times:" << times_ << std::endl;
        std::cout << "eps:" << eps_ << std::endl;
    }

    RoadmapNode* startNode = new RoadmapNode(start_);
    RoadmapNode* goalNode  = new RoadmapNode(goal_);

    Tstart_->addNode(startNode);
    Tgoal_ ->addNode(goalNode);

    for (unsigned int i=0; i<extraGoals_.size(); i++){
        RoadmapNode *node = new RoadmapNode(extraGoals_[i]);
        Tgoal_->addNode(node);
    }

    bool isSucceed = false;
  
    for (int i=0; i<times_; i++) {
        //if (!isRunning_) break;
        if (verbose_){
            printf("%5d/%5dtrials : %5d/%5dnodes\r", i+1, times_, Tstart_->nNodes(),Tgoal_->nNodes());
            fflush(stdout);
        }
        if (isSucceed = extendOneStep()) break;
    }
  
    extractPath();
    Tgoal_->integrate(Tstart_);

    if (verbose_) {
        std::cout << std::endl << "fin.(calcPath), retval = " << isSucceed << std::endl;
    }
    return isSucceed;
}

void RRT::swapTrees()
{
    Roadmap *tmp = Ta_;
    Ta_ = Tb_;
    Tb_ = tmp;
}
