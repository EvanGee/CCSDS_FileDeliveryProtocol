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
#include "utils.h"
#include "app_control.h"
#include "mib.h"
#include "list.h"

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
    List *file_list;

} Config;

typedef enum request_type {
    REQUEST_GET,
    REQUEST_PUT
} Request_type;


typedef struct file_path_pair
{
   char dest_name[255];
   char src_name[255]; 
   Request_type type;

} File_path_pair;



//sets file_name and returns the length of the file_name
static int get_file_name(char *buff, int len, char *file_name) {
    int i = 0;
    for (i = 0; i < len; i++) {
        if (buff[i] == ' ' || buff[i] == '\0') {
            file_name[i] = '\0';
            break;
        }
        file_name[i] = buff[i];
    }

    return i;
}

//parses the file name paths between source and destination filenames
static int parse_file_path(char *buff, int len, char *file_name) {
    int i = 0;
    for (i = 0; i < len; i++) {
        if (buff[i] == '|') {
            file_name[i] = '\0';
            break;
        }
        file_name[i] = buff[i];
    }
    return i;
}



static List *parse_file_list(char *target){

    int buff_size = 5000;
    int len = strnlen(target, buff_size);
    char string_name[buff_size];
    char source_name[255];
    char dest_name[255];

    List *list = linked_list();

    int i = 0;
    int j = 0;
    int path_len = 0;
    File_path_pair pair;
    memset(&pair, 0, sizeof(File_path_pair));

    while (i < len) {
        i += get_file_name(&target[i], len, string_name) + 1;
        if (strncmp(string_name, "PUT", 4) == 0) {
            
            pair.type = REQUEST_PUT;
            //ssp_printf("PUT REQUEST!!!!\n");
        } else if (strncmp(string_name, "GET", 4) == 0) {

            pair.type = REQUEST_GET;
            //ssp_printf("GET REQUEST!!!!\n");
        } else {
            
            //ssp_printf("file name: %s\n", string_name);
            j = 0;
            path_len = strnlen(string_name, 255);
            
            j += parse_file_path(&string_name[j], path_len, source_name) + 1;
            //printf("source_name %s %d\n",source_name, j);
            strncpy(pair.src_name, source_name, 255);

            j += parse_file_path(&string_name[j], path_len, dest_name);
            //printf("dest_name %s %d\n",dest_name, j);
            strncpy(pair.dest_name, dest_name, 255);
            
            File_path_pair *add = ssp_alloc(1, sizeof(File_path_pair));
            memcpy(add, &pair, sizeof(File_path_pair));

            list->push(list, add, -1);
            
        }
    }

    return list;
}


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
    conf->unackowledged_mode = ACKNOWLEDGED_MODE;
    
    uint32_t tmp;

    while ((ch = getopt(argc, argv, "f: i: c: v: k: hu")) != -1)
    {
        switch (ch)
        {
        case 'i':
            tmp = strtol(optarg, NULL, 10);
            conf->my_cfdp_id = tmp;
            break;

        case 'f':
            conf->file_list = parse_file_list(optarg);
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
            printf("\n-----------HELP MESSAGE------------\n");
            printf("\nusage: %s [options] \n\n", basename(argv[0]));
            printf("Options: %s%s%s%s\n",
                    "-i <my cfdp id for server>\n",
                    "-c <client id>\n",
                    "-f list of file names eg, \"PUT local/path|/path/on/sat GET /path/on/sat|local/path ...\"\n",
                    "-v <verbose level> eg (1-3)\n"
                    "-k <uart-device> eg /dev/ttyUSB0\n"
                    "-b <baudrate> default is 9600"
                    "-h HelpMessage");

            printf("Default port number is 1111\n");
            printf("\n---------------END----------------\n");
            break;

        case ':':
            printf("missing argument\n");
            break;

        default:
            printf("\ngot something not found using default\n");
            break;
        }
    }
    return conf;
}


