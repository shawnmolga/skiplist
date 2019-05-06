//
// Created by shawn on 9/04/2019.
//
#include "skiplist.h"
#include "skiplistNode.h"


/*a skiplist stores a set of key-value pairs, ordered according to the keys.*/

    /****************MEMORY LIBRARY******************/
    bool isNodeDeleted(skiplist<key_t key, value_t val> * node){
        //returns true if last bit of the pointer is 1.
        // o.w returns false
        //todo
    }

    skiplistNode getNextNode(skiplistNode * node, int level){
        //todo - return the next node. Do we need to lock?
        return node->next[level];
    }
    /************************************************/

    bool logicalDelete(skiplistNode & node) {
        //todo - atomic?
        bool previous_value  = node.isNextNodeDeleted;
        node.isNextNodeDeleted = true;
        return previous_value;
    }

    bool removePrefix(int level, skiplistNode & node){
        this.head[level].next = &node;
        return true;
        //todo - when does this return false? why is in in "if" clause?
    }

    void Restructure(){
        int i = this.levels -1;
        skiplistNode pred = this.head;
        while (i > 0) {
            skiplistNode h = getNextNode(this.head.next[i]);
            skiplistNode cur = getNextNode(pred.next[i - 1]);
//            if (!h.isNextNodeDeleted){
            if (isNodeDeleted(h.next[0])) {
                i--;
                continue;
            }
//            while (cur.isNextNodeDeleted){
            if (isNodeDeleted(isNodeDeleted(cur.next[0]))) {
                pred = cur;
                cur = getNextNode(pred.next[i - 1]);
//                cur = pred.next[i];
            }
            if (removePrefix(i, pred.next[i])) {
                i--;
            }
        }
    }

    _Data deleteMin() {
        x = this.head;
        offset = 0;
        newHead = NULL;
        observedHead = x.next[0];
        do {
            skiplistNode next = getNextNode();
            bool isNextNodeDeleted = isNodeDeleted(x.next[0]);
            if (next == this.tail) { //if queue is empty - return
                return -1; //todo - fix this
            }
            if (x.isInserting && newHead == NULL) {
                newHead = x; //head may not surpass pending insert
            }
            //todo - FAO
            skiplistNode nextNode = getNextNode(x.next[0]);
            bool deleted = std::atomic_fetch_or(&nextNode, true);//todo - check this. should change only LAST BIT
            // bool deleted = logicalDelete(x);
            offset++;
            x = next;
        } while (deleted);

        //todo - exclusive access
        int v = x.value;

        if (offset < this.BOUND_OFFSET) {
            return v;
        }
        if (newHead= NULL) {
            newHead = x;
        }

//        if (removePrefix(q, x)) {
        if (std::atomic_compare_exchange_strong(x.head[0], x.head[0], newHead)){
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

    void Insert(key_t key, value_t val){
        height = Random(1, this.levels);
        newNode = skiplistNode(height); //todo - function AllocNode. Also, what is "height"?
        newNode.key = key;
        do {
            //todo - locate preds
            newNode.next[0] = succs[0];
        } while(!atomic_compare_exchange_strong(getNextNode(pred,0), succ[0], newNode ));

        int i = 1;
        while (i < height){ //insert node at higher levels
            newNode.next[i] = succ[i];
            if (newNode.isNextDeleted() || succs[i].isNextDeleted() || succs[i] == del){
                break; //todo -  what is that last one?????
            }
            if (atomic_compare_exchange_strong(getNextNode(preds[i], i), succs[i], newNode)){
                i++; // if successful - ascend to next level
            }
            else{
                //todo - locate preds
                if (succs[0] != newNode)
                    break; //new has been deleted
            }
        }
        newNode.inserting = false; //allow batch deletion past this node
    }

    void locatePreds(key_t k){
        int i = this.levels - 1;
        skiplistNode pred = this.head;
        skiplistNode del = NULL;
        skipListNode preds[];
        while (i >= 0){
            skiplistNode cur = getNextNode(pred, i);
            isDel = isNodeDeleted(pred.next, i);
            while (cur.key < k|| isDel || (isDel && i==0)){
                if ((isDel) && i==0)
                    del = cur;
                pred = cur;
                skiplistNode cur = getNextNode(pred, i);
                isDel = isNodeDeleted(pred.next, i);
            }
            preds[i] = pred;
            succs[i] = cur;
            i--;
        }
        return preds , succs, del; //todo  -what the actual fuck? tuple?
    }



