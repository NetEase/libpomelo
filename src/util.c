//
//  pomelo
//  util.c
//  
//
//  
//
//

#include <stdio.h>
#include "pomelo.h"

/**
 * set customized user data, can be used in callback (especially in C++)
 *
 * @param client client instance.
 * @param slot   which slot this user data goes in.
 * @param data   user data set by user.
 *
 */
void pc_set_user_data(pc_client_t *client, unsigned int slot, void *data)
{
    if (slot >= USER_DATA_COUNT) {
        return;
    }
    
    client->userData[slot] = data;
}


/**
 * get customized user data, can be used in callback (especially in C++)
 *
 * @param client client instance.
 * @param slot   which slot this user data goes in.
 *
 */
void* pc_get_user_data(pc_client_t *client, unsigned int slot)
{
    if (slot >= USER_DATA_COUNT) {
        return NULL;
    }
    return client->userData[slot];
}



/**
 * get client instance from request instance.
 *
 * @param req request instance.
 *
 */
pc_client_t* pc_get_client(pc_request_t* req)
{
    return req->client;
}