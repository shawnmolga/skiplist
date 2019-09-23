#include <iostream>
#include <atomic>
#include <assert.h>
#include <thread>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <mutex>
#include<bits/stdc++.h>
#include <string>
#include "recordmgr/record_manager.h"
#include <chrono>

using namespace std;

#define LEVELS 30
#define THREADS_NUM 80


struct Vertex {
public:    
    unsigned int index;
    vector <pair<Vertex *, int>> neighbors; // Vertex + weight of edge
    unsigned int dist;
};


class Offer {
public :
    Vertex *vertex;
    int dist;

    Offer(Vertex *vertex, int dist) : vertex(vertex), dist(dist) {}
    Offer(){};

};

int getRandomHeight(){
    unsigned int r = rand();
    r = r * 1103515245 + 12345;
    r &= (1u << (LEVELS - 1)) - 1;
    int level = __builtin_ctz(r) + 1;
    if (!(1 <= level && level <= LEVELS)) {
        level = getRandomHeight();
        assert(1 <= level && level <= LEVELS);
        return level;
    }
    assert(1 <= level && level <= LEVELS);
    return level;
}

class skiplistNode {
public:
    bool isInserting;
    int key;
    int value;
    int levels;
    std::vector<skiplistNode *> next;

    skiplistNode(int key, int data, int levels, skiplistNode *val = nullptr) : isInserting(true), key(key), value(data),
                                                                               levels(levels) {
        for (int i = 0; i < levels; i++) {
            next.push_back(val);
        }
    }

    skiplistNode() {}

    ~skiplistNode(){}

    bool getIsNextNodeDeleted() {
        if (this->next.size() > 0) {
            uintptr_t  address = (uintptr_t) this->next[0];
            uintptr_t isNextDeleted = address & 1;
            if (isNextDeleted)
                return true;
            else
                return false;
        }
        else
            return false;
    }

    /* returns next node when the last bit is always 0 (real address) */
    skiplistNode *getNextNodeMarked(int level) {
        return this->next[level];
    }

    /* returns next node when the last bit is either 0 or 1, depending whether
     * or not the node is marked as deleted */
    skiplistNode *getNextNodeUnmarked(int level) {
        return (skiplistNode *) (((uintptr_t)(this->next[level])) & ~1);
    };

};

bool isNodeDeleted(skiplistNode * node){
    return (((uintptr_t)(node)) & 1);
}

skiplistNode * getNodeUnmarked(skiplistNode * node){
    return ((skiplistNode *)(((uintptr_t)(node)) & ~1));
}

skiplistNode * getNodeMarked(skiplistNode * node){
    return ((skiplistNode *)(((uintptr_t)(node)) | 1));   
}

class skiplist {
public:
    skiplistNode * head;
    skiplistNode * tail;
    int levels = LEVELS;
    int BOUND_OFFSET = 100;
    bool restructuring = false;
    record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,skiplistNode> * NodeMgr; 
    record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,Offer> * OfferMgr; 


    skiplist(int levels) {
        NodeMgr = new record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,skiplistNode>(THREADS_NUM,SIGQUIT);
        OfferMgr = new record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,Offer>(THREADS_NUM,SIGQUIT);
        this->tail = new skiplistNode(INT_MAX, 0, LEVELS);
        this->tail->isInserting = false;
        this->head = new skiplistNode(INT_MIN, 0, LEVELS, this->tail);
        this->head->isInserting = false;
    }


    ~skiplist(){
        delete NodeMgr;

        skiplistNode * cur, * pred;
        cur = this->head;
        while (cur != this->tail) {
            pred = cur;
            cur = getNodeUnmarked(pred->next[0]);
            if (pred){
                delete pred;
            }
        }

        delete  this->tail;
    }


    void locatePreds(int k, std::vector<skiplistNode *> &preds, std::vector<skiplistNode *> &succs,
                     skiplistNode *del, int tid) {
        int i = this->levels - 1;
        skiplistNode * pred = this->head;
        while (i >= 0) {
            skiplistNode * cur = pred->getNextNodeUnmarked(i);
            int isCurDeleted = pred->getIsNextNodeDeleted();
            bool isNextDeleted = cur->getIsNextNodeDeleted();
            while (cur->key < k || isNextDeleted || ((i == 0) && isCurDeleted)) {
                if ((isCurDeleted) && i == 0) { //if there is a level 0 node deleted without a "delete flag" on:
                    del = cur;
                }
                pred = cur;
                cur = pred->getNextNodeUnmarked(i);
                isCurDeleted = pred->getIsNextNodeDeleted();
                isNextDeleted = cur->getIsNextNodeDeleted();
            }
            preds[i] = pred;
            succs[i] = cur;
            i--;
        }
    }


