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
#define balance(n) ((n == nullptr) ? (0) : (height(n->left) - height(n->right)))
#define infinity 2147483647
#define GET_DATA(n) ((n == nullptr) ? (infinity) : (n->data))

template <typename T>
class AVL {
private:
    avl_node<T> *root;
    uint32_t size;

    avl_node<T>* insert_help(avl_node<T>* cur, avl_node<T>* new_node) {
        if (cur == nullptr) {
	    size++;
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

    avl_node<T>* best_fit_help(avl_node<T>* cur, T target) {
        if (cur == nullptr) return nullptr;
        if (cur->data < target) return best_fit_help(cur->right, target); // too small so it doesn't work
        if (cur->data > target) {
	    avl_node<T>* a_try = best_fit_help(cur->left, target);
            return (cur->data < GET_DATA(a_try)) ? (cur) : (a_try);
	//	return (cur->data < GET_DATA(best_fit_help(cur->left, target))) ? (cur) : (cur->left); // current node works, but may be a better fit
	}
        return cur; // first found; perfect match 
    }

    avl_node<T>* remove_help(avl_node<T>* cur, T val) {
        if (cur == nullptr) return nullptr;
        if (val < cur->data) cur->left = remove_help(cur->left, val);
        else if (val > cur->data) cur->right = remove_help(cur->right, val);
        // o.w. it's a match
	else {
            if (cur->left == nullptr || cur->right == nullptr) {
	        if (cur->left == nullptr && cur->right == nullptr) {
		    free(cur);
		    cur = nullptr;
		} else {
		    avl_node<T>* nonempty = (cur->left == nullptr) ? cur->right : cur->left;
		    cur->data = nonempty->data; // we have to copy the contents of nonempty into cur
		    cur->left = nonempty->left;
		    cur->right = nonempty->right;
		    cur->height = nonempty->height;
		    free(nonempty);
		}
		size--;
	    } else {
	        // two children case
		avl_node<T>* replacement = cur->right;

		while (replacement->left != nullptr) { // get min value in cur's right subtree
		    replacement = replacement->left;
		}

		cur->data = replacement->data;
                cur->right = remove_help(cur->right, replacement->data); // recursive delete call needed
	    }
	}

	if (cur == nullptr) return cur; // if cur now is nullptr, can just return
	cur->height = 1+MAX(height(cur->left), height(cur->right));
        const int thresh = 1; // threshold of 1 for now
        if (balance(cur) > thresh) {
            if (balance(cur->left) >= 0) {
                cur = rotate_right(cur);
             } else { // balance(cur->left) < 0
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

    void destroy_tree(avl_node<T> *cur) {
        if (cur == nullptr) return;
	avl_node<T> *lhs = cur->left;
	avl_node<T> *rhs = cur->right;
	delete(cur);
	destroy_tree(lhs);
	destory_tree(rhs);
    }

public:
    AVL() { 
        root = nullptr;
	size = 0;
    } 
    ~AVL() {
        destroy_tree(root);
	size = 0;
	root = nullptr;
    }
    AVL<T>& operator=(const AVL& rhs);

    void insert(T val) {
        auto node = new avl_node(val);
        root = insert_help(root, node);
    }

    void remove(T val) {
        root = remove_help(root, val);
    }

    T best_fit(T val) {
        auto node = best_fit_help(root, val);
	return (node == nullptr) ? T{} : node->data;
    }
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

    avl_node<T>* get_root() { 
	    return root; 
    }
};



#endif
