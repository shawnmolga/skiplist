//
// Created by shawn on 9/04/2019.
//

#ifndef SKIPLIST_SKIPLIST_H
#define SKIPLIST_SKIPLIST_H

#define BOUND_OFFSET = 2

#include "skiplistNode.h"


template <typename _Key, typename _Data>
class skiplist{
    skiplistNode<_Key,_Data>* head;
    skiplistNode<_Key,_Data>* tail;
    int levels;

    skiplist(int levels){
        this.head = nullptr;
        this.tail = nullptr;
        this.levels = levels;
    }

    bool removePrefix(int level, skiplistNode<_Key, _Data> &node) {
        this.head[level].next = &node;
        return true;
        //todo - when does this return false? why is in in "if" clause?
    }

    void locatePreds(int k, skiplistNode<int, int> * preds, skiplistNode<int, int> * succs[], skiplistNode<int, int> &del);
    _Data deleteMin();
    void locatePreds(_Key k, skiplistNode<_Key,_Data> * preds, skiplistNode<_Key,_Data> * succs, skiplistNode<_Key,_Data>& del);
    void Insert(_Key key, _Data val);
    void Restructure();
    bool removePrefix(int level, skiplistNode<_Key, _Data> & node);
    bool logicalDelete(skiplistNode<_Key, _Data> & node);
    skiplistNode<_Key,_Data> getNextNode(skiplistNode<_Key, _Data> * node, int level);
    bool isNodeDeleted(skiplist * node);







    };


#endif //SKIPLIST_SKIPLIST_H
