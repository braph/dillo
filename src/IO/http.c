/*
 * File: http.c
 *
 * Copyright (C) 2000, 2001 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * HTTP connect functions
 */


#include <config.h>

#include <unistd.h>
#include <errno.h>              /* for errno */
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>         /* for lots of socket stuff */
#include <netinet/in.h>         /* for ntohl and stuff */
#include <arpa/inet.h>          /* for inet_ntop */

#include "IO.h"
#include "Url.h"
#include "../msg.h"
#include "../klist.h"
#include "../dns.h"
#include "../cache.h"
#include "../web.hh"
#include "../cookies.h"
#include "../prefs.h"
#include "../misc.h"

#include "../uicmd.hh"

/* Used to send a message to the bw's status bar */
#define MSG_BW(web, root, ...)                                        \
D_STMT_START {                                                        \
   if (a_Web_valid((web)) && (!(root) || (web)->flags & WEB_RootUrl)) \
      a_UIcmd_set_msg((web)->bw, __VA_ARGS__);                        \
} D_STMT_END

#define _MSG_BW(web, root, ...)

#define DEBUG_LEVEL 5
#include "../debug.h"



/* 'Url' and 'web' are just references (no need to deallocate them here). */
typedef struct {
   int SockFD;
   const DilloUrl *Url;    /* reference to original URL */
   uint_t port;            /* need a separate port in order to support PROXY */
   bool_t use_proxy;       /* indicates whether to use proxy or not */
   DilloWeb *web;          /* reference to client's web structure */
   Dlist *addr_list;       /* Holds the DNS answer */
   int Err;                /* Holds the errno of the connect() call */
   ChainLink *Info;        /* Used for CCC asynchronous operations */
} SocketData_t;


/*
 * Local data
 */
static Klist_t *ValidSocks = NULL; /* Active sockets list. It holds pointers to
                                    * SocketData_t structures. */

static DilloUrl *HTTP_Proxy = NULL;
static char *HTTP_Proxy_Auth_base64 = NULL;

/*
 * Initialize proxy vars.
 */
int a_Http_init(void)
{
   char *env_proxy = getenv("http_proxy");

   if (env_proxy && strlen(env_proxy))
      HTTP_Proxy = a_Url_new(env_proxy, NULL, 0, 0, 0);
   if (!HTTP_Proxy && prefs.http_proxy)
      HTTP_Proxy = a_Url_dup(prefs.http_proxy);

/*  This allows for storing the proxy password in "user:passwd" format
 * in dillorc, but as this constitutes a security problem, it was disabled.
 *
   if (HTTP_Proxy && prefs.http_proxyuser && strchr(prefs.http_proxyuser, ':'))
      HTTP_Proxy_Auth_base64 = a_Misc_encode_base64(prefs.http_proxyuser);
 */
   return 0;
}

/*
 * Tell whether the proxy auth is already set (user:password)
 * Return: 1 Yes, 0 No
 */
int a_Http_proxy_auth(void)
{
   return (HTTP_Proxy_Auth_base64 ? 1 : 0);
}

/*
 * Activate entered proxy password for HTTP.
 */
void a_Http_set_proxy_passwd(char *str)
{
   char *http_proxyauth = dStrconcat(prefs.http_proxyuser, ":", str, NULL);
   HTTP_Proxy_Auth_base64 = a_Misc_encode_base64(http_proxyauth);
   dFree(http_proxyauth);
}

/*
 * Create and init a new SocketData_t struct, insert into ValidSocks,
 * and return a primary key for it.
 */
static int Http_sock_new(void)
{
   SocketData_t *S = dNew0(SocketData_t, 1);
   return a_Klist_insert(&ValidSocks, S);
}

/*
 * Free SocketData_t struct
 */
static void Http_socket_free(int SKey)
{
   SocketData_t *S;

   if ((S = a_Klist_get_data(ValidSocks, SKey))) {
      a_Klist_remove(ValidSocks, SKey);
      dFree(S);
   }
}

/*
 * Close the socket's FD
 */
