#include <iostream>
#include <atomic>
#include <assert.h>
#include <thread>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <mutex>
#include<bits/stdc++.h>



using namespace std;

#define LEVELS 5 //log n (10^7)

int getRandomHeight(){
    //check if the ith bit is 1.
    return (rand() % LEVELS) + 1;
}


class skiplistNode {
    /*array of pointers , entry i points to "next" skiplistNode on level i
      the last bit in next[i] indicates whether or not next[i] is deleted or not */
public:
    bool isInserting;
    int key;
    int value;
    int levels;
    std::vector<skiplistNode *> next;

    skiplistNode(int key, int data,  int levels, skiplistNode * val = nullptr) : isInserting(true), key(key), value(data), levels(levels), next(levels, val) {}

    skiplistNode(){}


    bool getIsNextNodeDeleted(int level) {
//        std::cout << "in getIsNextNodeDeleted" << std::endl;
        uintptr_t address = (uintptr_t) this->next[level];

//        std::cout << "after this>next[level]" << std::endl;

        uintptr_t isNextDeleted = address & 1;
        if (isNextDeleted)
            return true;
        else
            return false;
    }

    /* sets the last bit of the address to 1*/
    void setIsNextNodeDeleted(int level) {
//        this->next[level] = ((void *)(((uintptr_t)(this->next[level])) | 1));
        this->next[level] = ((skiplistNode *)(((uintptr_t)(this->next[level])) | 1));
    }

    /* returns next node when the last bit is always 0 (real address) */
    skiplistNode * getNextNodeMarked(int level) {
        return this->next[level];
    }

    /* returns next node when the last bit is either 0 or 1, depending whether
     * or not the node is marked as deleted */
    skiplistNode * getNextNodeUnmarked(int level){
        return (skiplistNode *)(((uintptr_t)(this->next[level])) & ~1);
    };
};


class skiplist {
public:
    skiplistNode * head;
    skiplistNode * tail;
    int levels = LEVELS;
    int BOUND_OFFSET = 4;//todo - wtf

    skiplist(int levels) {
        this->tail = new skiplistNode(10000, 0, LEVELS);
        this->tail->isInserting = false;
        this->head = new skiplistNode(-10000, 0, LEVELS, this->tail);
        this->head->isInserting = false;
    }


    void locatePreds(int k, skiplistNode * preds[], skiplistNode * succs[],
                     skiplistNode * del) {
//        std::cout << "in locate_preds" << std::endl;
        int i = this->levels - 1;
        skiplistNode * pred = this->head;
        while (i >= 0) {
//            std::cout << "in while. i = " << i << std::endl;
            skiplistNode * cur = pred->next[i];
//            std::cout << "11111" << std::endl;
            int isCurDeleted = pred->getIsNextNodeDeleted(i);
//            std::cout << "after isNextNodeDeleted" << std::endl;

            bool isNextDeleted = cur->getIsNextNodeDeleted(0) ;
            while (cur->key < k || isNextDeleted || ((i == 0) && isCurDeleted)) {
//                std::cout << "beginning of while "<<std::endl;
                if ((isCurDeleted) && i == 0) //if there is a level 0 node deleted without a "delete flag" on:
                    del = cur;
                pred = cur;
                cur = pred->next[i];
                isCurDeleted = pred->getIsNextNodeDeleted(0);
//                std::cout << "before getIsNextNodeDeleted number two" << std::endl;
//                this->printSkipList();
                isNextDeleted = cur ->getIsNextNodeDeleted(0);
//                std::cout << "end of while" << std::endl;

            }
//            std::cout <<  "after while " << std::endl;
            preds[i] = pred;
            succs[i] = cur;
            i--;
        }
    }

    void restructure() {
        int i = this->levels - 1;
        skiplistNode * pred = this->head;
        while (i > 0) {
            skiplistNode * h = this->head->next[i];//record observed heada
            bool is_h_next_deleted = h->getIsNextNodeDeleted(0);
            skiplistNode * cur = pred->next[i]; //take one step forwarad
            bool is_cur_next_deleted = cur->getIsNextNodeDeleted(0);
            if (!is_h_next_deleted) {
                i--;
                continue;
            }
            //traverse level until non-marked node is found
            while (is_cur_next_deleted) {
                pred = cur;
                cur = pred->next[i];
                is_cur_next_deleted = pred->getIsNextNodeDeleted(0);
            }
            assert (pred->getIsNextNodeDeleted(0));//todo - delete
            if (__sync_bool_compare_and_swap(&this->head->next[i], h,
                                             pred->next[i])) { // if CAS fails, the same operation will be done for the same level
                i--;
            }
        }
    }

