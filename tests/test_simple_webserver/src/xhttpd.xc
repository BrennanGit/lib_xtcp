// Copyright (c) 2011-2016, XMOS Ltd, All rights reserved

#include <xs1.h>
#include <print.h>
#include "httpd.h"
#include "xtcp.h"

// The main webserver thread
void xhttpd(chanend tcp_svr)
{
  xtcp_connection_t conn;
  printstrln("**WELCOME TO THE SIMPLE WEBSERVER DEMO**");
  // Initiate the HTTP state
  httpd_init(tcp_svr);

  // Loop forever processing TCP events
  while(1)
    {
      select
        {
        case xtcp_event(tcp_svr, conn):
          httpd_handle_event(tcp_svr, conn);
          break;
        }

    }
}

