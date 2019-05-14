//
// Created by shawn on 9/04/2019.
//

#ifndef SKIPLIST_SKIPLISTNODE_H
#define SKIPLIST_SKIPLISTNODE_H
using namespace std;

template <typename _Key, typename _Data>
class skiplistNode {
    /*array of pointers , entry i points to "next" skiplistNode on level i
      the last bit in next[i] indicates whether or not next[i] is deleted or not */
public:

    bool isInserting;
    _Key key;
    _Data value;
    skiplistNode<_Key, _Data> * next[];


    skiplistNode(int height){

        //todo - what are the initial values?
    }


    bool getIsNextNodeDeleted(int level){
//        return next[level];
        return true;
    }

    bool getIsNodeDeleted(int level){
//        return next[level];
        return true;
    }

    bool setIsNextNodeDeleted(int level){
        return next[level];

    }

    void setIsNodeDeleted(bool isDeleted){
        //todo do - get last bit of *this pointer
    }


    skiplistNode<int, int> * getNextNode(int level) {
        //todo - return the next node. Do we need to lock?
        //todo - last bit needs to be 0????
        return this->next[level];
    }
};


#endif //SKIPLIST_SKIPLISTNODE_H
