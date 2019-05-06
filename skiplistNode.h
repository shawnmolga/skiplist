//
// Created by shawn on 9/04/2019.
//

#ifndef SKIPLIST_SKIPLISTNODE_H
#define SKIPLIST_SKIPLISTNODE_H

template <typename _Key, typename _Data>
struct skiplistNode {
    /*array of pointers , entry i points to "next" skiplistNode on level i
      the last bit in next[i] indicates whether or not next[i] is deleted or not */
    skiplistNode * next[];
    bool isInserting;
    _Key key;
    _Data value;


    skiplistNode(){
        //todo - what are the initial values?
    }

    isNextDeleted(){
        //todo - lock??
        //todo - return last bit of "next" pointer
        return true;
    }
};


#endif //SKIPLIST_SKIPLISTNODE_H
