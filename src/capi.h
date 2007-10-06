#ifndef __CAPI_H__
#define __CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "cache.h"
#include "web.hh"

/*
 * Function prototypes
 */
void a_Capi_init(void);
int a_Capi_open_url(DilloWeb *web, CA_Callback_t Call, void *CbData);
int a_Capi_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize);
int a_Capi_dpi_send_cmd(DilloUrl *url, void *bw, char *cmd, char *server,
                         int flags);
void a_Capi_stop_client(int Key, int force);
void a_Capi_conn_abort_by_url(const DilloUrl *url);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAPI_H__ */