    void restructure() {
        int i = LEVELS - 1;
        skiplistNode * pred = this->head;
        skiplistNode * cur;
        skiplistNode * h;

        pred = this->head;
        while (i > 0) {
            h = this->head->next[i]; /* record observed head */
            cur = pred->next[i]; /* take one step forward from pred */
            if (!isNodeDeleted(h->next[0])) {
                i--;
                continue;
            }
            while(isNodeDeleted(cur->next[0])) {
                pred = cur;
                cur = pred->next[i];
            }
            assert(isNodeDeleted(pred->next[0]));
            if (__sync_bool_compare_and_swap(&this->head->next[i],h,cur))
                i--;
        }
    }

    
    skiplistNode *deleteMin(int tid) {

        skiplistNode * x = this->head;
        int offset = 0;
        skiplistNode * newhead = NULL;
        skiplistNode * obs_head = x->next[0];
        skiplistNode *cur;
        int lvl = 0;
        skiplistNode * nxt = NULL;

        do {
            nxt = x->next[0];
            if (getNodeUnmarked(nxt) == this->tail) {
                return nullptr;
            }
            if (newhead == NULL && x->isInserting){
                newhead = x;
            }
            nxt = __sync_fetch_and_or(&x->next[0], 1);
            offset++;
            x = getNodeUnmarked(nxt);

        } while ( isNodeDeleted(nxt) );

        assert(!isNodeDeleted(x));
        
        if (offset <= this->BOUND_OFFSET){
            return x;
        }
        if (newhead == NULL){
            newhead = x;
        }
        if (this->head->next[0] != obs_head){
            return x;
        } 
        
        if (__sync_bool_compare_and_swap(&this->restructuring, false, true))
        {
            if (__sync_bool_compare_and_swap(&this->head->next[0], obs_head, getNodeMarked(newhead)))
            {

                restructure();

                cur = getNodeUnmarked(obs_head);
                while (cur != getNodeUnmarked(newhead)) {
                    nxt = getNodeUnmarked(cur->next[0]);
                    assert(isNodeDeleted(cur->next[0]));
                    if (cur){
                        NodeMgr->retire(tid, cur);

                    }
                    cur = nxt;
                }

            }
            if (!__sync_bool_compare_and_swap(&this->restructuring, true, false)){
                std::cout<<"bAD BAD BAD"<<std::endl;

            }
        }
     
        return x;
    }

    bool peekMin(int tid) {
        skiplistNode * x = this->head;
        int offset = 0;
        skiplistNode * newhead = NULL;
        skiplistNode * obs_head = x->next[0];
        skiplistNode *cur;
        int lvl = 0;
        skiplistNode * nxt = NULL;

         do {
            nxt = x->next[0];
            if (getNodeUnmarked(nxt) == this->tail) {
                return false;
            }
            if (newhead == NULL && x->isInserting){
                newhead = x;
            }
            offset++;
            x = getNodeUnmarked(nxt);
        } while (isNodeDeleted(nxt));
        return true;
    }



    void insert(int key, int val, int tid) {
        NodeMgr->enterQuiescentState(tid);
        int height = getRandomHeight();
        skiplistNode* newNode = NodeMgr->template allocate<skiplistNode>(tid);
        newNode->key=key;
        newNode->value=val;
        newNode->levels = height;
        newNode->isInserting = true;
        for (int i = 0; i < levels; i++) {
            newNode->next.push_back(nullptr);
        }
        std::vector < skiplistNode * > preds(this->levels, nullptr);
        std::vector < skiplistNode * > succs(this->levels, nullptr);
        skiplistNode *del = nullptr;
        int while_counter = 0;
        do {
            this->locatePreds(key, preds, succs, del, tid);
            newNode->next[0] = succs[0];
        } while (!__sync_bool_compare_and_swap(&preds[0]->next[0], succs[0], newNode));
        int i = 1;
        while (i < height) { //insert node at higher levels (bottoms up)
            newNode->next[i] = succs[i];
            if (newNode->getIsNextNodeDeleted() || succs[i]->getIsNextNodeDeleted()|| succs[i] == del)
                break; //new is already deleted
            if (__sync_bool_compare_and_swap(&(preds[i]->next[i]), succs[i], newNode)) {
                i++; // if successful - ascend to next level
            } else {
                this->locatePreds(key, preds, succs, del, tid);
                if (succs[0] != newNode)
                    break; //new has been deleted
            }
        }
        newNode->isInserting = false; //allow batch deletion past this node
        NodeMgr->leaveQuiescentState(tid);

    }

};


