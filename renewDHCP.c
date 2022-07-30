
//Written by Antoine Zayyat 30-07-2022 Licensed under the GNU General Public License

#include <arpa/inet.h>

#include <linux/if_packet.h>

#include <linux/ip.h>

#include <linux/udp.h>

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <sys/ioctl.h>

#include <sys/socket.h>

#include <net/if.h>

#include <netinet/ether.h>

#include <unistd.h>

#include <pthread.h>

#include <ctype.h>




#define ETHER_TYPE      0x0800



/* DHCP Definitions */

#define MAX_DHCP_CHADDR_LENGTH           16
#define MAX_DHCP_SNAME_LENGTH            64
#define MAX_DHCP_FILE_LENGTH             128
#define MAX_DHCP_OPTIONS_LENGTH          312
#define MAX_DHCP_MESSAGE_SIZE          576

#define BOOTREQUEST     1
#define BOOTREPLY       2

#define DHCPDISCOVER    1
#define DHCPOFFER       2
#define DHCPREQUEST     3
#define DHCPDECLINE     4
#define DHCPACK         5
#define DHCPNACK        6
#define DHCPRELEASE     7


#define DHCP_OPTION_MESSAGE_TYPE        53
#define DHCP_OPTION_HOST_NAME           12
#define DHCP_OPTION_BROADCAST_ADDRESS   28
#define DHCP_OPTION_REQUESTED_ADDRESS   50
#define DHCP_OPTION_LEASE_TIME          51
#define DHCP_OPTION_RENEWAL_TIME        58
#define DHCP_OPTION_REBINDING_TIME      59

#define DHCP_INFINITE_TIME              0xFFFFFFFF

#define DHCP_BROADCAST_FLAG 32768

#define DHCP_SERVER_PORT   67
#define DHCP_CLIENT_PORT   68

#define ETHERNET_HARDWARE_ADDRESS            1     /* used in htype field of dhcp packet */
#define ETHERNET_HARDWARE_ADDRESS_LENGTH     6     /* length of Ethernet hardware addresses */




typedef struct dhcp_packet_struct {

	u_int8_t op;

	u_int8_t htype;

	u_int8_t hlen;

	u_int8_t hops;

	u_int32_t xid;

	u_int16_t secs;

	u_int16_t flags;

	struct in_addr ciaddr;          /* IP address of this machine (if we already have one) */

	struct in_addr yiaddr;          /* IP address of this machine (offered by the DHCP server) */

	struct in_addr siaddr;          /* IP address of DHCP server */

	struct in_addr giaddr;          /* IP address of DHCP relay */

	unsigned char chaddr[MAX_DHCP_CHADDR_LENGTH];      /* hardware address of this machine */

	char sname[MAX_DHCP_SNAME_LENGTH];    /* name of DHCP server */

	char file[MAX_DHCP_FILE_LENGTH];      /* boot file name */

	char options[MAX_DHCP_OPTIONS_LENGTH];

} dhcpPacketStructure;





int createDHCPClientSocket(void) {

	int dhcpClientSocket;

	int reuseSockOption;

	int broadcastSockOption;

	int bindResult;

	int flag = 1;

	struct sockaddr_in client;

	struct ifreq interface;

	bzero(&client, sizeof(client));

	client.sin_family = AF_INET;

	client.sin_addr.s_addr = INADDR_ANY;

	client.sin_port = htons(67);

	bzero(&client.sin_zero ,sizeof(client.sin_zero));

	dhcpClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	(dhcpClientSocket == -1) ? (perror("Socket creation"), exit(0)) : 1;

	/* Set the REUSE socket option on the socket */

	reuseSockOption = setsockopt(dhcpClientSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));

        (reuseSockOption == -1) ? (perror("setsockopt"), close(dhcpClientSocket), exit(0)) : 1;

	/* Set the Broadcast socket option */

	broadcastSockOption = setsockopt(dhcpClientSocket, SOL_SOCKET, SO_BROADCAST, (char *)&flag, sizeof flag);

	(broadcastSockOption == -1) ? (perror("setsockopt"), close(dhcpClientSocket), exit(0)) : 1;

	bindResult = bind(dhcpClientSocket, (struct sockaddr *)&client, sizeof(client));

	(bindResult == -1) ? (perror("setsockopt"), close(dhcpClientSocket), exit(0)) : 1;

	return dhcpClientSocket;


}







unsigned int *get_hardware_address(char *interface_name) {

	int ioctlResp;

	int tempSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	(tempSock == -1) ? (perror("Socket creation"), exit(0)) : 1;

	struct ifreq ifr;

	memcpy(ifr.ifr_name, interface_name, sizeof(interface_name));

	ioctlResp = ioctl(tempSock, SIOCGIFHWADDR, &ifr);

	(ioctlResp == -1) ? (perror("ioctl"), close(tempSock), exit(0)) : 1;

	unsigned int *macAddress = malloc(6);

	(macAddress == NULL) ? (perror("malloc"), close(tempSock), exit(0)) : 1;

	memcpy(macAddress, &ifr.ifr_hwaddr.sa_data[0], 1);

	memcpy(macAddress+1, &ifr.ifr_hwaddr.sa_data[1], 1);

	memcpy(macAddress+2, &ifr.ifr_hwaddr.sa_data[2], 1);

	memcpy(macAddress+3, &ifr.ifr_hwaddr.sa_data[3], 1);

	memcpy(macAddress+4, &ifr.ifr_hwaddr.sa_data[4], 1);

	memcpy(macAddress+5, &ifr.ifr_hwaddr.sa_data[5], 1);

	return macAddress;

}