static void Http_socket_close(SocketData_t *S)
{
   int st;
   do
      st = close(S->SockFD);
   while (st < 0 && errno == EINTR);
}

/*
 * Make the http query string
 */
char *a_Http_make_query_str(const DilloUrl *url, bool_t use_proxy)
{
   char *str, *ptr, *cookies;
   Dstr *s_port     = dStr_new(""),
        *query      = dStr_new(""),
        *full_path  = dStr_new(""),
        *proxy_auth = dStr_new("");

   /* Sending the default port in the query may cause a 302-answer.  --Jcid */
   if (URL_PORT(url) && URL_PORT(url) != DILLO_URL_HTTP_PORT)
      dStr_sprintfa(s_port, ":%d", URL_PORT(url));

   if (use_proxy) {
      dStr_sprintfa(full_path, "%s%s",
                    URL_STR(url),
                    (URL_PATH_(url) || URL_QUERY_(url)) ? "" : "/");
      if ((ptr = strrchr(full_path->str, '#')))
         dStr_truncate(full_path, ptr - full_path->str);
      if (HTTP_Proxy_Auth_base64)
         dStr_sprintf(proxy_auth, "Proxy-Authorization: Basic %s\r\n",
                      HTTP_Proxy_Auth_base64);
   } else {
      dStr_sprintfa(full_path, "%s%s%s%s",
                    URL_PATH(url),
                    URL_QUERY_(url) ? "?" : "",
                    URL_QUERY(url),
                    (URL_PATH_(url) || URL_QUERY_(url)) ? "" : "/");
   }

   cookies = a_Cookies_get(url);
   if (URL_FLAGS(url) & URL_Post) {
      dStr_sprintfa(
         query,
         "POST %s HTTP/1.0\r\n"
         "Accept-Charset: utf-8, iso-8859-1\r\n"
         "Host: %s%s\r\n"
         "%s"
         "User-Agent: Dillo/%s\r\n"
         "Cookie2: $Version=\"1\"\r\n"
         "%s"
         "Content-type: application/x-www-form-urlencoded\r\n"
         "Content-length: %ld\r\n"
         "\r\n"
         "%s",
         full_path->str, URL_HOST(url), s_port->str,
         proxy_auth->str, VERSION, cookies,
         (long)strlen(URL_DATA(url)),
         URL_DATA(url));

   } else {
      dStr_sprintfa(
         query,
         "GET %s HTTP/1.0\r\n"
         "%s"
         "Accept-Charset: utf-8, iso-8859-1\r\n"
         "Host: %s%s\r\n"
         "%s"
         "User-Agent: Dillo/%s\r\n"
         "Cookie2: $Version=\"1\"\r\n"
         "%s"
         "\r\n",
         full_path->str,
         (URL_FLAGS(url) & URL_E2EReload) ?
            "Cache-Control: no-cache\r\nPragma: no-cache\r\n" : "",
         URL_HOST(url), s_port->str,
         proxy_auth->str,
         VERSION,
         cookies);
   }
   dFree(cookies);

   str = query->str;
   dStr_free(query, FALSE);
   dStr_free(s_port, TRUE);
   dStr_free(full_path, TRUE);
   dStr_free(proxy_auth, TRUE);
   DEBUG_MSG(4, "Query:\n%s", str);
   return str;
}

/*
 * Create and submit the HTTP query to the IO engine
 */
static void Http_send_query(ChainLink *Info, SocketData_t *S)
{
   char *query;
   DataBuf *dbuf;

   /* Create the query */
   query = a_Http_make_query_str(S->Url, S->use_proxy);
   dbuf = a_Chain_dbuf_new(query, (int)strlen(query), 0);

   /* actually this message is sent too early.
    * It should go when the socket is ready for writing (i.e. connected) */
   _MSG_BW(S->web, 1, "Sending query to %s...", URL_HOST_(S->Url));

   /* send query */
   a_Chain_link_new(Info, a_Http_ccc, BCK, a_IO_ccc, 1, 1);
   a_Chain_bcb(OpStart, Info, &S->SockFD, NULL);
   a_Chain_bcb(OpSend, Info, dbuf, NULL);
   dFree(dbuf);
   dFree(query);

   /* Tell the cache to start the receiving CCC for the answer */
   a_Chain_fcb(OpSend, Info, &S->SockFD, "SockFD");
}

