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

static int num_of_elements;


#define LEVELS 25//log n (10^7)


int getRandomHeight(){
    unsigned int r = rand();

    r = r * 1103515245 + 12345;
    r &= (1u << (LEVELS - 1)) - 1;
        /* uniformly distributed bits => geom. dist. level, p = 0.5 */
    int level = __builtin_ctz(r) + 1;

    if (!(1 <= level && level <= 32)) {
        level = getRandomHeight();
        assert(1 <= level && level <= 32);
        return level;
    }

    assert(1 <= level && level <= 32);
    return level;
}




struct skiplistNode {
    /*array of pointers , entry i points to "next" skiplistNode on level i
      the last bit in next[i] indicates whether or not next[i] is deleted or not */
public:
    bool isInserting;
    int key;
    int value;
    int levels;
    std::vector<skiplistNode *> next;
    record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,skiplistNode> * mgr; //todo - check DEBRA


    skiplistNode(int key, int data, int levels, skiplistNode *val = nullptr) : isInserting(true), key(key), value(data),
                                                                               levels(levels) {
        for (int i = 0; i < levels; i++) {
            next.push_back(val);
        }
    }

    skiplistNode() {}



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

    /* sets the last bit of the address to 1*/
    void setIsNextNodeDeleted(int level) {
        this->next[level] = ((skiplistNode *) (((uintptr_t)(this->next[level])) | 1));
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


class skiplist {
public:
    skiplistNode *head;
    skiplistNode *tail;
    int levels = LEVELS;
    int BOUND_OFFSET = 10000;
    int size = 0;
    std::mutex * sizelock = new std::mutex();
    bool restructuring = false;
    std::vector<skiplistNode * > to_delete;
    volatile bool safe;
    record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,skiplistNode> * mgr;

    skiplist(int levels) {
        this->tail = new skiplistNode(INT_MAX, 0, LEVELS);
        this->tail->isInserting = false;
        this->head = new skiplistNode(INT_MIN, 0, LEVELS, this->tail);
        this->head->isInserting = false;
        mgr = new record_manager<reclaimer_debra<>,allocator_new<>,pool_none<>,skiplistNode>(2,SIGQUIT);
    }

    ~ skiplist(){
        std::cout<<"in skiplist dest" <<std::endl;
        delete head;
        delete tail;
        delete sizelock;
    }

    void locatePreds(int k, std::vector<skiplistNode *> &preds, std::vector<skiplistNode *> &succs,
                     skiplistNode *del, int tid) {
        mgr->enterQuiescentState(tid); //todo - do i need this if there is no "new" or "delete"?
        int i = this->levels - 1;
        skiplistNode * pred;
        pred = this->head;

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
        int i = this->levels - 1;
        skiplistNode * pred = this->head;
        skiplistNode * cur;
        skiplistNode * h;
        while (i > 0) {
            h = this->head->next[i];//record observed head
            bool is_h_next_deleted = h->getIsNextNodeDeleted();
            skiplistNode *cur = pred->next[i]; //take one step forwarad
            bool is_cur_next_deleted = cur->getIsNextNodeDeleted();
            if (!is_h_next_deleted) {
                i--;
                continue;
            }
            //traverse level until non-marked node is found
            while (is_cur_next_deleted) {
                pred = cur;
                cur = pred->next[i];
                is_cur_next_deleted = cur->getIsNextNodeDeleted();
            }
            if(!pred->getIsNextNodeDeleted())//todo - delete
                std::cout<<"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! "<<pred->key << ", "<<pred->value<<std::endl;

            if (__sync_bool_compare_and_swap(&this->head->next[i], h, cur)) { // if CAS fails, the same operation will be done for the same level
                i--;
            }
            else{
                std::cout<<"CAS failed" <<std::endl;
            }
        }
    }

    skiplistNode *deleteMin(int tid) {
        mgr->enterQuiescentState(tid);
        std::cout<<"in delete min"<<std::endl;

        skiplistNode *x = this->head;
        std::cout<<"hi . head = "<<x->key<<std::endl;
        std::cout<<"hi . head->next = "<<x->next[0]->key<<std::endl;
        int offset = 0;
        skiplistNode *newHead = nullptr;
        skiplistNode *observedHead_marked = x->getNextNodeMarked(0); //first real head (after sentinel)
        skiplistNode * observedHead_unmarked = x->getNextNodeUnmarked(0);
        bool isNextNodeDeleted;
        skiplistNode *next;
        int i= 0;
        do {
            i++;
            next = x->getNextNodeUnmarked(0);
            isNextNodeDeleted = x->getIsNextNodeDeleted();
            if (x->getNextNodeUnmarked(0) == this->tail) { //if queue is empty - return
                this->sizelock->lock();
                this->size = 0;
                this->sizelock->unlock();
                return nullptr;
            }
            if (x->isInserting && !newHead) {
                newHead = x; //head may not surpass pending insert, inserting node is "newhead".
            }
            skiplistNode *nxt = __sync_fetch_and_or(&x->next[0], 1); //logical deletion of successor to x.
            offset++;
            x = next;
        } while (isNextNodeDeleted);

        int v = x->value;

        this->sizelock->lock();
        if (this->size > 0)
            this->size--;//todo - delete this
        this->sizelock->unlock();


        if (offset < this->BOUND_OFFSET) {//if the offset is big enough - try to perform memory reclamation
            std::cout<<"returning x = "<<x->key<<std::endl;
            return x;
        }
        if (newHead == 0) {
            newHead = x;
        }

        if (__sync_bool_compare_and_swap(&this->head->next[0], observedHead_marked, newHead)) {
            if (__sync_bool_compare_and_swap(&this->restructuring, false, true)) {
                restructure();
                skiplistNode * cur = observedHead_unmarked;
                int i = 0;
                while (cur != newHead) {
                    next = cur->getNextNodeUnmarked(0);
                    cur = next;
                    i++;
                }

                if (__sync_bool_compare_and_swap(&this->restructuring, true, false)) {
                    std::cout<<"this->restructuring = false. this->to_delete.size() = "<< this->to_delete.size() << std::endl;

                }
                else{
                    std::cout<<"something really really bad happened"<< this->to_delete.size() << std::endl;

                }
            }
        }
        std::cout<<"leaving delete min"<<std::endl;

        return x;
    }


    void insert(int key, int val, int tid) {
        mgr->enterQuiescentState(tid);
        int height = getRandomHeight();
//        skiplistNode * newNode = new skiplistNode(key, val, height);
        skiplistNode* newNode = mgr->template allocate<skiplistNode>(tid); //todo - what the fuck
        newNode->key=key;
        newNode->value=val;
        newNode->levels = height;
        newNode->isInserting = true;
        for (int i = 0; i < levels; i++) {
            newNode->next.push_back(nullptr);
        }


        sizelock->lock();
        this->to_delete.push_back(newNode);
        sizelock->unlock();
        std::vector < skiplistNode * > preds(this->levels, nullptr);
        std::vector < skiplistNode * > succs(this->levels, nullptr);
        skiplistNode *del = nullptr;
        do {
            std::cout << "before locate preds" <<std::endl;
            this->locatePreds(key, preds, succs, del, tid);
            std::cout<<"after locate preds"<<std::endl;

//            newNode->next[0] = succs[0];
            newNode->next.push_back(succs[0]);

            std::cout<<"after             newNode->next[0] = succs[0];"<<std::endl;
        } while (!__sync_bool_compare_and_swap(&preds[0]->next[0], succs[0], newNode));

        std::cout<<"!!!!!!!!!!!!!!!!!!!!!!!!1 " << head->next[0]->key<<std::endl;

        int i = 1;
        std::cout<<"before first wile"<<std::endl;
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
        std::cout<<"after first wile"<<std::endl;

        sizelock->lock();
        this->size++;
        sizelock->unlock();

        newNode->isInserting = false; //allow batch deletion past this node

    }

    void printSkipList() {
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "printing skiplist, levels = "<< this->levels << std::endl;
        skiplistNode *node = this->head;
        for (int i = 0; i < this->levels; i++) {
            std::cout << "level " << i << " , head ";
            skiplistNode *nextNode = node->getNextNodeUnmarked(i);
            bool isNextDeleted = node->getIsNextNodeDeleted();
            bool to_continue = true;
            while (to_continue) {
                if (isNextDeleted && i == 0)
                    std::cout << "~";
                std::cout << "(" << nextNode->value << ", " << nextNode->key << ") ";
                isNextDeleted = nextNode->getIsNextNodeDeleted();
                nextNode = nextNode->getNextNodeUnmarked(i);
                if (!nextNode) {
                    to_continue = false;
                } else
                    to_continue = true;
            }
            std::cout << std::endl;
        }
        std::cout << "--------------------------------------------------" << std::endl;
    }

};//skiplist


struct Vertex {
    unsigned int index;
    vector <pair<Vertex *, int>> neighbors; // Vertex + weight of edge
    unsigned int dist;
    pair<int, int> location; // location = queue index, heap index

};


class Graph {
public:
    int source;//why is this needed?
    vector<Vertex *> vertices;
    Graph() {};

    ~Graph() {};//todo
};


//////////////////////////////////////////////////////////////////
/// DIJKSTRA ///

class threadInput {
public:
    bool *done;
    skiplist *queue;
    Graph *G;
    std::mutex **distancesLocks;
    int tid;
    int * distances;

    threadInput(bool *done, skiplist *queue, Graph *G, int * distances,
                std::mutex **distancesLocks, int tid) {
        this->done = done;
        this->queue = queue;
        this->G = G;
        this->distances = distances;
        this->distancesLocks = distancesLocks;
        this->tid = tid;
    }
};

void *parallelDijkstra(void *void_input) {
    threadInput * input = (threadInput *) void_input;
    bool * done = input->done;
    skiplist *queue = input->queue;
    Graph *G = input->G;
    int tid = input->tid;
    Vertex *curr_v;
    Vertex *neighbor;
    bool explore = true;
    int curr_dist = -1;
    int alt;
    int weight;
    int num_of_threads = 2;
    while (true) {
        if (queue->size % 100000 == 0){
            std::cout<<queue->size<<std::endl;
        }

        if (queue->size <=0){
            done[tid] = true;
        }
        else
            done[tid] = false;

        skiplistNode * shortest_distance_node = nullptr;
        if (!done[tid])
            if (tid == 1)
                std::cout<<"before delete min"<<std::endl;
            shortest_distance_node = queue->deleteMin(tid);
            if (tid == 1)
                std::cout<<"after delete min"<<std::endl;
        else{
            int k = 0;
            while (done[k] && k < num_of_threads)
                k++;
            if (k == num_of_threads)
                return NULL;
            else
                continue;
        }


        if (shortest_distance_node == nullptr) {
            if (tid == 1)
                std::cout << "min offer is nullpter " <<std::endl;
            done[tid] = true;
            int k = 0;
            while (done[k] && k < num_of_threads)
                k++;
            if (k == num_of_threads)
                return NULL;
            else
                continue;
        }

        done[tid] = false;
        curr_v = G->vertices[shortest_distance_node->value];
        std::cout<<"shortest distance node = " << shortest_distance_node->key << std::endl;

        curr_dist = shortest_distance_node->key;//todo - check this


        ////critical section - updating distance vector//////
        if (curr_dist < input->distances[shortest_distance_node->value]) {
            input->distances[shortest_distance_node->value] = curr_dist; //todo remove field dist from vertex
            explore = true;
        } else {
            explore = false;
        }
        ////end of critical section/////

        int inserted = 0;
        if (tid == 1) {
            std::cout << "before explore" << std::endl;
        }

        if (explore) {
            bool to_insert = false;

            for (int i = 0; i < (curr_v->neighbors.size()); i++) {
                to_insert = false;
                neighbor = curr_v->neighbors[i].first;
                weight = curr_v->neighbors[i].second;
                alt = curr_dist + weight;
                input->distancesLocks[neighbor->index]->lock();
                if (alt < input->distances[neighbor->index])
                    to_insert = true;
                input->distancesLocks[neighbor->index]->unlock();
                if (to_insert) {
                    inserted++;
                    if (tid == 1)
                        std::cout << "before delete min" << std::endl;
                    queue->insert(alt, neighbor->index, tid);//todo- check this
                    if (tid == 1)
                        std::cout << "after delete min" << std::endl;
                }
            }
        }

    }//end of while
}



void dijkstra_shortest_path(Graph *G) {
    int num_of_theads = 2; //todo - optimize this
    skiplist *queue = new skiplist(LEVELS); //global
    Vertex *curr_v;
    int * distances = (int * )malloc(sizeof(int)*G->vertices.size());
    bool * done = (bool * )malloc(sizeof(bool)*num_of_theads);
    std::mutex ** distancesLocks = new std::mutex *[G->vertices.size()];


    //init locks
    for (int i = 0; i < G->vertices.size(); i++) {
        distancesLocks[i] = new std::mutex();
        distances[i] = INT_MAX;
    }
    queue->insert(0, 0, 0);//insert first element to queue. First element: key (dist) = 0, value(index) = 0 (this is the soure)

    pthread_t threads[num_of_theads];
    std::vector<threadInput*> to_delete;
    for (int i = 0; i < num_of_theads; i++) {
        done[i] = false;
        threadInput * input = new threadInput(done, queue, G, distances, distancesLocks, i); //todo - delete i
        to_delete.push_back(input);
        pthread_create(&threads[i], NULL, &parallelDijkstra, (void *) input);
    }

    for (long i = 0; i < num_of_theads; i++) {
        (void) pthread_join(threads[i], NULL);
        delete to_delete[i];
    }

    std::cout << "things to delete = " << queue->to_delete.size() <<std::endl;

    int i;
    for (i = 0; i <queue->to_delete.size(); i++){
        delete queue->to_delete.back();
        queue->to_delete.pop_back();
    }
//    std::cout<<i<<std::endl;

    delete queue;
    for (int i = 0; i < G->vertices.size(); i++) {
        delete distancesLocks[i];
    }


    ofstream myfile;
    myfile.open ("output.txt");
    for (int i = 0; i < G->vertices.size(); i++) {
        myfile <<distances[i] << std::endl;
    }
    myfile.close();

}

int main(int argc, char *argv[]) {
//    string pathToFile = argv[1];
    std::string pathToFile = "./input_full.txt";
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
    //Vertex* source = new Vertex();
    char *n;

    getline(f, firstLine); //get the first line of # nodes, # edges and source node

    // get source vertex
    char str[line.size() + 1];
    strncpy(str, firstLine.c_str(), firstLine.size() + 1);
    char *token = strtok(str, " ");
    int num_vertices = strtol(token, &n, 0);
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
        v1->dist = INT_MAX;
        v2->dist = INT_MAX;
        v2->index = v2_index;
        //Weight
        v1->neighbors.push_back(make_pair(v2, weight));
        v2->neighbors.push_back(make_pair(v1, weight));
    }   //when we reached here - done inserting all nodes to graph
    G->vertices[0]->dist = 0;
    dijkstra_shortest_path(G);
    return 0;
}