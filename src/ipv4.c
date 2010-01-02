/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2010  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "connman.h"

struct connman_ipv4 {
	enum connman_ipconfig_method method;
	struct in_addr address;
	struct in_addr netmask;
	struct in_addr broadcast;
};

static int set_ipv4(struct connman_element *element,
			struct connman_ipv4 *ipv4, const char *nameserver)
{
	struct ifreq ifr;
	struct sockaddr_in addr;
	int sk, err;

	DBG("element %p ipv4 %p", element, ipv4);

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = element->index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0) {
		close(sk);
		return -1;
	}

	DBG("ifname %s", ifr.ifr_name);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr = ipv4->address;
	memcpy(&ifr.ifr_addr, &addr, sizeof(ifr.ifr_addr));

	err = ioctl(sk, SIOCSIFADDR, &ifr);

	if (err < 0)
		DBG("address setting failed (%s)", strerror(errno));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr = ipv4->netmask;
	memcpy(&ifr.ifr_netmask, &addr, sizeof(ifr.ifr_netmask));

	err = ioctl(sk, SIOCSIFNETMASK, &ifr);

	if (err < 0)
		DBG("netmask setting failed (%s)", strerror(errno));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr = ipv4->broadcast;
	memcpy(&ifr.ifr_broadaddr, &addr, sizeof(ifr.ifr_broadaddr));

	err = ioctl(sk, SIOCSIFBRDADDR, &ifr);

	if (err < 0)
		DBG("broadcast setting failed (%s)", strerror(errno));

	close(sk);

	if (nameserver == NULL)
		connman_error("No nameserver for %s defined", ifr.ifr_name);
	else
		connman_resolver_append(ifr.ifr_name, NULL, nameserver);

	return 0;
}

static int clear_ipv4(struct connman_element *element)
{
	struct ifreq ifr;
	struct sockaddr_in addr;
	int sk, err;

	DBG("element %p", element);

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = element->index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0) {
		close(sk);
		return -1;
	}

	DBG("ifname %s", ifr.ifr_name);

	connman_resolver_remove_all(ifr.ifr_name);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	memcpy(&ifr.ifr_addr, &addr, sizeof(ifr.ifr_addr));

	//err = ioctl(sk, SIOCDIFADDR, &ifr);
	err = ioctl(sk, SIOCSIFADDR, &ifr);

	close(sk);

	if (err < 0 && errno != EADDRNOTAVAIL) {
		DBG("address removal failed (%s)", strerror(errno));
		return -1;
	}

	return 0;
}

static char *index2name(int index)
{
	struct ifreq ifr;
	int sk, err;

	if (index < 0)
		return NULL;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	err = ioctl(sk, SIOCGIFNAME, &ifr);

	close(sk);

	if (err < 0)
		return NULL;

	return strdup(ifr.ifr_name);
}

static int ipv4_probe(struct connman_element *element)
{
	struct connman_element *connection;
	struct connman_ipv4 ipv4;
	const char *address = NULL, *netmask = NULL, *broadcast = NULL;
	const char *nameserver = NULL, *timeserver = NULL;

	DBG("element %p name %s", element, element->name);

	connman_element_get_value(element,
				CONNMAN_PROPERTY_ID_IPV4_ADDRESS, &address);
	connman_element_get_value(element,
				CONNMAN_PROPERTY_ID_IPV4_NETMASK, &netmask);
	connman_element_get_value(element,
				CONNMAN_PROPERTY_ID_IPV4_BROADCAST, &broadcast);

	connman_element_get_value(element,
			CONNMAN_PROPERTY_ID_IPV4_NAMESERVER, &nameserver);
	connman_element_get_value(element,
			CONNMAN_PROPERTY_ID_IPV4_TIMESERVER, &timeserver);

	DBG("address %s", address);
	DBG("netmask %s", netmask);
	DBG("broadcast %s", broadcast);

	if (address == NULL || netmask == NULL)
		return -EINVAL;

	memset(&ipv4, 0, sizeof(ipv4));
	ipv4.address.s_addr = inet_addr(address);
	ipv4.netmask.s_addr = inet_addr(netmask);
	if (broadcast)
		ipv4.broadcast.s_addr = inet_addr(broadcast);
	else
		ipv4.broadcast.s_addr = ipv4.address.s_addr |
						~ipv4.netmask.s_addr;

	set_ipv4(element, &ipv4, nameserver);

	connman_timeserver_append(timeserver);

	connection = connman_element_create(NULL);

	connection->type    = CONNMAN_ELEMENT_TYPE_CONNECTION;
	connection->index   = element->index;
	connection->devname = index2name(element->index);

	if (connman_element_register(connection, element) < 0)
		connman_element_unref(connection);

	return 0;
}

static void ipv4_remove(struct connman_element *element)
{
	const char *timeserver = NULL;

	DBG("element %p name %s", element, element->name);

	connman_element_get_value(element,
			CONNMAN_PROPERTY_ID_IPV4_TIMESERVER, &timeserver);

	connman_timeserver_remove(timeserver);

	clear_ipv4(element);
}

static struct connman_driver ipv4_driver = {
	.name		= "ipv4",
	.type		= CONNMAN_ELEMENT_TYPE_IPV4,
	.priority	= CONNMAN_DRIVER_PRIORITY_LOW,
	.probe		= ipv4_probe,
	.remove		= ipv4_remove,
};

int __connman_ipv4_init(void)
{
	return connman_driver_register(&ipv4_driver);
}

void __connman_ipv4_cleanup(void)
{
	connman_driver_unregister(&ipv4_driver);
}
