/*
 * avl.cc
 */

#include "avl.h"
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define height(n) ((n == nullptr) ? (-1) : (n->height))
#define abs(x) ((x < 0) ? (-x) : (x))
#define balance(n) (height(n->left) - height(n->right))
#define infinity 2147483647
#define GET_DATA(n) ((n == nullptr) ? (infinity) : (n->data))

template <typename T>
avl_node<T>* AVL<T>::rotate_right(avl_node<T>* n) {
    auto left_node = n->left;
    n->left = left_node->right;
    left_node->right = n;
    n->height = 1+MAX(height(n->left), height(n->right));
    left_node->height = 1+MAX(height(left_node->left), height(left_node->right));
    return left_node;
}

template <typename T>
avl_node<T>* AVL<T>::rotate_left(avl_node<T>* n) {
    auto right_node = n->right;
    n->right = right_node->left;
    right_node->left = n;
    n->height = 1+MAX(height(n->left), height(n->right));
    right_node->height = 1+MAX(height(right_node->left), height(right_node->right));
    return right_node;
}

template <typename T>
avl_node<T>* AVL<T>::rotate_left_right(avl_node<T>* n) {
    n->left = rotate_left(n->left);
    return rotate_right(n);
}

template <typename T>
avl_node<T>* AVL<T>::rotate_right_left(avl_node<T>* n) {
    n->right = rotate_right(n->right);
    return rotate_left(n);
}

template <typename T>
void AVL<T>::insert(T val) {
    auto node = new avl_node(val);
    root = insert_help(root, node);
}

template <typename T>
avl_node<T>* AVL<T>::insert_help(avl_node<T>* cur, avl_node<T>* new_node) {
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

template <typename T>
T AVL<T>::best_fit(T val) {
    auto node = best_fit_help(root, val);
}


template <typename T>
avl_node<T>* AVL<T>::best_fit_help(avl_node<T>* cur, T target) {
    if (cur == nullptr) return nullptr;
    if (cur->data < target) return best_fit_help(cur->right, target); // too small so it doesn't work
    if (cur->data > target) return MIN(cur->data, GET_DATA(best_fit_help(cur->left, target))); // current node works, but may be a better fit
    return cur->data; // first found; perfect match 
}


/*
template <typename T>
T AVL<T>::remove(T val) {

}
*/
