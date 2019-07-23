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

int getRandomHeight() {
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

    skiplistNode(int key, int data, int levels, skiplistNode *val = nullptr) : isInserting(true), key(key), value(data),
                                                                               levels(levels) {
        for (int i = 0; i < levels; i++) {
            next.push_back(val);
        }
    }

    skiplistNode() {}


    bool getIsNextNodeDeleted() {
//        std::cout<< "checking if " <<this->key<< ", " <<  this->value << " next is deleted" << std::endl;
        if (this->next.size() > 0) {
//            std::cout << "apparently, this->value = " << this->value <<  "has next. size = " << this->next.size()<<std::endl;
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
    int BOUND_OFFSET = 1;//todo - wtf

    skiplist(int levels) {
        this->tail = new skiplistNode(5555555, 0, LEVELS);
        this->tail->isInserting = false;
        this->head = new skiplistNode(-5555555, 0, LEVELS, this->tail);
        this->head->isInserting = false;
    }


    void locatePreds(int k, std::vector<skiplistNode *> &preds, std::vector<skiplistNode *> &succs,
                     skiplistNode *del) {
//        std::cout << "locating preds for k = " << k <<std::endl;
        int i = this->levels - 1;
        skiplistNode * pred;
        while (i >= 0) {
            pred = this->head;
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
//        std::cout << "SUCCESSFULLY LOCATIED PREDECESSOR OF K= " << k << std::endl;
    }

    void restructure() {
        int i = this->levels - 1;
        skiplistNode * pred = this->head;
        skiplistNode * cur;
        skiplistNode * h;
        while (i > 0) {//todo - should this be i>=0???
            h = this->head->next[i];//record observed heada
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
            assert(pred->getIsNextNodeDeleted());//todo - delete
            if (__sync_bool_compare_and_swap(&this->head->next[i], h, cur)) { // if CAS fails, the same operation will be done for the same level
                i--;
            }
        }
    }

    skiplistNode *deleteMin() {
        skiplistNode *x = this->head;
        int offset = 0;
        skiplistNode *newHead = nullptr;
        skiplistNode *observedHead_marked = x->getNextNodeMarked(0); //first real head (after sentinel)
        skiplistNode * observedHead_unmarked = x->getNextNodeUnmarked(0);
        bool isNextNodeDeleted;
        skiplistNode *next;
        do {
            next = x->getNextNodeUnmarked(0);
            isNextNodeDeleted = x->getIsNextNodeDeleted();
            if (x->getNextNodeUnmarked(0) == this->tail) { //if queue is empty - return
                return nullptr;
            }
            if (x->isInserting && !newHead) {
                newHead = x; //head may not surpass pending insert, inserting node is "newhead".
            }
            skiplistNode *nxt = __sync_fetch_and_or(&x->next[0], 1); //logical deletion of successor to x.
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

        if (__sync_bool_compare_and_swap(&this->head->next[0], observedHead_marked, newHead)) {
            restructure();
            skiplistNode *cur = observedHead_unmarked;
            while (cur != newHead) {
                next = observedHead_unmarked->getNextNodeUnmarked(0);
//                markRecycle(cur); //todo !!!!!!!!!!!!!!!!!!!!! what the fuck is this
                cur = next;
            }
        }
        std::cout<<"leaving delete min" <<std::endl;
        return x;
    }

    void insert(int key, int val) {
        std::cout << "inserting val =" << val <<", key = " << key << std::endl;
        int height = getRandomHeight();
        skiplistNode *newNode = new skiplistNode(key, val, height);
        std::vector < skiplistNode * > preds(this->levels, nullptr);
        std::vector < skiplistNode * > succs(this->levels, nullptr);

        skiplistNode *del = nullptr;
        do {
            this->locatePreds(key, preds, succs, del);
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
                this->locatePreds(key, preds, succs, del);
                if (succs[0] != newNode)
                    break; //new has been deleted
            }
        }
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
    std::mutex **offerLock; //used to prevent multiple threads from concurrently changing the priority (distance) to the same node.
    Graph() {};

    ~Graph() {};//todo
};

class Offer {
public :
    Vertex *vertex;
    int dist;

    Offer(Vertex *vertex, int dist) : vertex(vertex), dist(dist) {}
};

//////////////////////////////////////////////////////////////////
/// DIJKSTRA ///

skiplist q = skiplist(LEVELS);

bool finished_work(bool * done, int size) {
    for (int i = 0; i < size; i++) {
        if (!done[i]) {
            return false;
        }
    }
    return true;
}


void relax(skiplist *queue, int * distances, std::mutex **offersLocks, std::vector<Offer *> &offers,
           Vertex *vertex, int alt) {
    Offer *curr_offer;

    //// critical section /////
    offersLocks[vertex->index]->lock();
    if (alt < distances[vertex->index]) {
        curr_offer = offers[vertex->index];
        if (curr_offer == NULL || alt < curr_offer->dist) {//todo- look into changing it to "alt<vertex->dist"
            Offer *new_offer = new Offer(vertex, alt);
            queue->insert(alt, vertex->index);//todo- check this
            offers[vertex->index] = new_offer;
        }
    }
    offersLocks[vertex->index]->unlock();
    /////end of critical section/////
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
    std::vector<Offer *> offers;

    threadInput(bool *done, skiplist *queue, Graph *G, int * distances, std::mutex **offersLocks,
                std::mutex **distancesLocks, std::vector<Offer *> offers, int tid) {
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

    while (!done[tid]) {
        skiplistNode *min_offer_int = queue->deleteMin();
        if (min_offer_int == nullptr) {
            std::cout << "min offer is nullpter " <<std::endl;
            done[tid] = true; // 1 is done. change cp to num of thread.
            int k = 0;
            while (done[k] && k < 1)
                k++;
            if (k == 1)
                return NULL;
            else
                done[tid] = false;
                continue;
        }
        std::cout << tid <<" deleted node = "<< min_offer_int->key <<std::endl;

        curr_v = G->vertices[min_offer_int->value];
        curr_dist = min_offer_int->key;//todo - check this

        ////critical section - updating distance vector//////
        input->distancesLocks[min_offer_int->value]->lock();
        if (curr_dist < input->distances[min_offer_int->value]) {
            input->distances[min_offer_int->value] = curr_dist; //todo remove field dist from vertex
            explore = true;

        } else {
            explore = false;
        }
        input->distancesLocks[min_offer_int->value]->unlock();
        ////end of critical section/////
        if (explore) {
            for (int i = 0; i < (curr_v->neighbors.size()); i++) {
                neighbor = curr_v->neighbors[i].first;
                weight = curr_v->neighbors[i].second;
                alt = curr_dist + weight;
                relax(queue, input->distances, input->offersLocks, input->offers, neighbor, alt);
            }
        }
    }//end of while
    return NULL;
}



void dijkstra_shortest_path(Graph *G) {
    int num_of_theads = 1; //todo - optimize this
    skiplist *queue = new skiplist(LEVELS); //global
    Offer *min_offer;
    Vertex *curr_v;
    int * distances = (int * )malloc(sizeof(int)*G->vertices.size());
    std::vector < Offer * > offers(G->vertices.size(), nullptr);
    bool * done = (bool * )malloc(sizeof(bool)*num_of_theads);
    std::mutex **offersLocks = new std::mutex *[G->vertices.size()];
    std::mutex **distancesLocks = new std::mutex *[G->vertices.size()];


    //init locks
    for (int i = 0; i < G->vertices.size(); i++) {
        offersLocks[i] = new std::mutex();
        distancesLocks[i] = new std::mutex();
        distances[i] = 1000000000;
    }

    queue->insert(0, 0);//insert first element to queue. First element: key (dist) = 0, value(index) = 0 (this is the soure)
    pthread_t threads[num_of_theads];
    for (int i = 0; i < num_of_theads; i++) {
        done[i] = false; //set thread[i] to 'not-done'
        threadInput *input = new threadInput(done, queue, G, distances, offersLocks, distancesLocks, offers, i);
        pthread_create(&threads[i], NULL, &parallelDijkstra, (void *) input);
    }

    for (long i = 0; i < num_of_theads; i++)
        (void) pthread_join(threads[i], NULL);

    ofstream myfile;
    myfile.open ("output.txt");
    for (int i = 0; i < G->vertices.size(); i++) {
        myfile << "i = " << i <<", distance = " <<distances[i] << std::endl;
    }
    myfile.close();

}


int main(int argc, char *argv[]) {
//    string pathToFile = argv[1];
    std::string pathToFile = "./input_1.txt";
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
        v2->index = v2_index;
        //Weight
        v1->neighbors.push_back(make_pair(v2, weight));
        v2->neighbors.push_back(make_pair(v1, weight));


    }   //when we reached here - done inserting all nodes to graph
    G->vertices[0]->dist = 0;
    dijkstra_shortest_path(G);
    return 0;
}