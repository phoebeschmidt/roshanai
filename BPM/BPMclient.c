//
// The client is sending messages to the 'server'.
// In this case, 'server' just means a known ip and port,
// but we are giving the server data, not vice-versa.
//

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h> // exit() on some platforms.
#include <string.h> // memset() on some platforms.

#include "BPMPulse.h"

static void
Fatal(const char* msg) {
	fprintf(stderr, "Fatal: %s\n", msg);
	exit(1);
}

void
print_data(BPMPulseData_t* data) {
 printf("rolling_sequence %hhu\n",data->rolling_sequence);
 printf("beat_interval_ms %hu\n", data->beat_interval_ms);
 printf("elapsed_ms       %u\n",  data->elapsed_ms);
 printf("est_BPM          %f\n",  data->est_BPM);
}

static void
TestServer(int s, struct sockaddr_in* si, uint8_t pod_id) {

	int i;
	BPMPulseData_t data;

	// generate value approximately between 300 and 2500
	float scale = 2200./((float)RAND_MAX), offset=300.;

	srand(time(NULL));

	data.pod_id = pod_id;
	data.rolling_sequence = 0;

	for (i  = 0; i < 100; i++) {

		data.rolling_sequence++;
 		data.beat_interval_ms = rand()*scale + offset;
 		data.elapsed_ms = 0; // no delay in test server.
 		data.est_BPM = 60.*1000./(float)data.beat_interval_ms;

		print_data(&data);

		sendto(s, (char*)&data, sizeof(data), 0, (struct sockaddr*)si, sizeof(struct sockaddr_in));

		sleep(1);
	}
}


int main(int ac, char* av[]) {

	char* srv_ip = "127.0.0.1";
	short port = 1234;
	int errors = 0;
	BPMPulseData_t data;
	int verbose = 0;
	uint8_t pod_id = 1;

	if (ac > 1) srv_ip = av[1];
	if (ac > 2) port = atoi(av[2]);
	if (ac > 3) pod_id = atoi(av[3]);

	struct sockaddr_in si_toserver;
	int s, i;
	socklen_t slen;
	//int slen; // change for your platform.

	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		Fatal("Can't create socket\n");

	memset((char *) &si_toserver, 0, sizeof(si_toserver));
	si_toserver.sin_family = AF_INET;
	si_toserver.sin_port = htons(port);

	if (inet_aton(srv_ip, &si_toserver.sin_addr)==0)
		Fatal("inet_aton() failed\n");

	TestServer(s, &si_toserver, pod_id);

	close(s);
	return 0;
}

