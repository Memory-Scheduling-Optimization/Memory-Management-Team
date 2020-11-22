/*
 * avl.h
 */
#ifndef _AVL_H_
#define _AVL_H_

#include "avl_node.h"
#include "debug.h"

template <typename T>
class AVL {
private:
    static avl_node<T> *root;
    static uint32_t size;
    static avl_node<T>* insert_help(avl_node<T>* cur, avl_node<T>* new_node);
    static avl_node<T>* best_fit_help(avl_node<T>* cur, T target);
public:
    AVL() { 
        root = nullptr;
	size = 0;
    } 
    ~AVL() {}
    AVL<T>& operator=(const AVL& rhs);

    static void insert(T val);
    static void remove(T val);
    static T best_fit(T val);
    static avl_node<T>* rotate_right(avl_node<T>* n);
    static avl_node<T>* rotate_left(avl_node<T>* n);
    static avl_node<T>* rotate_left_right(avl_node<T>* n);
    static avl_node<T>* rotate_right_left(avl_node<T>* n);
    static avl_node<T>* get_root() { return root; }
};

#endif
