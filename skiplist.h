//
// Created by shawn on 9/04/2019.
//

#ifndef SKIPLIST_SKIPLIST_H
#define SKIPLIST_SKIPLIST_H

#define BOUND_OFFSET = 2

template <typename _Key, typename _Data>
struct skiplist{
    skiplistNode* head;
    skiplistNode* tail;
    int levels;

    skiplist(levels){
        this.head = nullptr;
        this.tail = nullptr;
        this.levels = levels;
    }

    _Data deleteMin();

};


#endif //SKIPLIST_SKIPLIST_H
