/* @(#) $Id: ./src/rootcheck/check_rc_ports.c, 2011/09/08 dcid Exp $
 */

/* Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

 
#ifndef WIN32
 
#include "shared.h"
#include "rootcheck.h"

/* SunOS netstat */
#if defined(sun) || defined(__sun__)
#define NETSTAT "netstat -an -P %s | "\
                "grep \"[^0-9]%d \" > /dev/null 2>&1"

/*
#elif WIN32
#define NETSTAT "netstat -an -p %s | "\
                "find \":%d\""
*/

#elif defined(Linux)
#define NETSTAT_LIST "netstat -an | grep \"^%s\" | "\
                     "cut -d ':' -f 2 | cut -d ' ' -f 1"
#define NETSTAT "netstat -an | grep \"^%s\" | " \
                "grep \"[^0-9]%d \" > /dev/null 2>&1"                          
#endif

#ifndef NETSTAT
#define NETSTAT "netstat -an | grep \"^%s\" | " \
                "grep \"[^0-9]%d \" > /dev/null 2>&1"
#endif


int run_netstat(int proto, int port)
{
    int ret;
    char nt[OS_SIZE_1024 +1];

    if(proto == IPPROTO_TCP)
        snprintf(nt, OS_SIZE_1024, NETSTAT, "tcp", port);
    else if(proto == IPPROTO_UDP)
        snprintf(nt, OS_SIZE_1024, NETSTAT, "udp", port);
    else
    {
        merror("%s: Netstat error (wrong protocol)", ARGV0);
        return(0);
    }

    ret = system(nt);

    if(ret == 0)    
        return(1);

    else if(ret == 1)
    {
        return(0);
    }
    
    return(1);
}


int conn_port(int proto, int port)
{
    int rc = 0;
    int ossock;
    struct sockaddr_in server;

    if(proto == IPPROTO_UDP)
    {
        if((ossock = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0)
            return(0);
    }
    else if(proto == IPPROTO_TCP)
    {
        if((ossock = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
            return(0);
    }


    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons( port );
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    
    /* If we can't bind, it means the port is open */
    if(bind(ossock, (struct sockaddr *) &server, sizeof(server)) < 0)
    {
        rc = 1;
    }

    /* Setting if port is open or closed */
    if(proto == IPPROTO_TCP)
    {
        total_ports_tcp[port] = rc;
    }
    else
    {
        total_ports_udp[port] = rc;
    }
    
    close(ossock);  

    return(rc);  
}


void test_ports(int proto, int *_errors, int *_total)
{
    int i;

    for(i = 0; i<= 65535; i++)
    {
        (*_total)++;
        if(conn_port(proto, i))
        {
            /* Checking if we can find it using netstat, if not,
             * check again to see if the port is still being used.
             */
            if(run_netstat(proto, i))
            {
                continue;
                
                #ifdef OSSECHIDS
                sleep(2);
                #endif
            }

            /* If we are being run by the ossec hids, sleep here (no rush) */
            #ifdef OSSECHIDS
            sleep(2);
            #endif

            if(!run_netstat(proto, i) && conn_port(proto, i))
            {
                char op_msg[OS_SIZE_1024 +1];

                (*_errors)++;

                snprintf(op_msg, OS_SIZE_1024, "Port '%d'(%s) hidden. "
                        "Kernel-level rootkit or trojaned "
                        "version of netstat.", i, 
                        (proto == IPPROTO_UDP)? "udp" : "tcp");

                notify_rk(ALERT_ROOTKIT_FOUND, op_msg);
            }
        }

        if((*_errors) > 20)
        {
            char op_msg[OS_SIZE_1024 +1];
            snprintf(op_msg, OS_SIZE_1024, "Excessive number of '%s' ports "
                             "hidden. It maybe a false-positive or "
                             "something really bad is going on.",
                             (proto == IPPROTO_UDP)? "udp" : "tcp" );
            notify_rk(ALERT_SYSTEM_CRIT, op_msg);
            return;
        }
    }

}


/*  check_rc_ports: v0.1
 *  Check all ports
 */
void check_rc_ports()
{
    int _errors = 0;
    int _total = 0;

    int i = 0;

    while(i<=65535)
    {
        total_ports_tcp[i] = 0;
        total_ports_udp[i] = 0;
        i++;
    }
    
    /* Trsting TCP ports */ 
    test_ports(IPPROTO_TCP, &_errors, &_total);

    /* Testing UDP ports */
    test_ports(IPPROTO_UDP, &_errors, &_total);

    if(_errors == 0)
    {
        char op_msg[OS_SIZE_1024 +1];
        snprintf(op_msg,OS_SIZE_1024,"No kernel-level rootkit hiding any port."
                                   "\n      Netstat is acting correctly."
                                    " Analyzed %d ports.", _total);
        notify_rk(ALERT_OK, op_msg);
    }
    
    return;
}


#else
void check_rc_ports()
{
    return;
}
#endif


/* EOF */
