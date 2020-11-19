/*
 * avl.cc
 */

#include "avl.h"
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define height(n) ((n == 0) ? -1 : n.height)

template <typename T>
avl_node<T> AVL<T>::rotate_right(avl_node<T> n) {
    auto left_node = n.left;
    n.left = left_node.right;
    left_node.right = n;
    n.height = 1+MAX(height(n.left), height(n.right));
    left_node.height = 1+MAX(height(left_node.left), height(left_node.right));
    return left_node;
}

template <typename T>
avl_node<T> AVL<T>::rotate_left(avl_node<T> n) {
    auto right_node = n.right;
    n.right = right_node.left;
    right_node.left = n;
    n.height = 1+MAX(height(n.left), height(n.right));
    right_node.height = 1+MAX(height(right_node.left), height(right_node.right));
    return right_node;
}

template <typename T>
avl_node<T> AVL<T>::rotate_left_right(avl_node<T> n) {
    n.left = rotate_left(n.left);
    return rotate_right(n);
}

template <typename T>
avl_node<T> AVL<T>::rotate_right_left(avl_node<T> n) {
    n.right = rotate_right(n.right);
    return rotate_left(n);
}

template <typename T>
void AVL<T>::insert(T val) {
}
