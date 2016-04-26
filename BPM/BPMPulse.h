
#ifndef _BPMPulse_H_
#define _BPMPulse_H_

#include <stdint.h>

struct __BPMPulseData_t{

 uint16_t bpm_tenths_sec; // BPM's outside 30-250 are dead people,
		// so BPMx10 bounded by 300-2500
 uint32_t detector_time;
 
}__attribute__((packed)); 

typedef struct __BPMPulseData_t BPMPulseData_t;
#endif // _BPMPulse_H_

