/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "list.h"
#include "port.h"

/*------------------------------------------------------------------------------
    This function creates a new node to add into the linked list, returns the
    new node, or NULL if failed
------------------------------------------------------------------------------*/

Node *createNode(void *element, uint32_t id)
{
    Node *newNode = ssp_alloc(sizeof(Node), 1);
    if (newNode == NULL) {
        return NULL;
    }
    newNode->element = element;
    newNode->id = id;
    return newNode;
}

/*------------------------------------------------------------------------------
    This function creates a new node to add into the linked list, returns the
    new node. 
------------------------------------------------------------------------------*/


static void freeNode(Node *node) {
    if (node != NULL)
        ssp_free(node);    
}

static void *pop(List *list) {

    if (list->count == 0)
        return NULL;

    Node *last_data_node = list->tail->prev;
    
    Node *prev = last_data_node->prev;
    prev->next = list->tail;
    list->tail->prev = prev;

    list->count--;
    void *element = last_data_node->element;
    freeNode(last_data_node);
    return element;
}


/*------------------------------------------------------------------------------
    This function creates a new node to add into the beginning of the
    linked list, returns the new node. 
------------------------------------------------------------------------------*/

static int insert(List *list, void *element, uint32_t id) {

    Node *head = list->head;
    Node *node = createNode(element, id);
    if (node == NULL) {
        return 0;
    }

    node->next = head->next;
    node->next->prev = node;
    node->prev = head;

    head->next = node;
    list->count++;
    return 1;
}

/*------------------------------------------------------------------------------
    This function adds a new element into the linked list. returns 1 if success
    0 if failed.
------------------------------------------------------------------------------*/

static int push(List *list, void *element, uint32_t id)
{

    Node *newNode = createNode(element, id);
    if (newNode == NULL) {
        return 0;
    }
    Node *tail = list->tail;

    newNode->next = tail;
    newNode->prev = tail->prev;

    tail->prev->next = newNode;
    tail->prev = newNode;

    if (list->count == 0) {
        list->head->next = newNode;
    }
    
    list->count++;
    return 1;
}

/*------------------------------------------------------------------------------
    This function will print out the list if given a callback that is designed
    to print out an element.
------------------------------------------------------------------------------*/

static void iterate(List *list, void (*f)(Node *node, void *element, void *args), void *args)
{
    Node *cur = list->head->next;
    Node *next;
    while (cur->next != NULL && cur != list->tail)
    {
        next = cur->next;
        f(cur, cur->element, args);
        cur = next;
    }
}

//deletes the node, and returns the object, (motifys the list)
static void *removeNode(List *list, Node *node) {


    Node *previous = node->prev;
    Node *next = node->next;

    previous->next = next;
    next->prev = previous;

    void *element = node->element;
    list->freeNode(node);
    list->count--;
    return element;
}


/*------------------------------------------------------------------------------
    This function removes an element from the linked list, returns the node stored if success
    and NULL if item not found. it can use either an id, or callback to find the
    element (callback can be the find function)
------------------------------------------------------------------------------*/

static void *removeElement(List *list, uint32_t id, int (*f)(void *element, void *args), void *args)
{
    Node *cur = list->head;
    int found_with_func = 0;
    int found_with_id = 0;
    while (cur->next != NULL)
    {
        if (f != NULL)
            found_with_func = f(cur->element, args);

        if (id == cur->id)
            found_with_id = 1;

        if (found_with_func || found_with_id)
        {
            void *element = removeNode(list, cur);
            return element;
        }
        cur = cur->next;
    }
    return NULL;
}



/*------------------------------------------------------------------------------
    frees the linked list. Takes a free function that is a function pointer to 
    a function that frees and elemnent.  Returns nothing,
------------------------------------------------------------------------------*/

static void freeList(List *list, void (*f)(void *element))
{
    Node *cur = list->head->next;

    while (cur->next != NULL)
    {
        Node *n = cur;
        cur = cur->next;
        f(n->element);
        freeNode(n);
    }
    ssp_free(list->head);
    ssp_free(list->tail);
    ssp_free(list);
}


static void freeNodes(List *list) {

    Node *cur = list->head->next;

    while (cur->next != NULL)
    {
        Node *n = cur;
        cur = cur->next;
        freeNode(n);
    }
    ssp_free(list->head);
    ssp_free(list->tail);
    ssp_free(list);

}

/*------------------------------------------------------------------------------
    This function finds an element, returns and element on success and NULL on
    failure. The return value should be cast to the element type. can search with
    either a callback, or id
------------------------------------------------------------------------------*/

static void *findElement(List *list, uint32_t id, int (*f)(void *element, void *args), void *args)
{

    Node *cur = list->head->next;
    int found_with_func = 0;
    int found_with_id = 0;
    while (cur->next != NULL)
    {
        if (f != NULL)
            found_with_func = f(cur->element, args);

        if(id != 0 && cur->id == id)
            found_with_id = 1;

        if (found_with_func || found_with_id){
            return cur->element;
        }
        cur = cur->next;
    }
    return NULL;
}

static int insertAt(List *list, void *element, uint32_t id, int (*f)(void *element, void *args), void *args) {
  
    Node *cur = list->head->next;
    int found_with_func = 0;
    int found_with_id = 0;
    while (cur->next != NULL)
    {
        if (f != NULL)
            found_with_func = f(cur->element, args);

        if(id != 0 && cur->id == id)
            found_with_id = 1;

        if (found_with_func || found_with_id) {
            Node *new = createNode(element, id);
            new->next = cur;
            new->prev = cur->prev;
            new->prev->next = new;
            cur->prev = new;
            list->count++;
        }
            
        cur = cur->next;
    }
    return -1;
}

 
static Node *findNode(List *list, uint32_t id, int (*f)(void *element, void *args), void *args) {

    Node *cur = list->head->next;
    int found_with_func = 0;
    int found_with_id = 0;
    while (cur->next != NULL)
    {
        if (f != NULL)
            found_with_func = f(cur->element, args);

        if(id != 0 && cur->id == id)
            found_with_id = 1;

        if (found_with_func || found_with_id){
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}
//see header file return NULL if fails

List *linked_list()
{
    List *newList = ssp_alloc(sizeof(List), 1);
    if (newList == NULL) 
        return NULL;

    newList->head = createNode(NULL, 0);
    if (newList->head == NULL) {
        ssp_free(newList);
        return NULL;
    }

    newList->tail = createNode(NULL, 0);
    if (newList->tail == NULL) {
        ssp_free(newList->head);
        ssp_free(newList);
        return NULL;
    }
    
    Node *tail = newList->tail;
    Node *head = newList->head;

    tail->prev = head;
    tail->next = NULL;
    head->next = tail;
    head->prev = NULL;

    newList->count = 0;
    newList->push = push;
    newList->remove = removeElement;
    newList->iterate = iterate;
    newList->free = freeList;
    newList->insert = insert;
    newList->pop = pop;
    newList->find = findElement;
    newList->insertAt = insertAt;
    newList->findNode = findNode;
    newList->freeNode = freeNode;
    newList->freeOnlyList = freeNodes;
    newList->removeNode = removeNode;

    return newList;
}