/*
 * This function gets called after the DNS succeeds solving a hostname.
 * Task: Finish socket setup and start connecting the socket.
 * Return value: 0 on success;  -1 on error.
 */
static int Http_connect_socket(ChainLink *Info)
{
   int status;
#ifdef ENABLE_IPV6
   struct sockaddr_in6 name;
#else
   struct sockaddr_in name;
#endif
   SocketData_t *S;
   DilloHost *dh;
   socklen_t socket_len = 0;

   S = a_Klist_get_data(ValidSocks, VOIDP2INT(Info->LocalKey));

   /* TODO: iterate this address list until success, or end-of-list */
   dh = dList_nth_data(S->addr_list, 0);

   if ((S->SockFD = socket(dh->af, SOCK_STREAM, IPPROTO_TCP)) < 0) {
      S->Err = errno;
      DEBUG_MSG(5, "Http_connect_socket ERROR: %s\n", dStrerror(errno));
      return -1;
   }
   /* set NONBLOCKING and close on exec. */
   fcntl(S->SockFD, F_SETFL, O_NONBLOCK | fcntl(S->SockFD, F_GETFL));
   fcntl(S->SockFD, F_SETFD, FD_CLOEXEC | fcntl(S->SockFD, F_GETFD));

   /* Some OSes require this...  */
   memset(&name, 0, sizeof(name));
   /* Set remaining parms. */
   switch (dh->af) {
   case AF_INET:
   {
      struct sockaddr_in *sin = (struct sockaddr_in *)&name;
      socket_len = sizeof(struct sockaddr_in);
      sin->sin_family = dh->af;
      sin->sin_port = S->port ? htons(S->port) : htons(DILLO_URL_HTTP_PORT);
      memcpy(&sin->sin_addr, dh->data, (size_t)dh->alen);
      if (a_Web_valid(S->web) && (S->web->flags & WEB_RootUrl))
         DEBUG_MSG(5, "Connecting to %s\n", inet_ntoa(sin->sin_addr));
      break;
   }
#ifdef ENABLE_IPV6
   case AF_INET6:
   {
      char buf[128];
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&name;
      socket_len = sizeof(struct sockaddr_in6);
      sin6->sin6_family = dh->af;
      sin6->sin6_port = S->port ? htons(S->port) : htons(DILLO_URL_HTTP_PORT);
      memcpy(&sin6->sin6_addr, dh->data, dh->alen);
      inet_ntop(dh->af, dh->data, buf, sizeof(buf));
      if (a_Web_valid(S->web) && (S->web->flags & WEB_RootUrl))
         DEBUG_MSG(5, "Connecting to %s\n", buf);
      break;
   }
#endif
   }

   MSG_BW(S->web, 1, "Contacting host...");
   status = connect(S->SockFD, (struct sockaddr *)&name, socket_len);
   if (status == -1 && errno != EINPROGRESS) {
      S->Err = errno;
      Http_socket_close(S);
      DEBUG_MSG(5, "Http_connect_socket ERROR: %s\n", dStrerror(S->Err));
      return -1;
   } else {
      Http_send_query(S->Info, S);
   }

   return 0; /* Success */
}

/*
 * Test proxy settings and check the no_proxy domains list
 * Return value: whether to use proxy or not.
 */
static int Http_must_use_proxy(const DilloUrl *url)
{
   char *np, *p, *tok;
   int ret = 0;

   if (HTTP_Proxy) {
      ret = 1;
      if (prefs.no_proxy) {
         np = dStrdup(prefs.no_proxy);
         for (p = np; (tok = dStrsep(&p, " "));  ) {
            if (dStristr(URL_AUTHORITY(url), tok)) {
               ret = 0;
               break;
            }
         }
         dFree(np);
      }
   }
   return ret;
}