    int deleteMin() {
        skiplistNode * x = this->head;
        int offset = 0;
        skiplistNode * newHead = nullptr;
        skiplistNode * observedHead = x->next[0]; //first real head (after sentinel)
        bool isNextNodeDeleted;
        skiplistNode * next;
        do {
            next = x->getNextNodeUnmarked(0);
            isNextNodeDeleted = x->getIsNextNodeDeleted(0);
            if (x->getNextNodeUnmarked(0) == this->tail) { //if queue is empty - return
                return -1000;
            }
            if (x->isInserting && !newHead) {
                newHead = x; //head may not surpass pending insert, inserting node is "newhead".
            }
            skiplistNode * nxt = __sync_fetch_and_or(&x->next[0], 1); //logical deletion of successor to x.
            offset++;
            x = next;
        } while (isNextNodeDeleted);

        int v = x->value;

        if (offset < this->BOUND_OFFSET) {//if the offset is big enough - try to perform memory reclamation
            return v;
        }
        if (newHead == 0) {
            newHead = x;
        }

        if (__sync_bool_compare_and_swap(&this->head->next[0], observedHead, newHead)){
            restructure();
            skiplistNode *cur = observedHead;
            while (cur != newHead) {
                next = cur->next[0];
//                markRecycle(cur); //todo !!!!!!!!!!!!!!!!!!!!! what the fuck is this
                cur = next;
            }
        }
        return v;
    }

    void insert(int key, int val) {
//        std::cout << "inserting" << std::endl;
        int height = getRandomHeight() ;
        skiplistNode * newNode = new skiplistNode(key, val, height);
        skiplistNode * preds[this->levels];
        skiplistNode * succs[this->levels];
        skiplistNode * del = nullptr;
        do {
            this->locatePreds(key, preds, succs, del);
//            std::cout << "after locate preds" << std::endl;
            newNode->next[0] = succs[0];
        } while (!__sync_bool_compare_and_swap(&preds[0]->next[0], succs[0], newNode));

//        std::cout << "after first while" << std::endl;

        int i = 1;
        while (i < height) { //insert node at higher levels (bottoms up)
            newNode->next[i] = succs[i];

            bool test1 = newNode->getIsNextNodeDeleted(i);//was the lowest level successor has been deleted? (implied that newNode is deleted as well)
            bool test2 = succs[i]->getIsNextNodeDeleted(i);// is the candidate successor deleted? (could mean skewed getPreds)
            bool test3 = succs[i] == del;//is the candidate successor deleted? (could mean skewed getPreds)

            if (newNode->getIsNextNodeDeleted(0) || succs[i]->getIsNextNodeDeleted(i)|| succs[i] == del) {
                break; //new is already deleted
            }
            if (__sync_bool_compare_and_swap(&(preds[i]->next[i]), succs[i], newNode)) {
                i++; // if successful - ascend to next level
            } else {
//                std::cout << "in else" << std::endl;
                this->locatePreds(key, preds, succs, del);
                if (succs[0] != newNode)
                    break; //new has been deleted
            }
        }
//        std::cout << "done inserting " << std::endl;
        newNode->isInserting = false; //allow batch deletion past this node
    }


    void printSkipList(){
        skiplistNode * node = head;
        for (int i = 0; i < this->levels-1; i ++){
            std::cout << "level " << i << " , head ";
            skiplistNode * next = node->next[i];
            bool isDone = true;
            while (isDone){
                std::cout << next->key << " " ;
                next = next->next[i];
                isDone = next->next[0];
            }
            std::cout << std::endl;
        }
    }



};//skiplist


struct Vertex {
    unsigned int index;
    vector < pair <Vertex*, int> > neighbors; // Vertex + weight of edge
    unsigned int dist;
    pair < int, int > location; // location = queue index, heap index

};


class Graph {
public:
    int source;
    vector <Vertex*> vertices;
    std::mutex** offerLock; //used to prevent multiple threads from concurrently changing the priority (distance) to the same node.
    Graph(){std::cout << "hi linker" << std::endl; };
    ~Graph(){};//todo
};

struct Offer {
    Vertex* vertex;
    int dist;
};

//////////////////////////////////////////////////////////////////
/// DIJKSTRA ///


skiplist q = skiplist(5);

bool finished_work(bool done[], int size){
    for (int i = 0; i < size; i++)
    {
        if(!done[i])
        {
            return false;
        }
    }
    return true;
}

void relax(skiplist* queue, int* distances, std::mutex **offersLocks, Offer **offers, Vertex * vertex, int alt) {
    Offer* curr_offer;

    offersLocks[vertex->index]->lock();


    if (alt < distances[vertex->index]) {
        curr_offer = offers[vertex->index];
        if (curr_offer == NULL || alt < curr_offer->dist ){
            Offer* new_offer = new Offer{vertex, alt};
            queue->insert(new_offer->vertex->dist, new_offer->vertex->index);//todo- check this
//            queue->insert(new_offer);
            offers[vertex->index] = new_offer;
        }
    }
    offersLocks[vertex->index]->unlock();
}

void * add_thread(void *id)
{
    std::cout << "in add_thread" << std::endl;
    long base = 30 * (long)id;
    for(int i = 0; i < 30; i++) {
        std::cout<< "adding " << i << std::endl;
        q.insert(base + i + 1, base + i + 1);
        std::cout << "done adding " << i << std::endl;
    }
    return NULL;

}
void bla(void* i){

}

