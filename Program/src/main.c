
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

//Have to include these files

//for types
#include "protocol_handler.h"
//for conf
#include "utils.h"
//for put request
#include "requests.h"

#include "types.h"
//for main app
#include "file_delivery_app.h"
//for signal handler, because its nice
#include "server.h"
//for ssp_thread_join, can use p_thread join on linux
#include "port.h"
#include "tasks.h"

//exit handler variable for the main thread
static int *exit_now;

int main(int argc, char** argv) {

    //exit handler for the main thread;
    exit_now = prepareSignalHandler();

    //get-opt configuration
    Config *conf = configuration(argc, argv);

    if (conf->my_cfdp_id == 0){
        printf("can't start server, please select an ID (-i #) and client ID (-c #) \n");
        return 1;
    }

    FTP *app = init_ftp(conf->my_cfdp_id);
    
    //create a client
    if (conf->client_cfdp_id != 0){

        
        /*
        start_request(put_request(conf->client_cfdp_id, "pic.jpeg", "noProxy.jpg", ACKNOWLEDGED_MODE, app));
        start_request(put_request(conf->client_cfdp_id, "pic.jpeg", "noProxy2.jpg", ACKNOWLEDGED_MODE, app));
        start_request(put_request(conf->client_cfdp_id, "pic.jpeg", "noProxy3.jpg", ACKNOWLEDGED_MODE, app));
        */
        //start_request(put_request(conf->client_cfdp_id, "pic.jpeg", "noProxy4.jpg", ACKNOWLEDGED_MODE, app));

        
        Request *req = put_request(conf->client_cfdp_id, NULL, NULL, ACKNOWLEDGED_MODE, app);
        add_proxy_message_to_request(2, 1, "pic.jpeg", "proxy.jpg", req);
        start_request(req);
    
        req = put_request(conf->client_cfdp_id, NULL, NULL, ACKNOWLEDGED_MODE, app);
        add_proxy_message_to_request(2, 1, "pic.jpeg", "proxy2.jpg", req);
        start_request(req);


        req = put_request(conf->client_cfdp_id, NULL, NULL, ACKNOWLEDGED_MODE, app);
        add_proxy_message_to_request(2, 1, "pic.jpeg", "proxy3.jpg", req);
        start_request(req);
/*
        req = put_request(conf->client_cfdp_id, NULL, NULL, ACKNOWLEDGED_MODE, app);
        add_proxy_message_to_request(2, 1, "pic.jpeg", "proxy4.jpg", req);
        start_request(req);

        req = put_request(conf->client_cfdp_id, NULL, NULL, ACKNOWLEDGED_MODE, app);
        add_proxy_message_to_request(2, 1, "pic.jpeg", "proxy5.jpg", req);
        start_request(req);
*/

        //Request *req;
        for (int i=0; i < 10; i++) {
            sleep(10);
            req = put_request(conf->client_cfdp_id, NULL, NULL, ACKNOWLEDGED_MODE, app);
            char filename[11] = {'p','r','o','x','y', (char) (48+i), '.', 'j', 'p', 'g', '\0'};


            add_proxy_message_to_request(2, 1, "pic.jpeg", filename, req);
            start_request(req);

        }
     

    }

    ssp_thread_join(app->server_handle);
    free(conf); 

    
    return 0;
}