/*
 * Callback function for the DNS resolver.
 * Continue connecting the socket, or abort upon error condition.
 */
void a_Http_dns_cb(int Status, Dlist *addr_list, void *data)
{
   int SKey = VOIDP2INT(data);
   SocketData_t *S;

   S = a_Klist_get_data(ValidSocks, SKey);
   if (S) {
      if (Status == 0 && addr_list) {
         /* Successful DNS answer; save the IP */
         S->addr_list = addr_list;
         /* start connecting the socket */
         if (Http_connect_socket(S->Info) < 0) {
            MSG_BW(S->web, 1, "ERROR: %s", dStrerror(S->Err));
            a_Chain_fcb(OpAbort, S->Info, NULL, NULL);
            dFree(S->Info);
            Http_socket_free(SKey);
         }

      } else {
         /* DNS wasn't able to resolve the hostname */
         MSG_BW(S->web, 0, "ERROR: Dns can't resolve %s",
            (S->use_proxy) ? URL_HOST_(HTTP_Proxy) : URL_HOST_(S->Url));
         a_Chain_fcb(OpAbort, S->Info, NULL, NULL);
         dFree(S->Info);
         Http_socket_free(SKey);
      }
   }
}

/*
 * Asynchronously create a new http connection for 'Url'
 * We'll set some socket parameters; the rest will be set later
 * when the IP is known.
 * ( Data1 = Web structure )
 * Return value: 0 on success, -1 otherwise
 */
static int Http_get(ChainLink *Info, void *Data1)
{
   SocketData_t *S;
   char *hostname;

   S = a_Klist_get_data(ValidSocks, VOIDP2INT(Info->LocalKey));
   /* Reference Web data */
   S->web = Data1;
   /* Reference URL data */
   S->Url = S->web->url;
   /* Reference Info data */
   S->Info = Info;

   /* Proxy support */
   if (Http_must_use_proxy(S->Url)) {
      hostname = dStrdup(URL_HOST(HTTP_Proxy));
      S->port = URL_PORT(HTTP_Proxy);
      S->use_proxy = TRUE;
   } else {
      hostname = dStrdup(URL_HOST(S->Url));
      S->port = URL_PORT(S->Url);
      S->use_proxy = FALSE;
   }

   /* Let the user know what we'll do */
   MSG_BW(S->web, 1, "DNS resolving %s", URL_HOST_(S->Url));

   /* Let the DNS engine resolve the hostname, and when done,
    * we'll try to connect the socket from the callback function */
   a_Dns_resolve(hostname, a_Http_dns_cb, Info->LocalKey);

   dFree(hostname);
   return 0;
}

/*
 * CCC function for the HTTP module
 */
void a_Http_ccc(int Op, int Branch, int Dir, ChainLink *Info,
                void *Data1, void *Data2)
{
   int SKey = VOIDP2INT(Info->LocalKey);

   a_Chain_debug_msg("a_Http_ccc", Op, Branch, Dir);

   if (Branch == 1) {
      if (Dir == BCK) {
         /* HTTP query branch */
         switch (Op) {
         case OpStart:
            /* ( Data1 = Web ) */
            SKey = Http_sock_new();
            Info->LocalKey = INT2VOIDP(SKey);
            Http_get(Info, Data1);
            break;
         case OpEnd:
            /* finished the HTTP query branch */
            a_Chain_bcb(OpEnd, Info, NULL, NULL);
            Http_socket_free(SKey);
            dFree(Info);
            break;
         case OpAbort:
            /* something bad happened... */
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            Http_socket_free(SKey);
            dFree(Info);
            break;
         }
      } else {  /* FWD */
         /* HTTP send-query status branch */
         switch (Op) {
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }
   }
}



/*
 * Deallocate memory used by http module
 * (Call this one at exit time)
 */
void a_Http_freeall(void)
{
   a_Klist_free(&ValidSocks);
   a_Url_free(HTTP_Proxy);
   dFree(HTTP_Proxy_Auth_base64);
}
