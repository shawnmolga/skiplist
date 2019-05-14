//
// Created by shawn on 9/04/2019.
//
#include "skiplist.h"
#include "skiplistNode.h"


/*a skiplist stores a set of key-value pairs, ordered according to the keys.*/


        bool skiplist<int, int>::logicalDelete(skiplistNode<int, int> &node) {
            //todo - atomic?
            bool previous_value = node.getIsNextNodeDeleted(0);//todo - check getIsNextNodeDeleted input - level 0?
            node.setIsNextNodeDeleted(true);
            return previous_value;
        }

        void skiplist::Restructure() {
            int i = this->levels - 1;
            skiplistNode<int, int> * pred = this->head;
            while (i > 0) {
                skiplistNode<int, int> h = getNextNode(this->head->next[i], 0);
                skiplistNode<int, int> cur = getNextNode(pred->next[i - 1], 0);
//            if (!h.isNextNodeDeleted){
                if (!(h.next[0])->getIsNextNodeDeleted(0)) {//todo - fix this
                    i--;
                    continue;
                }
                if ((cur.next[0])->getIsNextNodeDeleted(0)) {
                    pred = &cur;
                    cur = *pred->next[i-1]->getNextNode(0);
                }
                if ((i, pred->next[i])) {
                    i--;
                }
            }
        }

        int skiplist::deleteMin() {
            skiplistNode<int, int>*  x = this.head;
            int offset = 0;
            skiplistNode<int, int> * newHead = nullptr;
            skiplistNode<int, int> observedHead = getNextNode(x,0);
            do {
                skiplistNode<int, int> next = getNextNode();
                bool isNextNodeDeleted = isNodeDeleted(x.next[0]);
                if (next == *this->tail) { //if queue is empty - return
                    return -1; //todo - fix this
                }
                if (x->isInserting && !newHead) {
                    newHead = x; //head may not surpass pending insert
                    //todo  - delete x?
                }
                //todo - FAO
                skiplistNode<int, int> nextNode = getNextNode(x.next[0]);
                bool deleted = std::atomic_fetch_or(&nextNode, true);//todo - check this. should change only LAST BIT
                // bool deleted = logicalDelete(x);
                offset++;
                x = next;
            } while (deleted);

            //todo - exclusive access
            int v = x.value;

            if (offset < this. BOUND_OFFSET) {
                return v;
            }
            if (newHead == 0) {
                newHead = x;
            }

//        if (removePrefix(q, x)) {
            if (std::atomic_compare_exchange_strong(x.head[0], x.head[0], newHead)) {
                Restructure(0, q);
                cur = observedHead;
                while (cur != newHead) {
                    next = getNextNode(cur);
                    markRecycle(cur);
                    cur = next;
                }
            }
            return v;
        }

        void skiplist::Insert(int key, _Value val) {
            int height = Random(1, this->levels);
            skiplistNode<int, int> newNode = skiplistNode<int, int>(height); //todo - function AllocNode. Also, what is "height"?
            newNode.key = key;
            do {
                skiplistNode<int, int> preds[this.levels];
                skiplistNode<int, int> succs[this.levels];
                skiplistNode<int, int> del = NULL;
                locatePreds(key, preds, succs, del);
                newNode.next[0] = succs[0];
            } while (!std::atomic_compare_exchange_strong(getNextNode(preds[0], 0), getNextNode(succs[0], 0), newNode));

            int i = 1;
            while (i < height) { //insert node at higher levels
                newNode.next[i] = succ[i];
                if (newNode.getIsNextNodeDeleted() || succs[i].isNextDeleted() || succs[i] == del) {
                    break; //todo -  what is that last one?????
                }
                if (std::atomic_compare_exchange_strong(getNextNode(preds[i], i), succs[i], newNode)) {
                    i++; // if successful - ascend to next level
                } else {
                    //todo - locate preds
                    if (succs[0] != newNode)
                        break; //new has been deleted
                }
            }
            newNode.isInserting = false; //allow batch deletion past this node
        }

        void skiplist::locatePreds(int k, skiplistNode<int, int> * preds, skiplistNode<int, int> * succs[], skiplistNode<int, int> &del) {
            int i = this.levels - 1;
            skiplistNode<int, int> pred = this.head;
//        skiplistNode del = NULL;
            while (i >= 0) {
                skiplistNode<int, int> cur = getNextNode(pred, i);
                bool isDel = getIsNextNodeDeleted(pred.next, i);
                while (cur.key < k || isDel || (isDel && i == 0)) {
                    if ((isDel) && i == 0)
                        del = cur;
                    pred = cur;
                    skiplistNode<int, int> cur = getNextNode(pred, i);
                    isDel = isNextNodeDeleted(pred.next, i);
                }
                preds[i] = pred;
                succs[i] = &cur;
                i--;
            }
        }