class Graph {
public:
    int source;
    vector<Vertex *> vertices;
    std::mutex **offerLock; //used to prevent multiple threads from concurrently changing the priority (distance) to the same node.
    Graph() {};

    ~Graph() {
        for(int i = 0; i < vertices.size(); i++){
            if(vertices[i])
                delete vertices[i];
        }
    };

};

void relax(skiplist* queue, int* distances, std::mutex **offersLocks, Offer **offers, Vertex* vertex, int alt, int tid) {
    queue->OfferMgr->enterQuiescentState(tid);
    Offer* curr_offer;
    offersLocks[vertex->index]->lock();
    if (alt < distances[vertex->index]) {
        curr_offer = offers[vertex->index];
        if (curr_offer == nullptr || alt < curr_offer->dist ){
            Offer* new_offer = queue->OfferMgr->template allocate<Offer>(tid); 
            new_offer->vertex = vertex;
            new_offer->dist = alt; 
            queue->insert(alt, vertex->index, tid);
            if (offers[vertex->index]) {
                queue->OfferMgr->retire(tid, offers[vertex->index]);
            }
            offers[vertex->index] = new_offer;
        }
    }
    offersLocks[vertex->index]->unlock();
    queue->OfferMgr->leaveQuiescentState(tid);
}

class threadInput {
public:
    bool *done;
    skiplist *queue;
    Graph *G;
    std::mutex **offersLocks;
    std::mutex **distancesLocks;
    int tid;
    int * distances;
    Offer ** offers;

    threadInput(bool *done, skiplist *queue, Graph *G, int * distances, std::mutex **offersLocks,
                std::mutex **distancesLocks, Offer ** offers, int tid) {
        this->done = done;
        this->queue = queue;
        this->G = G;
        this->distances = distances;
        this->offersLocks = offersLocks;
        this->distancesLocks = distancesLocks;
        this->offers = offers;
        this->tid = tid;
    }
};


bool finished_work(bool done[]){

    for (int i = 0; i < THREADS_NUM; i++){
        if(!done[i]){
            return false;
        }
    }
    return true;
}

void *parallelDijkstra(void *void_input) {
    threadInput * input = (threadInput *) void_input;
    bool * done = input->done;
    skiplist *queue = input->queue;
    Graph *G = input->G;
    Offer ** offers = input->offers;
    int tid = input->tid;
    Vertex *curr_v;
    Vertex *neighbor;
    bool explore = true;
    int curr_dist = -1;
    int alt;
    int weight;

    int* distances = input->distances;
    std::mutex ** offersLocks = input->offersLocks;
    std::mutex **distancesLocks = input->distancesLocks;

    while (true) {
        if (queue->peekMin(tid) == false){
            done[tid] = true;
        }

        skiplistNode * shortest_distance_node = nullptr;
        if (!done[tid]){
            shortest_distance_node = queue->deleteMin(tid);
        }
        else{
             if (finished_work(done)){
                return NULL;
            }
            else{
                continue;
            }
        }
        if (shortest_distance_node == nullptr) {
            if (finished_work(done)){
                return NULL;
            }
            else{
                continue;
            }
        }

        done[tid] = false;
        curr_v = G->vertices[shortest_distance_node->value];

        curr_dist = shortest_distance_node->key;//todo - check this


        distancesLocks[curr_v->index]->lock();
        if (curr_dist < distances[curr_v->index]) {
            distances[curr_v->index] = curr_dist; //todo remove field dist from vertex
            explore = true;
        } else {
            explore = false;
        }
        distancesLocks[curr_v->index]->unlock();
        

        if (explore) {
            for (int i = 0; i < (curr_v->neighbors.size()); i++) {
                neighbor = curr_v->neighbors[i].first;
                weight = curr_v->neighbors[i].second;
                alt = curr_dist + weight;
                relax(queue,distances,offersLocks,offers,neighbor,alt, tid);
            }
        }

    }
}


