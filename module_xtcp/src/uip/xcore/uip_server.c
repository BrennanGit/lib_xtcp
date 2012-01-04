// Copyright (c) 2011, XMOS Ltd, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef UIP_USE_SINGLE_THREADED_ETHERNET

#include <print.h>
#include <xccompat.h>
#include <string.h>
#include "uip.h"
#include "uip_arp.h"
#include "uip-split.h"
#include "xcoredev.h"
#include "xtcp_server.h"
#include "timer.h"
#include "uip_server.h"
#include "ethernet_rx_client.h"
#include "ethernet_tx_client.h"
#include "uip_xtcp.h"
#include "autoip.h"
#include "igmp.h"

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])
#define TCPBUF ((struct uip_tcpip_hdr *)&uip_buf[UIP_LLH_LEN])

extern void uip_printip4(const uip_ipaddr_t ip4);
void uip_server_init(chanend xtcp[], int num_xtcp, xtcp_ipconfig_t* ipconfig, unsigned char mac_address[6]);

extern int uip_static_ip;
extern xtcp_ipconfig_t uip_static_ipconfig;

/* Make sure that the uip_buf is word aligned */
unsigned int uip_buf32[(UIP_BUFSIZE + 5) >> 2];
u8_t *uip_buf = (u8_t *) &uip_buf32[0];

static void send(chanend mac_tx) {
	if (TCPBUF->srcipaddr != 0) {
		uip_split_output(mac_tx);
		uip_len = 0;
	}
}

static int needs_poll(xtcpd_state_t *s)
{
  return (s->s.connect_request | s->s.send_request | s->s.abort_request | s->s.close_request);
}

static int uip_conn_needs_poll(struct uip_conn *uip_conn)
{
  xtcpd_state_t *s = (xtcpd_state_t *) &(uip_conn->appstate);
  return needs_poll(s);
}

static int uip_udp_conn_needs_poll(struct uip_udp_conn *uip_udp_conn)
{
  xtcpd_state_t *s = (xtcpd_state_t *) &(uip_udp_conn->appstate);
  return needs_poll(s);
}

void uip_server(chanend mac_rx, chanend mac_tx, chanend xtcp[], int num_xtcp,
		xtcp_ipconfig_t *ipconfig, chanend connect_status) {

	struct uip_timer periodic_timer, arp_timer, autoip_timer;
	unsigned char hwaddr[6];

	timer_set(&periodic_timer, CLOCK_SECOND / 10);
	timer_set(&autoip_timer, CLOCK_SECOND / 2);
	timer_set(&arp_timer, CLOCK_SECOND * 10);

	xcoredev_init(mac_rx, mac_tx);

	mac_get_macaddr(mac_tx, hwaddr);

	uip_server_init(xtcp, num_xtcp, ipconfig, hwaddr);

	// Main uIP service loop
	while (1)
	{
		xtcpd_service_clients(xtcp, num_xtcp);

		for (int i = 0; i < UIP_CONNS; i++) {
			if (uip_conn_needs_poll(&uip_conns[i])) {
				uip_poll_conn(&uip_conns[i]);
				if (uip_len > 0) {
					uip_arp_out( NULL);
					send(mac_tx);
				}
			}
		}

		for (int i = 0; i < UIP_UDP_CONNS; i++) {
			if (uip_udp_conn_needs_poll(&uip_udp_conns[i])) {
				uip_udp_periodic(i);
				if (uip_len > 0) {
					uip_arp_out(&uip_udp_conns[i]);
					send(mac_tx);
				}
			}
		}

		uip_xtcp_checkstate();
		uip_xtcp_checklink(connect_status);
		uip_len = xcoredev_read(mac_rx, UIP_CONF_BUFFER_SIZE);
		if (uip_len > 0) {
			if (BUF->type == htons(UIP_ETHTYPE_IP)) {
				uip_arp_ipin();
				uip_input();
				if (uip_len > 0) {
					if (uip_udpconnection())
						uip_arp_out( uip_udp_conn);
					else
						uip_arp_out( NULL);
					send(mac_tx);
				}
			} else if (BUF->type == htons(UIP_ETHTYPE_ARP)) {
				uip_arp_arpin();

				if (uip_len > 0) {
					send(mac_tx);
				}
				for (int i = 0; i < UIP_UDP_CONNS; i++) {
					uip_udp_arp_event(i);
					if (uip_len > 0) {
						uip_arp_out(&uip_udp_conns[i]);
						send(mac_tx);
					}
				}
			}
		}

		for (int i = 0; i < UIP_UDP_CONNS; i++) {
			if (uip_udp_conn_has_ack(&uip_udp_conns[i])) {
				uip_udp_ackdata(i);
				if (uip_len > 0) {
					uip_arp_out(&uip_udp_conns[i]);
					send(mac_tx);
				}
			}
		}

		if (timer_expired(&arp_timer)) {
			timer_reset(&arp_timer);
			uip_arp_timer();
		}

		if (timer_expired(&autoip_timer)) {
			timer_reset(&autoip_timer);
			autoip_periodic();
			if (uip_len > 0) {
				send(mac_tx);
			}
		}

		if (timer_expired(&periodic_timer)) {

#if UIP_IGMP
			igmp_periodic();
			if(uip_len > 0) {
				send(mac_tx);
			}
#endif
			for (int i = 0; i < UIP_UDP_CONNS; i++) {
				uip_udp_periodic(i);
				if (uip_len > 0) {
					uip_arp_out(&uip_udp_conns[i]);
					send(mac_tx);
				}
			}

			for (int i = 0; i < UIP_CONNS; i++) {
				uip_periodic(i);
				if (uip_len > 0) {
					uip_arp_out( NULL);
					send(mac_tx);
				}
			}

			timer_reset(&periodic_timer);
		}

	}
	return;
}

#endif
