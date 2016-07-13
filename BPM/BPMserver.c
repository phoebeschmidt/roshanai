//

#include "BPMPulse.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h> // atoi() on some platforms.
#include <string.h> // memset() on some platforms.
#include <time.h> // just for time() function used in test data.

static void
Fatal(const char* msg) {
	fprintf(stderr, "Fatal: %s\n", msg);
	exit(1);
}

int
main(int ac, char* av[]) {

	int dobroadcast=1;
	short port = 1234;
	struct sockaddr_in si_serve;
	int s,verbose;
	socklen_t slen; // may be an int on some platforms.
	int readbytes; 
	BPMPulseData_t data;	

	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		Fatal("Can't create socket\n");

	if (ac > 1) port = atoi(av[1]);

	memset((char *) &si_serve, 0, sizeof(si_serve));
	si_serve.sin_family = AF_INET;
	si_serve.sin_port = htons(port);


	si_serve.sin_addr.s_addr = htonl(INADDR_ANY);
    	//char *ip = "255.255.255.255";

	printf("port %u\n", port);

#if 0
    	if( setsockopt(s, SOL_SOCKET, SO_BROADCAST, &dobroadcast, sizeof(dobroadcast)) != 0 )
		Fatal("setsockopt");
#endif

	if (bind(s, (struct sockaddr*)&si_serve, sizeof(si_serve))==-1)
		Fatal("bind");

	slen=sizeof(si_serve);
	
	while((readbytes=recvfrom(s, (char*)&data, sizeof(data), 0, (struct sockaddr*)&si_serve, &slen))!=-1) {

		if (data.beat_interval_ms == 0) data.beat_interval_ms = 1; // avoid divide by zero

		printf("Interval %hu ms (%u bpm), delay %u ms\n",
			data.beat_interval_ms,
			60*1000/data.beat_interval_ms,
			data.elapsed_ms);

		if (verbose) printf("Received %d bytes (wanted %lu) from %s:%d\n",
			readbytes, sizeof(data),
			inet_ntoa(si_serve.sin_addr),
			ntohs(si_serve.sin_port));

		slen=sizeof(si_serve);
	}

	close(s);
	return 0;
}