void dijkstra_shortest_path(Graph *G) {
    skiplist *queue = new skiplist(LEVELS); //global
    Offer *min_offer;
    Vertex *curr_v;
    int distances[G->vertices.size()];
    Offer *offers[G->vertices.size()];

    bool done[THREADS_NUM];
    for (int i = 0; i < THREADS_NUM; i++){
        done[i]=false;
    }

    std::mutex **offersLocks = new std::mutex *[G->vertices.size()];
    std::mutex **distancesLocks = new std::mutex *[G->vertices.size()];


    //init locks
    for (int i = 0; i < G->vertices.size(); i++) {
        offersLocks[i] = new std::mutex();
        distancesLocks[i] = new std::mutex();
        distances[i] = INT_MAX;
        offers[i]=NULL;
    }

    queue->insert(0, 0, 0);//insert first element to queue. First element: key (dist) = 0, value(index) = 0 (this is the soure)
    pthread_t threads[THREADS_NUM];

    std::vector<threadInput * > to_delete;


    for (int i = 0; i < THREADS_NUM; i++) {
        done[i] = false; //set thread[i] to 'not-done'
        threadInput *input = new threadInput(done, queue, G, distances, offersLocks, distancesLocks, offers, i);
        to_delete.push_back(input);
        pthread_create(&threads[i], NULL, &parallelDijkstra, (void *) input);
    }

    for (long i = 0; i < THREADS_NUM; i++)
        (void) pthread_join(threads[i], NULL);

    for (auto p : to_delete){
        delete p;
    } 
    to_delete.clear();


    for (int i = 0; i < G->vertices.size(); i++) {
    delete offersLocks[i];
    delete distancesLocks[i];
    if(offers[i])
        delete offers[i];
        
    }
    delete[] offersLocks;
    delete[] distancesLocks;
    delete queue;

    ofstream myfile;
    myfile.open ("output.txt");
    for (int i = 0; i < G->vertices.size(); i++) {
        myfile <<distances[i] << std::endl;
    }
    myfile.close();

}


int main(int argc, char *argv[]) {

    if (argc < 2){
        std::cout<<"missing name of file"<<std::endl;
        exit(1);
    }
    string pathToFile = argv[1];
    ifstream f;
    f.open(pathToFile);
    if (!f) {
        cerr << "Unable to open file " << pathToFile;
        exit(1);
    }
    Graph *G = new Graph();

    string line;
    string firstLine;
    int source_index;
    char *n;

    getline(f, firstLine); //get the first line of # nodes, # edges and source node

    // get source vertex
    char str[line.size() + 1];

    strncpy(str, firstLine.c_str(), firstLine.size() + 1);

    char *token = strtok(str, " ");

    int num_vertices = strtol(token, &n, 0);

    if (num_vertices <= 0){
        f.close();
        delete G;
        return 0;
    }

    token = strtok(NULL, " ");

    int num_edges = strtol(token, &n, 0);

    token = strtok(NULL, " ");

    source_index = strtol(token, &n, 0);

    G->source = source_index;

    G->vertices.resize(num_vertices);

    Vertex *source_vertex = new Vertex();
    source_vertex->dist = 0;
    source_vertex->index = 0;
    
    G->vertices[0] = source_vertex;

    for (int i = 1; i < num_vertices; i++) {
        G->vertices[i] = new Vertex();
    }

    while (getline(f, line)) {
        Vertex *source = nullptr;
        Vertex *v1 = nullptr;
        Vertex *v2 = nullptr;
        int v1_index;
        int v2_index;
        int weight;
        char *p;
        char *q;
        char *m;

        // copy line
        char str[line.size() + 1];
        strncpy(str, line.c_str(), line.size() + 1);

        // Returns first token
        token = strtok(str, " ");

        // get vertex v1 index
        v1_index = strtol(token, &p, 0);

        // get vertex v2 index
        v2_index = strtol(strtok(NULL, " "), &q, 0);

        v1 = G->vertices[v1_index];
        v2 = G->vertices[v2_index];
        // get edge weight
        weight = strtol(strtok(NULL, " "), &m, 0);

        v1->index = v1_index;
        v2->index = v2_index;
        //Weight
        v1->neighbors.push_back(make_pair(v2, weight));
        v2->neighbors.push_back(make_pair(v1, weight));

    }//done inserting all nodes to graph

    f.close();
    G->vertices[0]->dist = 0;
    dijkstra_shortest_path(G);
    delete G;
    return 0;
}