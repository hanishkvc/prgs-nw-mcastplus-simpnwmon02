#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

int portMCast=1111;
int portServer=1112;
int portClient=1113;

char *sLocalAddr="0.0.0.0";
char *sPIInitAddr="255.255.255.255";

int sock_mcast_init_ex(char *sMCastAddr)
{
	int iRet;
	int sockMCast = -1;
	struct ip_mreqn mreqn;

	sockMCast = socket(AF_INET, SOCK_DGRAM, 0);
	
	iRet = inet_aton(sMCastAddr, &mreqn.imr_multiaddr);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to set MCastAddr[%s], ret=[%d]\n", __func__, sMCastAddr, iRet);
		exit(-1);
	}
	iRet = inet_aton(sLocalAddr, &mreqn.imr_address);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to set localAddr[%s], ret=[%d]\n", __func__, sLocalAddr, iRet);
		exit(-1);
	}
	mreqn.imr_ifindex = 0;
	iRet = setsockopt(sockMCast, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn));
	if (iRet != 0) {
		fprintf(stderr, "ERROR:%s: Failed joining group, ret=[%d]\n", __func__, iRet);
		perror("Failed joining group:");
		exit(-1);
	}
	return sockMCast;
}




int main(int argc, char **argv) {

	int sockMCast = -1;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <mcast_addr>\n", argv[0]);
		exit(-1);
	}
	char *sMCastAddr = argv[1];
	sockMCast = sock_mcast_init_ex(sMCastAddr);
	fprintf(stderr, "INFO:%s: sockMCast [%d] setup for [%s]\n", __func__, sockMCast, sMCastAddr);

}

