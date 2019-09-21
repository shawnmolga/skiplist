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


/*array of pointers , entry i points to "next" skiplistNode on level i
the last bit in next[i] indicates whether or not next[i] is deleted or not */
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

#define get_marked_ref(_p)      ((skiplistNode *)(((uintptr_t)(_p)) | 1))
#define get_unmarked_ref(_p)    ((skiplistNode *)(((uintptr_t)(_p)) & ~1))
#define is_marked_ref(_p)       (((uintptr_t)(_p)) & 1)


class skiplist {
public:
    skiplistNode * head;
    skiplistNode * tail;
    int levels = LEVELS;
    int BOUND_OFFSET = 100;
    int size = 0;
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
        // delete offer_mgr;
        delete NodeMgr;

        skiplistNode * cur, * pred;
        cur = this->head;
        while (cur != this->tail) {
            pred = cur;
            cur = get_unmarked_ref(pred->next[0]);
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

//     void restructure() {
//         int i = this->levels - 1;
//         skiplistNode * pred = this->head;
//         skiplistNode * cur;
//         skiplistNode * h;
//         while (i > 0) {//todo - should this be i>=0???
//             h = this->head->next[i];//record observed heada
//             bool is_h_next_deleted = h->getIsNextNodeDeleted();
//             skiplistNode *cur = pred->next[i]; //take one step forwarad
//             bool is_cur_next_deleted = cur->getIsNextNodeDeleted();
//             if (!is_marked_ref(h->next[0])) {
//                 i--;
//                 continue;
//             }
//             //traverse level until non-marked node is found
//             while (is_marked_ref(cur->next[0])) {
//                 pred = cur;
//                 cur = pred->next[i];
//                 is_cur_next_deleted = cur->getIsNextNodeDeleted();
//             }
//             if(!is_marked_ref(pred->next[0]))//todo - delete
//                 std::cout<<"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! "<<pred->key << ", "<<pred->value<<std::endl;
// //            assert(pred->getIsNextNodeDeleted());//todo - delete

//             if (__sync_bool_compare_and_swap(&this->head->next[i], h, cur)) { // if CAS fails, the same operation will be done for the same level
//                 i--;
//             }
//         }
//     }

    void restructure() {
        skiplistNode *pred, *cur, *h;
        int i = LEVELS - 1;

        pred = this->head;
        while (i > 0) {
            /* the order of these reads must be maintained */
            h = this->head->next[i]; /* record observed head */
            cur = pred->next[i]; /* take one step forward from pred */
            if (!is_marked_ref(h->next[0])) {
                i--;
                continue;
            }
            /* traverse level until non-marked node is found
             * pred will always have its delete flag set
             */
            while(is_marked_ref(cur->next[0])) {
                pred = cur;
                cur = pred->next[i];
            }
            assert(is_marked_ref(pred->next[0]));
        
            /* swing head pointer */
            if (__sync_bool_compare_and_swap(&this->head->next[i],h,cur))
                i--;
        }
    }

//     skiplistNode *deleteMin(int tid) {
//         skiplistNode *x = this->head;
//         int offset = 0;
//         skiplistNode *newHead = nullptr;
//         skiplistNode *observedHead_marked = x->getNextNodeMarked(0); //first real head (after sentinel)
//         skiplistNode * observedHead_unmarked = x->getNextNodeUnmarked(0);
//         bool isNextNodeDeleted;
//         skiplistNode *next;
//         skiplistNode * cur;
//         do {
//             offset++;

//             // next = x->getNextNodeUnmarked(0);

//             next = x->next[0];


//             isNextNodeDeleted = x->getIsNextNodeDeleted();
//             if (get_unmarked_ref(next) == this->tail) { //if queue is empty - return
//                 this->sizelock->lock();
//                 this->size = 0;
//                 this->sizelock->unlock();
//                 return nullptr;
//             }
//             if ( !newHead && x->isInserting) {
//                 newHead = x; //head may not surpass pending insert, inserting node is "newhead".
//             }
//             next = __sync_fetch_and_or(&x->next[0], 1); //logical deletion of successor to x.
// //            std::cout<<"node key = " << x->key << ", value = "<< x->value<<std::endl;
// //            assert(x->getIsNextNodeDeleted());
//             // x = next;
//         } while ((x = get_unmarked_ref(next)) && is_marked_ref(next));
//         int v = x->value;


//         this->sizelock->lock();
//         if (this->size > 0)
//             this->size--;//todo - delete this
//         this->sizelock->unlock();

//        if (!newHead) {
//             newHead = x;
//         }

//         // std::cout<<"offset = "<<offset<<std::endl;

//         if (offset <= this->BOUND_OFFSET) {//if the offset is big enough - try to perform memory reclamation
//             return x;
//         }
 

//         if (__sync_bool_compare_and_swap(&this->head->next[0], observedHead_marked, newHead)) {
//         // if (__sync_bool_compare_and_swap(&this->restructuring, false, true)) {

//             restructure();
//             // if(!__sync_bool_compare_and_swap(&this->restructuring, true, false)){
//                 // std::cout<<"BAD BAD BAD!!!"<<std::endl;
//             // }
//         }
//         return x;
//     }


    
    skiplistNode *deleteMin(int tid) {
        int v = 0;
        skiplistNode *x, *nxt, *obs_head = NULL, *newhead, *cur;
        int offset, lvl;
        
        newhead = NULL;
        offset = lvl = 0;

        x = this->head;
        obs_head = x->next[0];

        do {
            offset++;

            /* expensive, high probability that this cache line has
             * been modified */
            nxt = x->next[0];

            // tail cannot be deleted
            if (get_unmarked_ref(nxt) == this->tail) {
                return nullptr;
            }

            /* Do not allow head to point past a node currently being
             * inserted. This makes the lock-freedom quite a theoretic
             * matter. */
            if (newhead == NULL && x->isInserting) newhead = x;

            /* optimization */
            if (is_marked_ref(nxt)) continue;
            /* the marker is on the preceding pointer */
            /* linearisation point deletemin */
            nxt = __sync_fetch_and_or(&x->next[0], 1);
        }
        while ( (x = get_unmarked_ref(nxt)) && is_marked_ref(nxt) );

        assert(!is_marked_ref(x));

        v = x->value;

        
        /* If no inserting node was traversed, then use the latest 
         * deleted node as the new lowest-level head pointed node
         * candidate. */
        if (newhead == NULL) newhead = x;

        /* if the offset is big enough, try to update the head node and
         * perform memory reclamation */
        if (offset <= 100) goto out;

        /* Optimization. Marginally faster */
        if (this->head->next[0] != obs_head) goto out;
        
        /* try to swing the lowest level head pointer to point to newhead,
         * which is deleted */
        if (__sync_bool_compare_and_swap(&this->restructuring, false, true))
        {
            if (__sync_bool_compare_and_swap(&this->head->next[0], obs_head, get_marked_ref(newhead)))
            {

                /* Update higher level pointers. */
                restructure();

                /* We successfully swung the upper head pointer. The nodes
                 * between the observed head (obs_head) and the new bottom
                 * level head pointed node (newhead) are guaranteed to be
                 * non-live. Mark them for recycling. */

                cur = get_unmarked_ref(obs_head);
                while (cur != get_unmarked_ref(newhead)) {
                    nxt = get_unmarked_ref(cur->next[0]);
                    assert(is_marked_ref(cur->next[0]));
                    if (cur){
                        NodeMgr->retire(tid, cur);

                        // std::cout<<cur<<std::endl;
                        // delete cur;
                        // std::cout<<"."<<std::endl;
                    }
                    cur = nxt;
                }

            }
            if (!__sync_bool_compare_and_swap(&this->restructuring, true, false)){
                std::cout<<"bAD BAD BAD"<<std::endl;

            }
        }
     out:
        return x;
    }


    bool peekMin(int tid) {
        int v = 0;
        skiplistNode *x, *nxt, *obs_head = NULL, *newhead, *cur;
        int offset, lvl;
        
        newhead = NULL;
        offset = lvl = 0;

        x = this->head;
        obs_head = x->next[0];

        do {
            offset++;

            /* expensive, high probability that this cache line has
             * been modified */
            nxt = x->next[0];

            // tail cannot be deleted
            if (get_unmarked_ref(nxt) == this->tail) {
                return false;
            }

            /* Do not allow head to point past a node currently being
             * inserted. This makes the lock-freedom quite a theoretic
             * matter. */
            if (newhead == NULL && x->isInserting) newhead = x;

            /* optimization */
            if (is_marked_ref(nxt)) continue;
            /* the marker is on the preceding pointer */
            /* linearisation point deletemin */
            // nxt = __sync_fetch_and_or(&x->next[0], 1);
            nxt = x->next[0];
        } while ( (x = get_unmarked_ref(nxt)) && is_marked_ref(nxt) );
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

};//skiplist


class Graph {
public:
    int source;//why is this needed?
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

//////////////////////////////////////////////////////////////////
/// DIJKSTRA ///

// void relax(skiplist* queue, int* distances, std::mutex **offersLocks, Offer **offers, Vertex* vertex, int alt) {
//     Offer* curr_offer;

//     offersLocks[vertex->index]->lock();


//     if (alt < distances[vertex->index]) {

//         curr_offer = offers[vertex->index];
//         if (curr_offer == NULL || alt < curr_offer->dist ){

//             Offer* new_offer = new Offer(vertex, alt);//TODO added by Haran 29.8

//             queue->insert(alt, vertex->index, 0);

//             offers[vertex->index] = new_offer;

//         }

//     }
//     offersLocks[vertex->index]->unlock();


// }


void relax(skiplist* queue, int* distances, std::mutex **offersLocks, Offer **offers, Vertex* vertex, int alt, int tid) {
    queue->OfferMgr->enterQuiescentState(tid);
    Offer* curr_offer;
    offersLocks[vertex->index]->lock();
    if (alt < distances[vertex->index]) {
        curr_offer = offers[vertex->index];
        if (curr_offer == nullptr || alt < curr_offer->dist ){
            // Offer* new_offer = queue->offer_mgr->template allocate<Offer>(tid); 
            Offer * new_offer = new Offer();
            new_offer->vertex = vertex;
            new_offer->dist = alt; 
            queue->insert(alt, vertex->index, tid);
            if (offers[vertex->index]) {
                // delete offers[vertex->index];
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


bool finished_work(bool done[], int size){

    for (int i = 0; i < size; i++){
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
            // assert(queue->size >= 0);
            done[tid] = true;
        }

        skiplistNode * shortest_distance_node = nullptr;
        if (!done[tid]){
            shortest_distance_node = queue->deleteMin(tid);
        }
        else{
            int k = 0;
            while (done[k] && k < THREADS_NUM)
                k++;
            if (k == THREADS_NUM)
                return NULL;
            else
                continue;
        }
        if (shortest_distance_node == nullptr) {
            done[tid] = true;
            int k = 0;
            while (done[k] && k < THREADS_NUM)
                k++;
            if (k == THREADS_NUM)
                return NULL;
            else
                continue;
        }
        
        // skiplistNode * shortest_distance_node = queue->deleteMin(tid);

        // if (shortest_distance_node == nullptr) {

        //     done[tid] = true;
        //     if (finished_work(done, p)) {

        //         //printf("tid %lu saw that all threads finished\n", tttt);//todo remove
        //         pthread_mutex_lock(&done_work_lock);
        //         pthread_cond_broadcast(&done_work_cond);
        //         pthread_mutex_unlock(&done_work_lock);

        //         return NULL;
        //     } else {
        //         //printf("tid %lu is going to sleep cause he doesn't have work to do\n", tttt);//todo remove
        //         // TODO: sleep on condition variable

        //         pthread_mutex_lock(&done_work_lock);
        //         while (!finished_work(done, p))
        //         {
        //             pthread_cond_wait(&done_work_cond, &done_work_lock); //todo NEW
        //         }
        //         pthread_mutex_unlock(&done_work_lock);

        //         if(finished_work(done, p)) {
        //             //printf("tid %lu was awakened because there was no more work so he can go home\n", tttt);//todo remove
        //             return NULL;
        //         } else {
        //             //printf("tid %lu was awakened because he needs to get back to work\n");
        //             done[tid] = false;
        //             continue;
        //         }
        //     }
        // }




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

    }//end of while
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

    }   //when we reached here - done inserting all nodes to graph

    f.close();
    G->vertices[0]->dist = 0;
    dijkstra_shortest_path(G);
    delete G;
    return 0;
}