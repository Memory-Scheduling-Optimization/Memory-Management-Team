/*
 * avl.h
 */
#ifndef _AVL_H_
#define _AVL_H_

#include "avl_node.h"
#include "debug.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define height(n) ((n == nullptr) ? (-1) : (n->height))
#define abs(x) ((x < 0) ? (-x) : (x))
#define balance(n) (height(n->left) - height(n->right))
#define infinity 2147483647
#define GET_DATA(n) ((n == nullptr) ? (infinity) : (n->data))

template <typename T>
class AVL {
private:
    avl_node<T> *root;
    uint32_t size;
    avl_node<T>* insert_help(avl_node<T>* cur, avl_node<T>* new_node) {
        if (cur == nullptr) {
            return new_node;        
        }
        if (new_node->data < cur->data) {
            cur->left = insert_help(cur->left, new_node); // recursive call
        } else {
            cur->right = insert_help(cur->right, new_node); // recursive call
        }
        cur->height = 1+MAX(height(cur->left), height(cur->right));
        const int thresh = 1; // threshold of 1 for now
        if (balance(cur) > thresh) {
            if (balance(cur->left) >= 0) {
                cur = rotate_right(cur);
             } else {
                 cur = rotate_left_right(cur);
             }
        } else if (balance(cur) < -thresh) {
            if (balance(cur->right) <= 0) {
                cur = rotate_left(cur);
            } else {
                cur = rotate_right_left(cur);
            }
        }
        return cur;
    }
    avl_node<T>* best_fit_help(avl_node<T>* cur, T target);
public:
    AVL() { 
        root = nullptr;
	size = 0;
    } 
    ~AVL() {}
    AVL<T>& operator=(const AVL& rhs);

    void insert(T val) {
        auto node = new avl_node(val);
        root = insert_help(root, node);
    }

    void remove(T val);
    T best_fit(T val);
    avl_node<T>* rotate_right(avl_node<T>* n) {
        auto left_node = n->left;
        n->left = left_node->right;
        left_node->right = n;
        n->height = 1+MAX(height(n->left), height(n->right));
        left_node->height = 1+MAX(height(left_node->left), height(left_node->right));
        return left_node;
    }

    avl_node<T>* rotate_left(avl_node<T>* n) {
        auto right_node = n->right;
        n->right = right_node->left;
        right_node->left = n;
        n->height = 1+MAX(height(n->left), height(n->right));
        right_node->height = 1+MAX(height(right_node->left), height(right_node->right));
        return right_node;
    }

    avl_node<T>* rotate_left_right(avl_node<T>* n) {
        n->left = rotate_left(n->left);
        return rotate_right(n);
    }

    avl_node<T>* rotate_right_left(avl_node<T>* n) {
        n->right = rotate_right(n->right);
        return rotate_left(n);
    }
    avl_node<T>* get_root() { return root; }
};



#endif