static int init_csp_stuff(Config conf){

    #ifdef CSP_NETWORK
        
        // enable/disable debug levels
        /*
        for (csp_debug_level_t i = 0; i <= CSP_LOCK; ++i) {
            csp_debug_set_level(i, (i <= debug_level) ? true : false);
        }
        */
        
       
        Remote_entity remote_entity;
        int error = get_remote_entity_from_json(&remote_entity, conf.my_cfdp_id);
        if (error < 0) {
            ssp_error("couldn't get client remote_entity from mib\n");
            return 1;
        }

        csp_conf_t csp_conf;
        csp_conf_get_defaults(&csp_conf);       
        csp_conf.buffers = 4096; 
        csp_conf.address = remote_entity.UT_address;
        csp_conf.buffer_data_size = 250;
    
        error = csp_init(&csp_conf);
        if (error != CSP_ERR_NONE) {
            csp_log_error("csp_init() failed, error: %d", error);
            exit(1);
        }
        /*
        for (int i = 0; i <= CSP_LOCK; ++i) {
            csp_debug_set_level(i, true);
        }
        */

        // Start router task with 10000 bytes of stack (priority is only supported on FreeRTOS) 
        csp_route_start_task(500, 0);

        // Add interface(s) 
        csp_iface_t * default_iface = NULL;
        if (conf.uart_device != NULL) {
            csp_usart_conf_t uart_conf = {.device = conf.uart_device,
                            .baudrate = conf.baudrate, // supported on all platforms 
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

        //printf("Interfaces\r\n");
        //csp_route_print_interfaces();

        //printf("Route table\r\n");
        //csp_route_print_table();
    #endif
    return 0;
}

#if 0
static int confirm(){
    char buff[100];
    memset(buff, 0, 100);
    fgets(buff, 100, stdin);
    if ((buff[0] == 'y' || buff[0] == 'Y') && buff[1] == '\0') {
        return 1;    
    } 
    else if ((buff[0] == 'n' || buff[0] == 'N') && buff[1] == '\0') {
        return 0;

    } else {
        return -1;
    }
}


static void input_daemon(uint32_t client_id, FTP *app){

    int buff_len = 25000;
    char input[buff_len];
    memset(input, 0, buff_len);


    char src_file[MAX_PATH];
    char dest_file[MAX_PATH];

    for (;;) {
            
        printf("send a file? type 'PUT <source_file> <destination_file>' or 'GET <destination_file> <source_file>'\n");
        memset(src_file, 0 , MAX_PATH);
        memset(dest_file, 0 , MAX_PATH);
        
        fgets(input, buff_len, stdin);
        input[strlen(input)-1]='\0';
        

        if (get_exit()) {
            break;
        }
        else if (strncmp(input, "exit", 5) == 0) {
            set_exit();
            break;
        }

        if (strncmp(input, "PUT ", 4) == 0) {
            
            int len = get_file_name(&input[4], buff_len, src_file);
            len = get_file_name(&input[len + 5], buff_len, dest_file);
                        
            while(1) {
                printf("put source_file:%s destination_file:%s? (y/n): ", src_file, dest_file);
                int confirming = confirm();
                if (confirming) {
                    start_request(put_request(client_id, src_file, dest_file, ACKNOWLEDGED_MODE, app));
                    break;
                } else if (confirming == 0) {
                    break;
                } else {
                    printf("please type either 'Y' or 'N'");
                }
            }

            break;

        } else if (strncmp(input, "GET ", 4) == 0) {

            int len = get_file_name(&input[4], buff_len, dest_file);
            len = get_file_name(&input[len + 5], buff_len, src_file);
            while(1) {
                printf("get destination_file:%s source_file:%s?(y/n)\n", src_file, dest_file);
                int confirming = confirm();
                if (confirming) {
                    start_request(get_request(client_id, src_file, dest_file, ACKNOWLEDGED_MODE, app));
                    break;
                } else if (confirming == 0) {
                    break;
                } else {
                    printf("please type either 'Y' or 'N'");
                }
            }
            break;
        }
    }
}
#endif

int main(int argc, char** argv) {


    //exit handler for the main thread;
    prepareSignalHandler();

    //get-opt configuration
    Config *conf = configuration(argc, argv);

    
    if (conf->my_cfdp_id == 0){
        printf("can't start server, please select an ID (-i #) and client ID (-c #) \n");
        return 1;
    }
    
    init_csp_stuff(*conf);

    FTP *app = ssp_alloc(1, sizeof(FTP));

    uint32_t id = conf->my_cfdp_id;
    void *handler = create_ftp_task(id, app);
    if (handler == NULL) {
        return 1;
    }

    while(app->my_cfdp_id != id);
    sleep(1);

    uint32_t client_id = conf->client_cfdp_id;
    if (client_id != -1) {
        while(conf->file_list->count > 0) {
            File_path_pair *file_names = conf->file_list->pop(conf->file_list);

            if (file_names->type == REQUEST_PUT) {
                ssp_printf("putting file %s path %s\n", file_names->src_name, file_names->dest_name);
                start_request(put_request(client_id, file_names->src_name, file_names->dest_name, conf->unackowledged_mode, app));
            }
            else if (file_names->type == REQUEST_GET) {
                ssp_printf("getting file %s to path %s\n", file_names->src_name, file_names->dest_name);

                start_request(get_request(client_id, file_names->src_name, file_names->dest_name, conf->unackowledged_mode, app));
            }
            free(file_names);
        }

        while (app->active_clients->count != 0) {
            sleep(1);
        }

        ssp_printf("closing app\n");
        app->close = 1;
    }
    if (conf->file_list != NULL)
        conf->file_list->freeOnlyList(conf->file_list);

    free(conf); 
    ssp_thread_join(handler);
    
    return 0;
}
