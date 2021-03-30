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

#include "app_control.h"
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
    bool unackowledged_mode;
} Config;


static Config *configuration(int argc, char **argv)
{
    int ch;
    Config *conf = calloc(sizeof(Config), 1);
    if (conf == NULL)
        return NULL;
        

    conf->timer = 15;
    conf->verbose_level = 0;
    conf->client_cfdp_id = -1;
    conf->my_cfdp_id = 0;
    conf->baudrate = 9600;
    conf->uart_device = NULL;
    conf->unackowledged_mode = 0;

    uint32_t tmp;
    while ((ch = getopt(argc, argv, "t: i: c: v: k: hu ")) != -1)
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
        case 'u':
            conf->unackowledged_mode = true;
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
                    "-u un_ackowledged mode\n"
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
        csp_conf.buffers = 4096; 
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

        csp_rtable_set(CSP_DEFAULT_ROUTE, 0, default_iface, CSP_NO_VIA_ADDRESS);
        //printf("Connection table\r\n");
        //csp_conn_print_table();

        printf("Interfaces\r\n");
        csp_route_print_interfaces();

        printf("Route table\r\n");
        csp_route_print_table();
    #endif

    FTP app;

    uint32_t id = conf->my_cfdp_id;
    void *handler = create_ftp_task(id, &app);
    if (handler == NULL) {
        return 1;
    }

    int buff_len = 25000;
    char input[buff_len];
    memset(input, 0, buff_len);

    uint32_t client_id = conf->client_cfdp_id;
    
    for (;;) {
        
        fgets(input, buff_len, stdin);
        input[strlen(input)-1]='\0';
        
        printf("%s\n", input);

        if (get_exit())
            break;

        if (strncmp(input, "exit", 5) == 0) {
            set_exit();
            break;
        }
    }
    
    /*
    if (client_id != -1) {

        //sleep(5);
        //put_request(client_id, "udp.jpeg", "test-received.jpg", conf->unackowledged_mode, &app);
        //put_request(client_id, "udp.jpeg", "udp.jpeg", conf->unackowledged_mode, &app);
        
        //put_request(client_id, "mib/peer_1.json", "mib/peer_test.json", ACKNOWLEDGED_MODE, &app);
        //get_request(client_id, "mib/peer_0.json", "GET_REQUEST.json", ACKNOWLEDGED_MODE, &app);

    }
    */

    free(conf); 
    ssp_thread_join(handler);
  
    return 0;
}
