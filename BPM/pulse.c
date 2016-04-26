// pulse.c
// set pulses up and down a strip of leds
// for the moment the pulse is just going to be a sine wave

#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ledscape.h"
#include "BPMPulse.h"

#ifndef TRUE 
#define TRUE 1
#endif // TRUE

#ifndef FALSE
#define FALSE 0
#endif // FALSE

typedef unsigned char uchar_t;

#define LEDS_PER_METER 30  // could be 60, depending on what hardware we end up with
#define STRIP_LEN 1        // number of meters in our strip


#define RED_BASE   0xFF
#define BLUE_BASE  0x00
#define GREEN_BASE 0x00

#define PHASE_PORT 1003
#define FREQ_PORT 1004

#define TARGET_FPS 30

static float pulseWidth = 0.25f;      // meters
static float speed      = 0.1f/1000;  // meters per millisecond
static float pulsePos   = 0;          // midpoint, meters

static float pulseTemplate[] = {0.0f, 0.1f, 0.2f, 0.4f, 0.6f, 0.9f, 1.0f, 0.9f, 0.6f, 0.4f, 0.2f, 0.1f, 0.0f};
static int nTemplateStops;
static float temporalPulseAmplitude[] = {0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.05,  0.07f, 0.09f, 0.11f, 0.14f,
                                         0.20f, 0.30f, 0.45f, 0.48f, 0.50f, 0.50f, 0.48f, 0.45f, 0.30f, 0.20f,
                                         0.14f, 0.11f, 0.09f, 0.07f, 0.05f, 0.0f,   0.0f, 0.0f,  0.0f,  0.0f}; 
static int nTemporalPulsePoints;                   
//static int temporalPulseAmplitudeIdx = 0;  

static int running = TRUE;

static int freqfd=0, phasefd=0;
static int beatsPerMin = 60;

// set up socket to listen for values
static int 
setupListeners(unsigned short phasePort, unsigned short freqPort)
{
    struct sockaddr_in freq_addr, phase_addr;

    printf("Listening for phase on port %d, frequency on port %d\n", phasePort, freqPort);
    
    freqfd  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    phasefd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (freqfd < 0 || phasefd < 0) {
       printf("Error opening socket, aborting\n");
       exit(1);
    }
    
    int true = 1;
    setsockopt(freqfd,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int));
    setsockopt(phasefd,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int));


    memset(&freq_addr, sizeof(freq_addr), 0);
    memset(&phase_addr, sizeof(phase_addr), 0);

    freq_addr.sin_family = AF_INET;
    freq_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    freq_addr.sin_port = htons(phasePort);

    phase_addr.sin_family = AF_INET;
    phase_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    phase_addr.sin_port = htons(freqPort);
    
    if (bind(freqfd, (struct sockaddr *) &freq_addr, sizeof(freq_addr)) < 0) {
        printf("Cannot bind frequency socket, aborting\n");
        return -1;
    }   

    if (bind(phasefd, (struct sockaddr *) &phase_addr, sizeof(phase_addr)) < 0) {
        printf("Cannot bind frequency socket, aborting\n");
        return -1;
    }
   
    printf("listeners set\n"); 
    return 0;
}

static float
getTemporalPulseAmplitude(double prevIdx, int deltaMilliSeconds, double *newIdx)
{
    printf("Getting temporal pulse amplitude\n");
    // get index
    if (prevIdx > nTemporalPulsePoints) prevIdx = nTemporalPulsePoints;

    double curIdx = prevIdx + ((double)((deltaMilliSeconds*beatsPerMin*nTemporalPulsePoints))/(60*1000));
    if (curIdx >= nTemporalPulsePoints) { 
        curIdx -= nTemporalPulsePoints;
    }
    printf("prevIdx is %f, curidx is %f\n", prevIdx, curIdx);
    // and amplitude...
    double intIdxDouble;
    double fracIdx = modf(curIdx, &intIdxDouble);
    
    if (newIdx) {
        *newIdx = curIdx;
    }
    int intIdx = (int)intIdxDouble;
    printf("actual index is %d\n", intIdx);
    float amp =  temporalPulseAmplitude[intIdx]
           + fracIdx*(temporalPulseAmplitude[intIdx + 1]  - temporalPulseAmplitude[intIdx]);
    printf("Returning temporal pulse amplitude, %f\n", amp);
    return amp;
}

static ledscape_t*
configureLedscape(ledscape_config_t *config)
{
    // Configure as strip
    printf("starting to configure ledscape\n");
    config->type = LEDSCAPE_STRIP;
    ledscape_strip_config_t * stripConfig = &config->strip_config;
    stripConfig->leds_width = STRIP_LEN*LEDS_PER_METER;
    stripConfig->leds_height = 1; // 1 strip
    ledscape_t * const leds  = ledscape_init(config, 0);
    
    printf("Configured ledscape, leds is %p\n", leds);
    return leds;
}


