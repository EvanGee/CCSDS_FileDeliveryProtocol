#include "unit_tests.h"
#include "test.h"
#include "requests.h"
#include "list.h"


void check_request_callback(Node *node, void *request, void *args){
    Request *r = (Request *) request;
    printf("id's remaining in list: %d\n", r->dest_cfdp_id);

}

void test_remove_request() {
    

    List *l = populate_request_list();
    Request *request = (Request *) l->find(l, 3, NULL, NULL);
    request->procedure = clean_up;
    //run a task here, or a removal function

    l->iterate(l, check_request_callback, NULL);
    l->free(l, ssp_cleanup_req);
}



int tasks_tests() {
    DECLARE_NEW_TEST("Tasks"); 

    test_remove_request();

    int error = 0;
    return error;
}