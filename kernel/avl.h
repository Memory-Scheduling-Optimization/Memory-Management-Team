/*
 * avl.h
 */
#ifndef _avl_h_
#define _avl_h_

#include "stdint.h"

template <typename T>
class avl_node {
    avl_node<T> *left;
    avl_node<T> *right;
    T data;
    uint32_t height;
};

template <typename T>
class AVL {
private:
    avl_node<T> *root;
public:
    AVL() : root(nullptr), size(0) {}
    ~AVL() {}
    AVL<T>& operator=(const AVL& rhs);

    void insert(T val);
    T remove(T val);
    T best_fit(T val);
    avl_node<T> rotate_right(avl_node<T> n);
    avl_node<T> rotate_left_right(avl_node<T> n);
    avl_node<T> rotate_left(avl_node<T> n);
    avl_node<T> rotate_right_left(avl_node<T> n);
}

#endif