void dijkstra_shortest_path(Graph *G, int c, int p) {
    int cp=c*p;
    skiplist * queue = new skiplist(5);
    Offer *min_offer;
    Vertex *curr_v;


    int distances[G->vertices.size()];
    Offer *offers[G->vertices.size()];
    bool done[p]; //
    std::mutex **offersLocks = new std::mutex *[G->vertices.size()];
    std::mutex **distancesLocks = new std::mutex *[G->vertices.size()];

    for (int i=0; i<cp; i++){ //todo check
        done[i] = false;
    }
    //init
    for (int i = 0; i < G->vertices.size(); i++) {
        distances[i] = INT_MAX;
        offers[i] =NULL;
    }

    //init locks
    for (int i = 0; i < G->vertices.size(); i++) {
        offersLocks[i] = new std::mutex();
        distancesLocks[i] = new std::mutex();
    }

    // initialization
    distances[G->source] = INT_MAX; //todo check

    // todo remove later
    for (int i = 1; i < (G->vertices.size()); i++) { //todo why 1 not 0?
        curr_v = G->vertices[i];
        curr_v->dist = INT_MAX;
//        if (curr_v->index != G->source) {
//            distances[curr_v->index] = INT_MAX; //change distances to v->dist for all nodes in G. We do need distances since we remove nodes from the queue and we need to save the minimal distance. we also need to sync between the queue and distances[].
//            curr_v->dist = INT_MAX;
//        }
    }


    min_offer = new Offer{G->vertices[G->source], 0};
    queue->insert(min_offer->vertex->dist, min_offer->vertex->index);

    // <bool[], Multiqueue*, int, Graph*,int*, std::mutex**, std::mutex**, Offer**, int>
    for(int i=0; i< p; i++) {
        pthread_t thread;
        int NUM_THREADS=2;
        pthread_t threads[NUM_THREADS];
        int rc = pthread_create(&threads[i], NULL, add_thread, (void *) i);

//        int rc = pthread_create(&threads[i], NULL, bla, (void *)i);
//        pthread_create(&thread, NULL, bla, (void*)i);
//        pthread_create(&thread, NULL, parallel_Dijkstra, done, queue, p, G, distances, offersLocks, distancesLocks, offers, i);
    }


}


//template<bool [], class Multiqueue, int, class Graph, int*, void*, void*, class Offer, int>
void parallel_Dijkstra(bool* done, skiplist *queue, int p, Graph *G, int* distances,
                       std::mutex **offersLocks, std::mutex **distancesLocks,  Offer **offers, int tid) {
    Offer *min_offer;
    Vertex *curr_v;
    Vertex *neighbor;
    bool explore = true;
    int curr_dist;
    int alt;
    int weight;

    while (!done[tid]) {
        int min_offer_int = queue->deleteMin();
        if (!min_offer_int) {
            done[tid] = true; // 1 is done. change cp to num of thread.
            if (!finished_work(done, p)) {
                done[tid] = false;
                continue;
            } else { // all threads finished work terminate process
                for (int i = 0; i < G->vertices.size(); i++) {
                    std::cout << distances[i] << "\n";
                }
                return;
            }
        }

        curr_v = min_offer->vertex;
        curr_dist = min_offer->dist;

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
                relax(queue, distances, offersLocks, offers, neighbor, alt);
            }
        }


    }
}

//////////////////////////////////////////////////////////////////






std::vector<pthread_t> ts(8);



void test_parallel_add()
{
    std::cout << "test parallel add, " << 8 << " threads"<<std::endl;
    for (long i = 0; i < 8; i ++) {
        std::cout << "before creatae thread " << i << std::endl;
        pthread_create(&ts[i], NULL, add_thread, (void *) i);
        std::cout << "created thread " << i << std::endl;
    }
    for (long i = 0; i < 8; i ++)
        (void)pthread_join (ts[i], NULL);

    unsigned long new_, old = 0;
    int PER_THREAD = 30;
    for (long i = 0; i < 8 * PER_THREAD; i++) {
        new_ = (long)q.deleteMin();
        assert (old < new_);
        old = new_;
    }
    std::cout<<"OK"<<std::endl;

}


//void test_parallel_del()
//{
//
//    std::cout << "test parallel del, " << 8 << " threads"<<std::endl;
//    int PER_THREAD = 30;
//    for (long i = 0; i < 8 * PER_THREAD; i++)
//        insert(pq, i+1, i+1);
//
//    for (long i = 0; i < nthreads; i ++)
//        pthread_create (&ts[i], NULL, removemin_thread, (void *)i);
//
//    for (long i = 0; i < nthreads; i ++)
//        (void)pthread_join (ts[i], NULL);
//
//    std::cout<<"OK"<<std::endl;
//}












int main(int argc,  char *argv[]) {
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

    //G->vertices.push_back(source);
    G->vertices.resize(num_vertices);
    for (int i = 0; i < num_vertices; i++) {
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
    }
    dijkstra_shortest_path(G, 3, 1);
    int a=1;
    test_parallel_add();

    return 0;

}