int main(int argc, char **argv)
{
    // initialize ledscape
    ledscape_config_t config;
    memset(&config,0, sizeof(ledscape_config_t)); 
    ledscape_t * const leds = configureLedscape(&config);

    // set up sockets to listen on...
    fd_set fds;
    if (setupListeners(PHASE_PORT, FREQ_PORT) < 0) {
        printf("Aborting\n");
        exit(1);
    }

    // initialize pulse structures
    nTemplateStops = sizeof(pulseTemplate)/sizeof(pulseTemplate[0]);
    nTemporalPulsePoints = sizeof(temporalPulseAmplitude)/sizeof(temporalPulseAmplitude[0]);
    printf(" there are %d elements in our pulseTemplate array\n", nTemplateStops);
    printf(" temporal pulse is %d frames\n", nTemporalPulsePoints);
    printf(" Pulse speed is %0.2f meters per second\n", speed*1000);
    printf(" Pulse width is %0.2f meters\n", pulseWidth);
    printf(" allocating array of with room for %d elements\n", nTemplateStops-1);

    int nLEDs = (int)(LEDS_PER_METER*STRIP_LEN);
    
    float temporalPulseAmplitudeIdx = 0; // not an int, we're treating the array as continuous

    // initialize frames
    uchar_t *frame = (uchar_t *)calloc(nLEDs*2,4);
//    uchar_t  frame[nLEDs*2*4];
    float    floatFrame[nLEDs];
    int buffer = 0;

    float templateStopsPerMeter = nTemplateStops / pulseWidth;
    float dydx = templateStopsPerMeter/LEDS_PER_METER;
    int delta_t = 0; // in milliseconds
    struct timespec ts_delta_t;
    double temporalPulseIdx = 0.0;

    while (running) {
        printf("FRAME START...\n");
        clock_gettime(CLOCK_MONOTONIC, &ts_delta_t);
        
        uchar_t *curFrame = frame + (buffer*nLEDs*4); 
        uchar_t *framePtr = curFrame;

        // initialize center of pulse
        pulsePos = pulsePos + speed * delta_t;
        if (pulsePos > STRIP_LEN + pulseWidth/2) {
           pulsePos = -pulseWidth/2;
        }
        printf("pulse pos is %f, delta t is %d\n", pulsePos, delta_t);

        // now find the pixel where the pulse begins
        float pulseStartInMeters = pulsePos - pulseWidth/2;
        float pulseStartInLEDs   = pulseStartInMeters * LEDS_PER_METER;
        double intStartLEDs, fracStartLEDs;
        fracStartLEDs = modf(pulseStartInLEDs, &intStartLEDs);
        // and where the pulse ends...
        float pulseEndInMeters = pulsePos + pulseWidth/2;
        float pulseEndInLEDs   = pulseEndInMeters * LEDS_PER_METER;
        double intEndLEDs, fracEndLEDs;
        fracEndLEDs = modf(pulseEndInMeters, &intEndLEDs);

        // and now put values in the array
        float temporalAmplitude = getTemporalPulseAmplitude(temporalPulseIdx, delta_t, &temporalPulseIdx);
        uchar_t amplitudeChar = 255 * temporalAmplitude;
        printf("Temporal amplitude float is %f, converts to char %d\n", temporalAmplitude, amplitudeChar);
        printf("new idx i %f\n", temporalPulseIdx);
        for (int i=0; i<nLEDs; i++) {
            //printf("LED %d\n", i);
            if (i<pulseStartInLEDs || i>pulseEndInLEDs) {
                floatFrame[i] = 0.0f;
            } else {
                // translate into template coordinates 
                float idxInTemplateCoords = -(pulseStartInMeters*templateStopsPerMeter) + i*dydx; 
                // find previous and next index...
                double prevTemplate, fracTemplate;
                fracTemplate = modf(idxInTemplateCoords, &prevTemplate);
                double prevTemplateValue, nextTemplateValue;
                (prevTemplate < 0                  ) ? (prevTemplateValue = 0) : (prevTemplateValue = pulseTemplate[(int) prevTemplate]);
                (prevTemplate + 1 >= nTemplateStops) ? (nextTemplateValue = 0) : (nextTemplateValue = pulseTemplate[(int)(prevTemplate+1)]);
                if (fracTemplate < 0.01f) {
                   floatFrame[i] = prevTemplateValue;
                } else if (fracTemplate > 0.99f) {
                   floatFrame[i] = nextTemplateValue;
                } else {
                   floatFrame[i] = prevTemplateValue + fracTemplate * (nextTemplateValue - prevTemplateValue);
                }
            }
            
            
          //  printf("FloatFrame of LED %d is %f\n", i, floatFrame[i]);
          //  printf("Red value will be %d\n", (char)(RED_BASE * floatFrame[i]));
            if (floatFrame[i] > 1.0f) {
                floatFrame[i] = 1.0f;
            } else if (floatFrame[i] < 0.0f) {
                floatFrame[i] = 0.0f;
            }
            // covert from intensity to RGB chars...
            uint32_t g = GREEN_BASE*floatFrame[i] + amplitudeChar;
            uint32_t b = BLUE_BASE*floatFrame[i] + amplitudeChar;
            uint32_t r = RED_BASE*floatFrame[i] + amplitudeChar;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            if (r > 255) r = 255;
            printf("r value for frame of LED %d is %d\n", i, r);

            *framePtr++ = g;
            *framePtr++ = b;
            *framePtr++ = r;

          /*  *framePtr++ = GREEN_BASE   * floatFrame[i];
            *framePtr++ = BLUE_BASE    * floatFrame[i];
            *framePtr++ = (uchar_t)(RED_BASE     * floatFrame[i]); */
            *framePtr++ = 0;
        }

        // send to ledscape... 
      //  printf("waiting for ledscape\n");
        ledscape_wait(leds);
      //  printf("drawing with ledscape\n");
        ledscape_draw(leds, curFrame);
      //  printf("finished drawing with ledscape\n");

        // swap buffer
        buffer = 1-buffer;
        
        // wait for something to happen...
        FD_ZERO(&fds);
        FD_SET(freqfd, &fds);
        FD_SET(phasefd, &fds);
        int maxfd = freqfd > phasefd ? freqfd : phasefd;
        struct timeval timeout = {0, 1000000/TARGET_FPS};
        //timeout.tv_sec =  0;
        //timeout.tv_usec = 1000000/TARGET_FPS;
        BPMPulseData_t pulseInfo;
        int nEvents = select(maxfd + 1, &fds, NULL, NULL, &timeout);
        if (nEvents > 0) {
            if (FD_ISSET(freqfd, &fds)) { 
                //ssize_t nBytes = recvfrom(freqfd, &beatsPerMin, 1, 0, NULL, NULL);
                ssize_t nBytes = recvfrom(freqfd, &pulseInfo, sizeof(pulseInfo), 0, NULL, NULL);
                if (nBytes == sizeof(pulseInfo)) {
                   printf("Received frequency reading %d\n", pulseInfo.bpm_tenths_sec); 
                   beatsPerMin = pulseInfo.bpm_tenths_sec * 10;
                } else {
                   printf("Oops. Received %d bytes on freqfd\n", nBytes);
                }
            } 
            if (FD_ISSET(phasefd, &fds)) {
                uchar_t phase; // XXX would like to be able to swap between sockets and files...
                ssize_t nBytes = recvfrom(phasefd, &phase, 1, 0, NULL, NULL);
                printf("Received phase reading %c\n", phase);
                // shift temporalpulse to be more in line with received  (XXX should shift this over time...)
                if (temporalPulseAmplitudeIdx < nTemporalPulsePoints/2) {
                    temporalPulseAmplitudeIdx++;
                } else if (temporalPulseAmplitudeIdx > nTemporalPulsePoints/2) {
                    temporalPulseAmplitudeIdx--;
                }
            }
        } else if (nEvents < 0) {
            if (errno == EINTR) {
                printf("Interrupted. Terminating program\n");
                running = FALSE;
            } else {  
                printf("Received error %d, ignoring\n", errno);
            }
        } else {
           //printf("timeout. next frame\n");
        }

        FD_ZERO(&fds);
 //       usleep(1000000/TARGET_FPS);

        printf("now monotonic is %d s, %ld ns\n", (int)ts_delta_t.tv_sec, ts_delta_t.tv_nsec);
	struct timespec time_new;
        clock_gettime(CLOCK_MONOTONIC, &time_new);
        time_t delta_sec = time_new.tv_sec - ts_delta_t.tv_sec;
        long delta_ns = time_new.tv_nsec - ts_delta_t.tv_nsec;
        printf("delta is %d s, %ld ns\n", (int)delta_sec, delta_ns);
        delta_t = (time_new.tv_sec - ts_delta_t.tv_sec) * 1000 + (time_new.tv_nsec/1000000 - ts_delta_t.tv_nsec/1000000);
        ts_delta_t.tv_sec = time_new.tv_sec;
        ts_delta_t.tv_nsec = time_new.tv_nsec;
        // if (ts_delta_t.tv_nsec < ts_delta_t.tv_
        // delta_t = (((float)(clock() - now))*100)/CLOCKS_PER_SEC;
        if (delta_t < 0) delta_t = 0; // let's just duplicate the frame if we roll.         
    }
    
    ledscape_close(leds);

}


// Helix panel configuration notes