char *getIPFromInterface(char *interface) {

	int socketFD;

	struct ifreq ifr;

	int ioctlResult;

	socketFD = socket(AF_INET, SOCK_DGRAM, 0);

	(socketFD == -1) ? (perror("Socket creation"), exit(0)) : 1;

	ifr.ifr_addr.sa_family = AF_INET;

	strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);

	ioctlResult = ioctl(socketFD, SIOCGIFADDR, &ifr);

	(ioctlResult == -1) ? (perror("ioctl"), close(socketFD), exit(0)) : 1;

	close(socketFD);

	 return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

}




int main(int argc, char *argv[]) {

	int sock = createDHCPClientSocket();

	struct sockaddr_in client;

	int *macAddress = get_hardware_address(argv[1]);

	char hostname[50];

	bzero(hostname, 50);

	int gethostnameResult = gethostname(hostname, 50);

	(gethostnameResult == -1) ? (perror("gethostname"), close(sock), exit(0)) : 1;

	char *ipAddress = getIPFromInterface(argv[1]);

	char *str;

	unsigned char value[4] = {0};

	size_t index = 0;

	str = ipAddress;

	while(*ipAddress) {

		if(isdigit((unsigned char)*ipAddress)) {

			value[index] *= 10;

			value[index] += *ipAddress - '0';

		}

		else {

			index++;

		}


		ipAddress++;

	}

	dhcpPacketStructure dhcpRequest;

	bzero(&dhcpRequest, sizeof(dhcpRequest));

	dhcpRequest.op = DHCPREQUEST;

	dhcpRequest.htype = ETHERNET_HARDWARE_ADDRESS;

	dhcpRequest.hlen = ETHERNET_HARDWARE_ADDRESS_LENGTH;

	u_int32_t pktXid = 0;

	srand(time(NULL));

	pktXid = random();

	dhcpRequest.xid = htonl(pktXid);

	ntohl(dhcpRequest.xid);

	dhcpRequest.secs = 0x01;

	dhcpRequest.flags = ntohs(0x000000);

	for(int i = 0; i < 6; i++) {

		dhcpRequest.chaddr[i] = macAddress[i];

	}

	dhcpRequest.options[0] = '\x63';

	dhcpRequest.options[1] = '\x82';

	dhcpRequest.options[2] = '\x53';

	dhcpRequest.options[3] = '\x63';

	dhcpRequest.options[4] = DHCP_OPTION_MESSAGE_TYPE;

	dhcpRequest.options[5] = '\x01';

	dhcpRequest.options[6] = DHCPREQUEST;

	dhcpRequest.options[7] = 61;

	dhcpRequest.options[8] = '\x07';

	dhcpRequest.options[9] = '\x01';

	dhcpRequest.options[10] = macAddress[0];

	dhcpRequest.options[11] = macAddress[1];

	dhcpRequest.options[12] = macAddress[2];

	dhcpRequest.options[13] = macAddress[3];

	dhcpRequest.options[14] = macAddress[4];

	dhcpRequest.options[15] = macAddress[5];

	dhcpRequest.options[16] = 55;

	dhcpRequest.options[17] = 17;

	dhcpRequest.options[18] = 1;

	dhcpRequest.options[19] = 2;

	dhcpRequest.options[20] = 6;

	dhcpRequest.options[21] = 12;

	dhcpRequest.options[22] = 15;

	dhcpRequest.options[23] = 26;

	dhcpRequest.options[24] = 28;

	dhcpRequest.options[25] = 121;

	dhcpRequest.options[26] = 3;

	dhcpRequest.options[27] = 33;

	dhcpRequest.options[28] = 40;

	dhcpRequest.options[29] = 41;

	dhcpRequest.options[30] = 42;

	dhcpRequest.options[31] = 119;

	dhcpRequest.options[32] = 249;

	dhcpRequest.options[33] = 252;

	dhcpRequest.options[34] = 17;

	dhcpRequest.options[35] = 57;

	dhcpRequest.options[36] = 2;

	dhcpRequest.options[37] = 2;

	dhcpRequest.options[39] = 50;

	dhcpRequest.options[40] = 4;

	dhcpRequest.options[41] = value[0];

	dhcpRequest.options[42] = value[1];

	dhcpRequest.options[43] = value[2];

	dhcpRequest.options[44] = value[3];

	dhcpRequest.options[45] = 12;

	dhcpRequest.options[46] = 6;

	int anotherIndex = 47;

	for(int i = 0; i < 50; i++) {

		dhcpRequest.options[anotherIndex] = hostname[i];

		anotherIndex++;

	}

	dhcpRequest.options[anotherIndex+1] = '\xff';

	dhcpRequest.options[anotherIndex+2] = '\xff';

	client.sin_family = AF_INET;

	client.sin_addr.s_addr = inet_addr("192.168.1.1"); //ip address of the dhcp server that leased you an ip address

	client.sin_port = htons(67);

	sendto(sock, (char *)&dhcpRequest, sizeof(dhcpRequest), 0, (const struct sockaddr *)&client, sizeof(client));

}
