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

    skiplistNode(int key, int data,  int levels, skiplistNode * val = nullptr) : isInserting(true), key(key), value(data), levels(levels) {
        for (int i = 0; i < levels; i ++) {
            next.push_back(val);
        }
    }

    skiplistNode(){}


    bool getIsNextNodeDeleted() {
        uintptr_t address = (uintptr_t) this->next[0];
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
    int BOUND_OFFSET = 400;//todo - wtf

    skiplist(int levels) {
        this->tail = new skiplistNode(5555555, 0, LEVELS);
        this->tail->isInserting = false;
        this->head = new skiplistNode(-5555555, 0, LEVELS, this->tail);
        this->head->isInserting = false;
    }

    void locatePreds(int k, skiplistNode * preds[], skiplistNode * succs[],
                     skiplistNode * del) {
        std::cout << "LOCATING PREDECESSOR OF K= " << k << std::endl;
        int i = this->levels - 1;
        skiplistNode * pred = this->head;
        while (i >= 0) {
//            std::stdcout << "in while. i = " << i << std::endl;
            skiplistNode * cur = pred->next[i];
            int isCurDeleted = pred->getIsNextNodeDeleted();
//            std::cout << "after isCurDeleted. isCurDeleted? " <<isCurDeleted <<std::endl;
            bool isNextDeleted = cur->getIsNextNodeDeleted() ;
//            std::cout << "is next node deleted? "<< isNextDeleted <<std::endl;
            while (cur->key < k || isNextDeleted || ((i == 0) && isCurDeleted)) {
//                std::cout << "******beginning of while "<<std::endl;
                if ((isCurDeleted) && i == 0) //if there is a level 0 node deleted without a "delete flag" on:
                    del = cur;
                pred = cur;
                cur = pred->next[i];
                isCurDeleted = pred->getIsNextNodeDeleted();
//                std::cout << "before getIsNextNodeDeleted number two" << std::endl;
//                this->printSkipList();
                isNextDeleted = cur ->getIsNextNodeDeleted();
//                std::cout << "end of while" << std::endl;

            }
//            std::cout <<  "after while " << std::endl;
            preds[i] = pred;
            succs[i] = cur;
            i--;
        }
    }

    void restructure() {
        std::cout<<"in restructure" <<std::endl;
        int i = this->levels - 1;
        skiplistNode * pred = this->head;
        while (i > 0) {
            skiplistNode * h = this->head->next[i];//record observed heada
            bool is_h_next_deleted = h->getIsNextNodeDeleted();
            skiplistNode * cur = pred->next[i]; //take one step forwarad
            bool is_cur_next_deleted = cur->getIsNextNodeDeleted();
            if (!is_h_next_deleted) {
                i--;
                continue;
            }
            //traverse level until non-marked node is found
            while (is_cur_next_deleted) {
                pred = cur;
                cur = pred->next[i];
                is_cur_next_deleted = pred->getIsNextNodeDeleted();
            }
            std::cout << "before assertion in restructure" << std::endl;
            assert (pred->getIsNextNodeDeleted());//todo - delete
            if (__sync_bool_compare_and_swap(&this->head->next[i], h,
                                             pred->next[i])) { // if CAS fails, the same operation will be done for the same level
                i--;
            }
        }
    }

    skiplistNode * deleteMin() {
        skiplistNode * x = this->head;
        int offset = 0;
        skiplistNode * newHead = nullptr;
        skiplistNode * observedHead = x->next[0]; //first real head (after sentinel)
        bool isNextNodeDeleted;
        skiplistNode * next;
//        std::cout << "in deleteMin(), before do" << std::endl;
        do {
            next = x->getNextNodeUnmarked(0);
            isNextNodeDeleted = x->getIsNextNodeDeleted();
            if (x->getNextNodeUnmarked(0) == this->tail) { //if queue is empty - return
//                std::cout << "I AM SUPPOSED to BE HEre!!!!!" << std::endl;
                return nullptr;
            }
            if (x->isInserting && !newHead) {
                newHead = x; //head may not surpass pending insert, inserting node is "newhead".
            }
            skiplistNode * nxt = __sync_fetch_and_or(&x->next[0], 1); //logical deletion of successor to x.
            assert(x->getIsNextNodeDeleted());
            offset++;
            x = next;
        } while (isNextNodeDeleted);

        int v = x->value;

        if (offset < this->BOUND_OFFSET) {//if the offset is big enough - try to perform memory reclamation
            return x;
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
        return x;
    }

    void insert(int key, int val) {
        std::cout << "inserting " <<key << std::endl;
        int height = getRandomHeight() ;
        skiplistNode * newNode = new skiplistNode(key, val, height);
        skiplistNode * preds[this->levels];
        skiplistNode * succs[this->levels];
        skiplistNode * del = nullptr;
        do {
            this->locatePreds(key, preds, succs, del);
//            std::cout << "pushing back" <<std::endl;
//            (newNode->next).push_back(succs[0]);
            newNode->next[0] = succs[0];
        } while (!__sync_bool_compare_and_swap(&preds[0]->next[0], succs[0], newNode));
        std::cout << "newNode key = " << newNode->key <<", newNOde.next.size() = " << newNode->next.size() << std::endl;
        int i = 1;
        while (i < height) { //insert node at higher levels (bottoms up)
            newNode->next[i] = succs[i];

            bool test1 = newNode->getIsNextNodeDeleted();//was the lowest level successor has been deleted? (implied that newNode is deleted as well)
            bool test2 = succs[i]->getIsNextNodeDeleted();// is the candidate successor deleted? (could mean skewed getPreds)
            bool test3 = succs[i] == del;//is the candidate successor deleted? (could mean skewed getPreds)

            if (newNode->getIsNextNodeDeleted() || succs[i]->getIsNextNodeDeleted()|| succs[i] == del) {
                break; //new is already deleted
            }
            if (__sync_bool_compare_and_swap(&(preds[i]->next[i]), succs[i], newNode)) {
                i++; // if successful - ascend to next level
            } else {
                this->locatePreds(key, preds, succs, del);
                if (succs[0] != newNode)
                    break; //new has been deleted
            }
        }
//        std::cout << "done inserting " << std::endl;
        newNode->isInserting = false; //allow batch deletion past this node
    }

    void printSkipList(){
        std::cout << "--------------------------------------------------" <<std::endl;
        std::cout << "printing skiplist" << std::endl;
        skiplistNode * node = this->head;
        for (int i = 0; i < this->levels-1; i ++){
            std::cout << "level " << i << " , head ";
            skiplistNode * nextNode = node->getNextNodeUnmarked(i);
            bool isNextDeleted = node->getIsNextNodeDeleted();
            bool to_continue = true;
            while (to_continue){
                if (isNextDeleted && i ==0)
                    std::cout<<"~";
                std::cout <<"("<<nextNode->value<<", "<< nextNode->key << ") ";
                isNextDeleted = nextNode->getIsNextNodeDeleted();
                nextNode = nextNode->getNextNodeUnmarked(i);
                if (!nextNode) {
                    to_continue = false;
                }
                else
                    to_continue = true;
            }
            std::cout<< std::endl;
        }
        std::cout << "--------------------------------------------------" <<std::endl;
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
    int source;//why is this needed?
    vector <Vertex*> vertices;
    std::mutex** offerLock; //used to prevent multiple threads from concurrently changing the priority (distance) to the same node.
    Graph(){};
    ~Graph(){};//todo
};

class Offer {
public :
    Vertex* vertex;
    int dist;
    Offer(Vertex * vertex, int dist) : vertex(vertex), dist(dist){}
};

//////////////////////////////////////////////////////////////////
/// DIJKSTRA ///

skiplist q = skiplist(5);

bool finished_work(bool * done, int size){
//    std::cout << "in finished work. size =  "<<size <<std::endl;
    for (int i = 0; i < size; i++)
    {
        std::cout << "thread " << i << "is done>? " << done[i] << std::endl;
        if(!done[i])
            return false;
    }
    return true;
}

void relax(skiplist* queue, int* distances, std::mutex **offersLocks, Offer **offers, Vertex * vertex, int alt) {
    std::cout << "in relax!!!" << std::endl;
//    std::cout << "in relax, vertext->dist = " << distances[vertex->index] <<", alt = " << alt <<std::endl;
    Offer* curr_offer;
    ////critical section/////
    offersLocks[vertex->index]->lock();
//    std::cout << "alt = " << alt << ", distances[vertex->index] = " <<  distances[vertex->index] << std::endl;
    if (alt < distances[vertex->index]) {
        curr_offer = offers[vertex->index];
        if (curr_offer == NULL)
            std::cout << "curr_oofer is null" << std::endl;
        if (curr_offer == NULL || alt < curr_offer->dist ){//todo- look into changing it to "alt<vertex->dist"
            Offer* new_offer = new Offer(vertex, alt);
            std::cout << "after creating new offer" << std::endl;
            queue->insert(alt, vertex->index);//todo- check this
            std::cout << "after insert"<< std::endl;
            offers[vertex->index] = new_offer;
            std::cout << "after adding element to offers" << std::endl;
            //todo- check this: changing the value of the vertex itself??
        }
    }
    offersLocks[vertex->index]->unlock();
    std::cout << "leaving relax()" << std::endl;
    /////end of critical section/////

    //todo - delete previous offer??

}

class threadInput{
public:

    bool* done;
    skiplist *queue;
    Graph *G;
    int* distances;
    std::mutex **offersLocks;
    std::mutex **distancesLocks;
    Offer **offers;
    int tid;
    threadInput(bool * done, skiplist * queue, Graph * G, int * distances, std::mutex ** offersLocks, std::mutex ** distancesLocks, Offer ** offers, int tid){
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

void * parallelDijkstra(void *void_input) {
    threadInput * input = (threadInput * ) void_input;
    bool * done = input->done;
    skiplist * queue = input->queue;
    Graph * G = input->G;
    int * distances = input->distances;
    std::mutex ** offersLocks = input->offersLocks;
    std::mutex ** distancesLocks = input->distancesLocks;
    Offer ** offers = input->offers;
    int tid = input->tid;
    Vertex *curr_v;
    Vertex *neighbor;
    bool explore = true;
    int curr_dist = -1;
    int alt;
    int weight;

    std::cout << "in parallelDijkstra. Thread # " << tid << std::endl;
    while (!done[tid]) {
        distancesLocks
        std::cout << "in while!" << std::endl;
        skiplistNode * min_offer_int = queue->deleteMin();
//        std::cout <<"!!!!!!!!!!!!!! deleted node = "<< min_offer_int->key <<std::endl;
//        std::cout << "found curr_v (index) = " << curr_v->index <<", curr_v(dist) = " << curr_v->dist << "printing neighbors"<< std::endl;
//        std::cout << "found curr_v (index) = " << curr_v->index << ". printing neighbors"<< std::endl;

        if (min_offer_int == nullptr) {//todo- what does this do????
//            std::cout << "min offer is nullpter, done["<<tid<<"] = true" <<std::endl;
            done[tid] = true; // 1 is done. change cp to num of thread.
            if (!finished_work(done, 2)) {//todo - second input - num of threads
                done[tid] = false;
                continue;
            } else { // all threads finished work terminate process
                for (int i = 0; i < G->vertices.size(); i++) {
                    std::cout << distances[i] << "\n";
                }
//                std::cout << "ALL DONE!!!!!!!!!!!!" << std::endl;
                return NULL;
            }
        }
        curr_v = G->vertices[min_offer_int->value];
        curr_dist = min_offer_int->key;//todo - check this
//        std::cout << "found curr_v (index) = " << curr_v->index << ". printing neighbors"<< std::endl;
//
//        for (int i = 0; i < curr_v->neighbors.size(); i++){
//            std::cout << curr_v->neighbors[i].first->index << std::endl;
//        }
        std::cout << "before critical section" << std::endl;
        ////critical section - updating distance vector//////
        distancesLocks[min_offer_int->value]->lock();
        std::cout << "thread # " << tid << " in critical section. curr dist = " << curr_dist << ", other = " <<distances[min_offer_int->value] << std::endl ;

        if (curr_dist < distances[min_offer_int->value]) {
            std::cout << "in IF!!!! " << curr_dist << " < " << distances[min_offer_int->value] <<std::endl;
            distances[min_offer_int->value] = curr_dist; //todo remove field dist from vertex
            explore = true;
        } else {
            explore = false;
        }
        distancesLocks[min_offer_int->value]->unlock();
        ////end of critical section/////
        std::cout << "left critical section" << std::endl;
        std::cout << explore << std::endl;
        if (explore) {
            for (int i = 0; i < (curr_v->neighbors.size()); i++) {
                std::cout << "in explore for. i = " <<i << std::endl;
                neighbor = curr_v->neighbors[i].first;
//                std::cout << "**** neighbor->dist = " << neighbor->dist << ", index= "<< neighbor->index << std::endl;
                weight = curr_v->neighbors[i].second;
                std::cout << "neighbor = " << neighbor->index << ", weight = " << weight << std::endl;
                alt = curr_dist + weight;
                std::cout << "curr_dist(" << curr_dist << ") + wieght(" << weight << ") = alt(" << alt << ")" << std::endl;
                relax(queue, distances, offersLocks, offers, neighbor, alt);
            }
            std::cout << "done exploring node. skiplist = " << std::endl;
//            queue->printSkipList();
        }
        std::cout << "end of while" << std::endl;
    }//end of while
    std::cout << "leaving parallel dijkstra" << std::endl;
    return NULL;
}//end of function

void dijkstra_shortest_path(Graph *G) {
    int num_of_theads = 2; //todo - optimize this
    skiplist * queue = new skiplist(5); //global
    Offer *min_offer;
    Vertex *curr_v;
    int distances[G->vertices.size()]; //global
    Offer *offers[G->vertices.size()];//global - why do we need both lists????
    bool done[num_of_theads]; //how many threads?????
    std::mutex **offersLocks = new std::mutex *[G->vertices.size()];
    std::mutex **distancesLocks = new std::mutex *[G->vertices.size()];

    for (int i=0; i<num_of_theads; i++){ //todo check
        done[i] = false;
    }
    //init distances and offers (why are there 2?)
    for (int i = 0; i < G->vertices.size(); i++) {
        distances[i] = 100000000; //todo - max int
        offers[i] =NULL;
    }

    //init locks
    for (int i = 0; i < G->vertices.size(); i++) {
        offersLocks[i] = new std::mutex();
        distancesLocks[i] = new std::mutex();
    }

    queue->insert(0, 0);//insert first element to queue. First element: key (dist) = 0, value(index) = 0 (this is the soure)

    pthread_t threads[num_of_theads];
    for(int i=0; i < num_of_theads; i++) {
        threadInput * input = new threadInput(done, queue, G, distances, offersLocks, distancesLocks, offers, i);
        std::cout<<"before creating thread # " << i << std::endl;
        pthread_create(&threads[i], NULL, &parallelDijkstra, (void *)input);
    }

    for (long i = 0; i < num_of_theads; i ++)
        (void)pthread_join (threads[i], NULL);

    std::cout << "threads joined?" << std::endl;

}

//////////////////////////////////////////////////////////////////

int main(int argc,  char *argv[]) {
//    string pathToFile = argv[1];
    std::string pathToFile = "./input.txt";
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
    Vertex * source_vertex = new Vertex();
    source_vertex->dist=0;
    source_vertex->index=0;
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
//        v1->dist = 1000000 ;//(maxInt)
//        v2->dist = 1000000 ; // (maxInt)
        v1->neighbors.push_back(make_pair(v2, weight));
        v2->neighbors.push_back(make_pair(v1, weight));

//        std::cout << "created nodes: v1_index = " << v1_index <<", " << v2_index << ", weight: "  << weight << std::endl;

    }   //when we reached here - done inserting all nodes to graph
    G->vertices[0]->dist=0;
    dijkstra_shortest_path(G);
    return 0;
}