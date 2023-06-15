// Header file for a Generic Linked List data structure.

#pragma once

#include <stdbool.h>

typedef struct linked_list_element {
    void *val;
    struct linked_list_element *next;
    struct linked_list_element *prev;
    int pid;
} linked_list_elem;

typedef struct linked_list_st {
    linked_list_elem *head;
    linked_list_elem *tail;
    int size;
} linked_list;

// Initialize an empty linked list.
void init_linked_list(linked_list *linked_list);

// Check if a specified linked list is empty.
int is_empty(linked_list *linked_list);

// Returns val in the first element of the linked list, doesn't remove the element from the list
// void* peek(linked_list *linked_list); 

// Remove the element at the head of the linked list.
// Returns the pointer to that element. Returns NULL
// if empty.
void *pop_head(linked_list *linked_list);

// Push an element to the back of the linked list.
void push_back(linked_list *linked_list, void *j);

// Push an element to the back of the linked list, used for scheduler	
void push_back_with_pid(linked_list *linked_list, void *j, int pid);

// Clear the entire linked list. Should pass in a function that
// frees the popped values, if they have dynamically allocated
// fields in the struct.
void clear(linked_list *linked_list, void (*free_value)(void *));

// Removes the first element from the list for which predicate evaluates to true.
// Also, requires a function for freeing the dynamically allocated value.
// Returns true if a value was remove, false otherwise.
// predicate is called with predicate(predicate_arg, val) where val is the stored void*
// at the current linked_list_element in the traversal.
bool remove_elem(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg,
                 void (*free_value)(void *));


// Removes and returns the target element (does not free the node, unlike remove_elem).
// This is used to move u_context nodes bewteen different priority queues in the scheduler
void *extract_elem(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg);

// Returns but does not remove the target element. See remove_elem for predicate details.
linked_list_elem *get_elem(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg);

// Check if a specific linked_list_elem element exists in the linked list.
bool elem_exists(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg);
