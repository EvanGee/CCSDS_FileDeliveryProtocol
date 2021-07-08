
/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef MY_LIST_H
#define MY_LIST_H
#include "stdint.h"

/*------------------------------------------------------------------------------
    Purpose: This is a node for the link list bellow
------------------------------------------------------------------------------*/
typedef struct Node
{
    uint32_t id;
    void *element;
    struct Node *next;
    struct Node *prev;
} Node;

/*------------------------------------------------------------------------------
    Purpose: This is a LIST struct that contains functions useful for 
    doing linked list functionality. 
------------------------------------------------------------------------------*/
typedef struct List
{
    struct Node *head;
    struct Node *tail;
    uint32_t count;
    int (*push)(struct List *list, void *element,  int id);
    void *(*remove)(struct List *list, int id, int (*f)(void *element, void *args), void *args);
    void (*iterate)(struct List *list, void (*f)(Node *node, void *element, void *args), void *args);
    void (*free)(struct List *list, void (*f)(void *element));
    void *(*pop) (struct List *list);
    int (*insert) (struct List *list, void *element, int id);
    int (*insertAt)(struct List *list, void *element, int id, int (*f)(void *element, void *args), void *args);
    //returns a void pointer that should be cast to the type
    void *(*find)(struct List *list, int id, int (*f)(void *element, void *args), void *args);
    struct Node *(*findNode)(struct List *list, int id, int (*f)(void *element, void *args), void *args);
    void (*freeOnlyList)(struct List *list);
    void (*freeNode)(Node *node);
    void *(*removeNode)(struct List *list, Node *node);

} List;

Node *createNode(void *element, int id);
/*------------------------------------------------------------------------------
    Purpose:    This function initializes a linked list LIST *.
    Perameters: empty is just for the compiler errors, TODO use it for something
    Return:     returns a pointer to an initilized LIST * 
------------------------------------------------------------------------------*/
List *linked_list(void);
#endif
