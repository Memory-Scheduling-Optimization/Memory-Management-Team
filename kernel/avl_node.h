/*
 * avl_node.h
 */
#ifndef _avl_node_h_
#define _avl_node_h_

#include "stdint.h"

template <typename T>
class avl_node {
public:
    avl_node<T> *left;
    avl_node<T> *right;
    T data;
    int32_t height;
    avl_node(T d) : data(d) {} 
};

#endif


