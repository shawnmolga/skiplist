#include <iostream>
#include <atomic>
#include <assert.h>
#include <thread>
#include <cstring>
#include <vector>

using namespace std;

#define LEVELS 5

int getRandomHeight(){
    return (rand() % LEVELS) + 1;
}


template<typename _Key, typename _Data>
class skiplistNode {
    /*array of pointers , entry i points to "next" skiplistNode on level i
      the last bit in next[i] indicates whether or not next[i] is deleted or not */
public:
    bool isInserting;
    _Key key;
    _Data value;
    int levels;
    std::vector<skiplistNode<_Key, _Data> *> next;

    skiplistNode(_Key key, _Data data,  int levels, skiplistNode<_Key, _Data> * val = nullptr) : isInserting(true), key(key), value(data), levels(levels), next(levels, val) {}

    skiplistNode(){}


    bool getIsNextNodeDeleted(int level) {
        uintptr_t address = (uintptr_t) this->next[level];
        uintptr_t isNextDeleted = address & 1;
        if (isNextDeleted)
            return true;
        else
            return false;
    }

    /* sets the last bit of the address to 1*/
    void setIsNextNodeDeleted(int level) {
        this->next[level] = ((void *)(((uintptr_t)(this->next[level])) | 1));
    }

    /* returns next node when the last bit is always 0 (real address) */
    skiplistNode<_Key, _Data> * getNextNodeMarked(int level) {
        return this->next[level];
    }

    /* returns next node when the last bit is either 0 or 1, depending whether
     * or not the node is marked as deleted */
    skiplistNode<_Key, _Data> * getNextNodeUnmarked(int level){
        return (skiplistNode<_Key, _Data> *)(((uintptr_t)(this->next[level])) & ~1);
    };
};


template<typename _Key, typename _Data>
class skiplist {
public:
    skiplistNode<_Key, _Data> *head;
    skiplistNode<_Key, _Data> *tail;
    int levels = LEVELS;
    int BOUND_OFFSET = 4;//todo - wtf

    skiplist(int levels) {
        this->tail = new skiplistNode<_Key, _Data>(10, 0, LEVELS);
        this->tail->isInserting = false;
        this->head = new skiplistNode<_Key, _Data>(-10, 0, LEVELS, this->tail);
        this->head->isInserting = false;
    }


    void locatePreds(_Key k, skiplistNode<_Key, _Data> *preds[], skiplistNode<_Key, _Data> *succs[],
                     skiplistNode<_Key, _Data> * del) {
        int i = this->levels - 1;
        skiplistNode<_Key, _Data> *pred = this->head;
        while (i >= 0) {
            skiplistNode<_Key, _Data> *cur = pred->next[i];
            int isCurDeleted = pred->getIsNextNodeDeleted(i);
            while (cur->key < k || cur->getIsNextNodeDeleted(1) || ((i == 0) && isCurDeleted)) {
                if ((isCurDeleted) && i == 0) //if there is a level 0 node deleted without a "delete flag" on:
                    del = cur;
                pred = cur;
                cur = pred->next[i];
                isCurDeleted = pred->getIsNextNodeDeleted(i);
            }
            preds[i] = pred;
            succs[i] = cur;
            i--;
        }
    }

    void restructure() {
        int i = this->levels - 1;
        skiplistNode<_Key, _Data> *pred = this->head;
        while (i > 0) {
            skiplistNode<_Key, _Data> *h = this->head->next[i];//record observed heada
            bool is_h_next_deleted = h->getIsNextNodeDeleted(0);
            skiplistNode<_Key, _Data> *cur = pred->next[i]; //take one step forwarad
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

    _Data deleteMin() {
        skiplistNode<_Key, _Data> *x = this->head;
        int offset = 0;
        skiplistNode<_Key, _Data> *newHead = nullptr;
        skiplistNode<_Key, _Data> *observedHead = x->next[0]; //first real head (after sentinel)
        bool isNextNodeDeleted;
        skiplistNode<_Key, _Data> *next;
        do {
            next = x->getNextNodeUnmarked(0);
            isNextNodeDeleted = x->getIsNextNodeDeleted(0);
            if (x->getNextNodeUnmarked(0) == this->tail) { //if queue is empty - return
                return -1000;
            }
            if (x->isInserting && !newHead) {
                newHead = x; //head may not surpass pending insert, inserting node is "newhead".
            }
            skiplistNode<_Key, _Data> *nxt = __sync_fetch_and_or(&x->next[0], 1); //logical deletion of successor to x.
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
            skiplistNode<_Key, _Data> *cur = observedHead;
            while (cur != newHead) {
                next = cur->next[0];
//                markRecycle(cur); //todo !!!!!!!!!!!!!!!!!!!!! what the fuck is this
                cur = next;
            }
        }
        return v;
    }

    void insert(_Key key, _Data val) {
        int height = getRandomHeight() ;
//        skiplistNode<_Key,_Data> * newNode = new skiplistNode<_Key, _Data>(key, val); //todo - function AllocNode. Also, what is "height"?
        skiplistNode<_Key,_Data> * newNode = new skiplistNode<_Key,_Data>(key, val, height);
        skiplistNode<_Key, _Data> * preds[this->levels];
        skiplistNode<_Key, _Data> * succs[this->levels];
        skiplistNode<_Key, _Data> * del = nullptr;
        do {
            this->locatePreds(key, preds, succs, del);
            newNode->next[0] = succs[0];
        } while (!__sync_bool_compare_and_swap(&preds[0]->next[0], succs[0], newNode));

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
                this->locatePreds(key, preds, succs, del);
                if (succs[0] != newNode)
                    break; //new has been deleted
            }
        }
        newNode->isInserting = false; //allow batch deletion past this node
    }


    void printSkipList(){
        skiplistNode<_Key,_Data> * node = head;
        for (int i = 0; i < this->levels-1; i ++){
            std::cout << "level " << i << " , head ";
            skiplistNode<_Key,_Data> * next = node->next[i];
            bool isDone = true;
            while (isDone){
                std::cout << next->key << " " ;
                next = next->next[i];
                isDone = next->next[0];
            }
            std::cout << std::endl;
        }
    }




};


int main() {

    skiplist<int, int> q = skiplist<int, int>(5);
    for (int i = 0; i < 7; i ++){
        q.insert(i,i);
        q.insert(i-7,i-7);
//        std::thread(q.insert, i, i);
    }

    q.printSkipList();


    int value = q.deleteMin();
    std::cout<<"value = " << value << std::endl;

    int value2 = q.deleteMin();
    std::cout<<"value2 = " << value2 << std::endl;

//    q.insert(3, 3);
//    q.insert(2,2);



    std::cout << "Hello, World!" << std::endl;
    return 0;
}