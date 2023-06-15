// Implementation of a Generic Linked List data structure.

#include "linked_list.h"
#include <stdlib.h>
#include <stdio.h>

void init_linked_list(linked_list *linked_list) {
    linked_list->head = NULL;
    linked_list->tail = NULL;
    linked_list->size = 0;
}

int is_empty(linked_list *linked_list) {
    return linked_list->size == 0;
}

void push_back(linked_list *linked_list, void *v) {
    linked_list_elem *new_node;

    new_node = malloc(sizeof(*new_node));
    if (new_node == NULL) {
        perror("Memory Allocation Failed For linked_list Node\n");
        return;
    }

    new_node->val = v;
    new_node->next = NULL;

    // Pushing first element to linked list.
    if (linked_list->size == 0) {
        new_node->prev = NULL;

        linked_list->head = new_node;
        linked_list->tail = new_node;
        linked_list->size++;
        return;
    }

    // Linked list isn't empty, thus a head and tail must exist.
    linked_list->tail->next = new_node;
    new_node->prev = linked_list->tail;
    linked_list->tail = new_node;

    linked_list->size++;
}

void push_back_with_pid(linked_list *linked_list, void *v, int pid) {
    linked_list_elem *new_node;
    new_node = malloc(sizeof(*new_node));

    if (new_node == NULL) {
        perror("Memory Allocation Failed For linked_list Node\n");
        exit(EXIT_FAILURE);
    }

    new_node->val = v;
    new_node->next = NULL;

    // Pushing first element to linked list.
    if (linked_list->size == 0) {
        new_node->prev = NULL;
        linked_list->head = new_node;
        linked_list->tail = new_node;
        linked_list->size++;
        return;
    }

    // Linked list isn't empty, thus a head and tail must exist.
    linked_list->tail->next = new_node;
    new_node->prev = linked_list->tail;
    linked_list->tail = new_node;
    linked_list->size++;
}


void *pop_head(linked_list *linked_list) {
    linked_list_elem *cur_head;
    void *v = NULL;

    if (!is_empty(linked_list)) {
        cur_head = linked_list->head;

        v = cur_head->val;

        linked_list->head = cur_head->next;
        if (linked_list->head != NULL) {
            linked_list->head->prev = NULL;
        } else {
            linked_list->tail = NULL;
        }

        linked_list->size--;

        free(cur_head);
        cur_head = NULL;
    }

    return v;
}

void clear(linked_list *linked_list, void (*free_value)(void *)) {
    while (!is_empty(linked_list)) {
        void *v = pop_head(linked_list);
        free_value(v);
    }
}

bool remove_elem(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg,
                 void (*free_value)(void *)) {
    linked_list_elem *cur_elem;

    if (!is_empty(linked_list)) {
        cur_elem = linked_list->head;

        while (cur_elem != NULL) {
            if (predicate(predicate_arg, cur_elem->val)) {
                linked_list_elem *prev = cur_elem->prev;
                linked_list_elem *next = cur_elem->next;

                if (prev != NULL) {
                    prev->next = next;
                }

                if (next != NULL) {
                    next->prev = prev;
                }

                if (cur_elem == linked_list->head) {
                    linked_list->head = next;
                }

                if (cur_elem == linked_list->tail) {
                    linked_list->tail = prev;
                }

                linked_list->size--;
                free_value(cur_elem->val);
                free(cur_elem);

                cur_elem = NULL;

                return true;
            }

            cur_elem = cur_elem->next;
        }
    }

    return false;
}

// Removes the element but doesn't free the node, this is used to move u_context from 1 linkedlist to another
void *extract_elem(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg) {
    linked_list_elem *cur_elem;
    void *cur_val;

    if (!is_empty(linked_list)) {
        cur_elem = linked_list->head;

        while (cur_elem != NULL) {
            if (predicate(predicate_arg, cur_elem->val)) {
                cur_val = cur_elem->val;

                linked_list_elem *prev = cur_elem->prev;
                linked_list_elem *next = cur_elem->next;
                if (prev != NULL) {
                    prev->next = next;
                }
                if (next != NULL) {
                    next->prev = prev;
                }
                if (cur_elem == linked_list->head) {
                    linked_list->head = next;
                }
                if (cur_elem == linked_list->tail) {
                    linked_list->tail = prev;
                }
                linked_list->size--;
                free(cur_elem);

                return cur_val;
            }

            cur_elem = cur_elem->next;
        }
    }

    return NULL;
}

linked_list_elem *get_elem(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg) {
    if (linked_list == NULL) {
        return NULL;
    }

    for (linked_list_elem *l = linked_list->head; l != NULL; l = l->next) {
        if (predicate(predicate_arg, l->val)) {
            return l;
        }
    }

    return NULL;
}


bool elem_exists(linked_list *linked_list, bool (*predicate)(void *, void *), void *predicate_arg) {
    linked_list_elem *cur_elem;

    if (!is_empty(linked_list)) {
        cur_elem = linked_list->head;

        while (cur_elem != NULL) {
            if (predicate(predicate_arg, cur_elem->val)) {
                return true;
            }

            cur_elem = cur_elem->next;
        }

        return false;
    }

    return false;
}
