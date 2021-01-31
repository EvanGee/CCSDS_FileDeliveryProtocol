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

#include "port.h"
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

#include "tasks.h"
#include "mib.h"

#ifdef CSP_NETWORK
    #include <csp/csp.h>
    #include <csp/drivers/usart.h>
#endif

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
    uint32_t baudrate;
    char *uart_device;
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
    conf->baudrate = 9600;
    conf->uart_device = NULL;
    

    uint32_t tmp;
    while ((ch = getopt(argc, argv, "t: i: c: v: k: h ")) != -1)
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

        case 'k':
            conf->uart_device = optarg;
            break;

        case 'b':
            tmp = strtol(optarg, NULL, 10);
            conf->baudrate = tmp;
            break;

        case 'h':
            ssp_printf("\n-----------HELP MESSAGE------------\n");
            ssp_printf("\nusage: %s [options] \n\n", basename(argv[0]));
            ssp_printf("Options: %s%s%s%s\n",
                    "-t <timeout>\n",
                    "-i <my cfdp id for server>\n",
                    "-c <client id>\n",
                    "-v <verbose level> eg (1-3)"
                    "-k <uart-device> eg /dev/ttyUSB0\n"
                    "-b <baudrate> default is 9600"
                    "-h HelpMessage");

            ssp_printf("Default port number is 1111\n");
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
    prepareSignalHandler();

    //get-opt configuration
    Config *conf = configuration(argc, argv);
    if (conf->my_cfdp_id == 0){
        printf("can't start server, please select an ID (-i #) and client ID (-c #) \n");
        return 1;
    }
    

    #ifdef CSP_NETWORK

        csp_debug_level_t debug_level = CSP_INFO;
        // enable/disable debug levels
        for (csp_debug_level_t i = 0; i <= CSP_LOCK; ++i) {
            csp_debug_set_level(i, (i <= debug_level) ? true : false);
        }

        Remote_entity remote_entity;
        int error = get_remote_entity_from_json(&remote_entity, conf->my_cfdp_id);
        if (error < 0) {
            ssp_error("couldn't get client remote_entity from mib\n");
            return 1;
        }

        csp_conf_t csp_conf;
        csp_conf_get_defaults(&csp_conf);        
        csp_conf.address = remote_entity.UT_address;

        error = csp_init(&csp_conf);
        if (error != CSP_ERR_NONE) {
            csp_log_error("csp_init() failed, error: %d", error);
            exit(1);
        }

        // Start router task with 10000 bytes of stack (priority is only supported on FreeRTOS) 
        csp_route_start_task(500, 0);

        // Add interface(s) 
        csp_iface_t * default_iface = NULL;
        if (conf->uart_device != NULL) {
            csp_usart_conf_t uart_conf = {.device = conf->uart_device,
                            .baudrate = conf->baudrate, // supported on all platforms 
                            .databits = 8,
                            .stopbits = 2,
                            .paritysetting = 0,
                            .checkparity = 0};
            error = csp_usart_open_and_add_kiss_interface(&uart_conf, CSP_IF_KISS_DEFAULT_NAME,  &default_iface);
            if (error != CSP_ERR_NONE) {
                ssp_printf("failed to add KISS interface, error: %d", error);
                exit(1);
            }
        }

        //printf("Connection table\r\n");
        //csp_conn_print_table();

        printf("Interfaces\r\n");
        csp_route_print_interfaces();

        //printf("Route table\r\n");
        //csp_route_print_table();
    #endif



    FTP app;
    //init_ftp(conf->my_cfdp_id, &app);
    uint32_t id = conf->my_cfdp_id;
    void *handler = create_ftp_task(id, &app);
    if (handler == NULL) {
        return 1;
    }
    
    uint32_t client_id = conf->client_cfdp_id;
    Request *req = put_request(client_id, "pictures/pic.jpeg", "pictures/udp.jpg", ACKNOWLEDGED_MODE, &app);

/*
    if (conf->client_cfdp_id == 1) {
        schedule_put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/scheduled5.jpg", ACKNOWLEDGED_MODE, app);
        schedule_put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/scheduled6.jpg", ACKNOWLEDGED_MODE, app);
        schedule_put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/scheduled7.jpg", UN_ACKNOWLEDGED_MODE, app);
        schedule_put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/scheduled8.jpg", UN_ACKNOWLEDGED_MODE, app);
        start_scheduled_requests(conf->client_cfdp_id, app);
    }
    else if (conf->client_cfdp_id == 2) {
        Request *req = init_request_no_client();
        put_request_no_client(req, "pictures/pic.jpeg", "pictures/scheduled.jpg", ACKNOWLEDGED_MODE, app);

        Client *client = start_client(app, conf->client_cfdp_id);
        if (client == NULL) {
            ssp_printf("client failed to start\n");
        } else 
            add_request_to_client(req, client);
    } else if (conf->client_cfdp_id == 3) {
        Request *req = put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/udp.jpg", ACKNOWLEDGED_MODE, app);
        start_request(req);
        //Request *req = put_request(conf->client_cfdp_id, NULL, NULL, ACKNOWLEDGED_MODE, app);
        //add_cont_partial_message_to_request(4, 1, conf->client_cfdp_id, 1, 0, 1, req);
        //start_request(req);

    }

*/
    //Request *req = put_request(conf->my_cfdp_id, "pictures/pic.jpeg", "pictures/noProxy2.jpg", ACKNOWLEDGED_MODE, app);
    //start_request(req);
    
    /*
    if (conf->my_cfdp_id == 1) {
        Request *re
    if (conf->client_cfdp_id == 3) {
          }q = put_request(conf->client_cfdp_id, "pictures/videoplayback.mp4", "pictures/vid_tcp.jpg", ACKNOWLEDGED_MODE, app);
        start_request(req);
    }
    if (conf->client_cfdp_id == 4) {
        //Request *req = put_request(conf->client_cfdp_id, NULL, NULL, UN_ACKNOWLEDGED_MODE, app);
        //add_cont_partial_message_to_request(conf->my_cfdp_id, 3, 1, 4, 1, 1, req);
        //add_proxy_message_to_request(7, 1, "pictures/pic.jpeg","pictures/proxy_yo!.jpeg", req);

        Request *req = put_request(conf->client_cfdp_id, "pictures/pic.jpeg", "pictures/noProxy2.jpg", UN_ACKNOWLEDGED_MODE, app);
        //add_proxy_message_to_request(7, 1, "pictures/pic.jpeg","pictures/proxy_yo!.jpeg", req);
        start_request(req);
    }
    */

    free(conf); 
    ssp_thread_join(handler);

    //ssp_thread_join(app2->server_handle);
  
    return 0;
}
