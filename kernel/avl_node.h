/*
 * avl_node.h
 */
#ifndef _avl_node_h_
#define _avl_node_h_

#include "stdint.h"

template <typename T>
class avl_node {
private:
    avl_node<T> *left;
    avl_node<T> *right;
    T data;
public:
    int32_t height;
};

#endif


