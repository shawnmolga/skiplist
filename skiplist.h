//
// Created by shawn on 9/04/2019.
//

#ifndef SKIPLIST_SKIPLIST_H
#define SKIPLIST_SKIPLIST_H

#define BOUND_OFFSET = 2

#include "skiplistNode.h"


template <typename _Key, typename _Data>
class skiplist{
    skiplistNode* head;
    skiplistNode* tail;
    int levels;

    skiplist(int levels){
        this.head = nullptr;
        this.tail = nullptr;
        this.levels = levels;
    }

    _Data deleteMin();

};


#endif //SKIPLIST_SKIPLIST_H
