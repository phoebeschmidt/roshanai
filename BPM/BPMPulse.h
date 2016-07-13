
#ifndef _BPMPulse_H_
#define _BPMPulse_H_

#ifdef __cplusplus__
extern "C" {
#endif

#include <stdint.h> // unit16_t, etc.
#include <netinet/in.h> // struct sockaddr_in

struct __BPMPulseData_t{

 uint16_t beat_interval_ms;
		// min BPM 30 has ms interval of 2000
		// max BPM 200 has ms interval of 300

 uint32_t elapsed_ms; // how long before now did this happen?
 
}__attribute__((packed)); 

typedef struct __BPMPulseData_t BPMPulseData_t;

extern int
SetupAnnounce_udp(char* ip, short port, int* sock, struct sockaddr_in* si_toserver);

extern void
AnnounceBPMdata_udp(double interval_ms, double elapsed_ms, int sock, struct sockaddr_in* si);

#ifdef __cplusplus__
} // extern "C"
#endif

#endif // _BPMPulse_H_

