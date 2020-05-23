/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

//Have to include these files
#include <libgen.h>
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
#include "posix_server_provider.h"
//for ssp_thread_join, can use p_thread join on linux
#include "port.h"
#include "tasks.h"

//exit handler variable for the main thread
static int *exit_now;

/*------------------------------------------------------------------------------
    Purpose: This struct if our configuration for this program, these elements
    are set with getopt
------------------------------------------------------------------------------*/
typedef struct config
{
    unsigned int timer;
    uint32_t client_cfdp_id;
    uint32_t my_cfdp_id;
    uint8_t verbose_level;
} Config;


static Config *configuration(int argc, char **argv)
{
    int ch;
    Config *conf = calloc(sizeof(Config), 1);
    if (conf == NULL)
        return NULL;
        

    conf->timer = 15;
    conf->verbose_level = 0;
    conf->client_cfdp_id = 0;
    conf->my_cfdp_id = 0;

    uint32_t tmp;
    while ((ch = getopt(argc, argv, "t: i: c: v: h")) != -1)
    {
        switch (ch)
        {
        case 't':
            tmp = strtol(optarg, NULL, 10);
            conf->timer = tmp;
            break;

        case 'i':
            tmp = strtol(optarg, NULL, 10);
            conf->my_cfdp_id = tmp;
            break;
        
        case 'v':
            tmp = strtol(optarg, NULL, 10);
            conf->verbose_level = (uint8_t) tmp;
            break;

        case 'c': 
            tmp = strtol(optarg, NULL, 10);
            conf->client_cfdp_id = tmp;
            break;

        case 'h':
            ssp_printf("\n-----------HELP MESSAGE------------\n");
            ssp_printf("\nusage: %s [options] \n\n", basename(argv[0]));
            ssp_printf("Options: %s%s%s%s\n",
                    "-t timeout\n",
                    "-i my cfdp id for server\n",
                    "-c client id\n",
                    "-v verbose level (1-3)"
                    "-h HelpMessage");

            ssp_printf("Default port number mis 1111\n");
            ssp_printf("\n---------------END----------------\n");
            break;
        default:
            ssp_printf("\ngot something not found using default\n");
            break;
        }
    }
    return conf;
}

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
    if (app == NULL) {
        return 1;
    }
    
    //create a client
    if (conf->client_cfdp_id != 0){

        start_request(put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/noProxy.jpg", ACKNOWLEDGED_MODE, app));
        //start_request(put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/noProxy2.jpg", UN_ACKNOWLEDGED_MODE, app));
        //start_request(put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/noProxy3.jpg", UN_ACKNOWLEDGED_MODE, app));
        Request *req = put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/tcp.jpg", ACKNOWLEDGED_MODE, app);
        start_request(req);

    }

    ssp_thread_join(app->server_handle);
    free(conf); 

    
    return 0;
}

