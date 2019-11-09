


#include "list.h"

#include "protocol_handler.h"
#include "requests.h"

#include "stdio.h"
#include "test.h"
#include "stdlib.h"
#include "string.h"

#include "unit_tests.h"


List *populate_request_list() {
    List *list = linked_list();
    Request *r;
    for (int i = 0; i < 5; i++) {
        r = init_request(100);
        r->dest_cfdp_id = i;
        list->push(list, r, i);
    }
    return list;
}


void print_list(Node *node, void *element, void *args) {
    Request *r = (Request *) element;
    printf("source file name: %s\n", r->source_file_name);
}

void print_list_ids(Node *node, void *element, void *args) {
    Request *r = (Request *) element;
    ASSERT_EQUALS_INT("should equal ids", node->id, r->dest_cfdp_id);
    
}

void remove_node_test(Node *node, void *element, void *args) {
    Request *r = (Request *) element;
    List *l = (List*) args;

    if (r->dest_cfdp_id == 2) {
        void *element = l->removeNode(l, node);
        ssp_cleanup_req(element);
    }
}

void test_remove_node() {

    List *l = populate_request_list();
    l->iterate(l, remove_node_test, l);
    l->iterate(l, print_list_ids, l);
    l->free(l, ssp_cleanup_req);
}



int list_tests() {

    DECLARE_NEW_TEST("list.c");
    List *list = linked_list();
    
    Request  *r = init_request(10000);
    Request *r2 = init_request(10000);

    memcpy(r->source_file_name, "mybestfriend", 12);
    memcpy(r2->source_file_name, "secondrequest", 12);

    list->push(list, r, 1);
    ASSERT_EQUALS_INT("should equal 1", 1, list->count);
    
    Request *r3 = (Request *) list->pop(list);
    ASSERT_EQUALS_STR("list string should equal", "mybestfriend", r3->source_file_name, 12);
    ASSERT_EQUALS_INT("should equal 0", 0, list->count);

    list->push(list, r2, 1);
    list->push(list, r, 2);
    ASSERT_EQUALS_INT("should equal 2", 2, list->count);

    //test removals
    Request *r4 = list->remove(list, 1, NULL, NULL);
    ASSERT_EQUALS_INT("request id should equal 1", 1, list->count);
    ASSERT_EQUALS_STR("request source file_name should equal", "secondrequest", r4->source_file_name, 12);

    list->push(list, r4, 0);
    list->iterate(list, print_list, NULL);
    list->free(list, ssp_cleanup_req);
    

    test_remove_node();

    return 0;
}