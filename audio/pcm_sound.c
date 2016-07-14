/* And now some C code for handling the audio 
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <asoundlib.h>

//#include "./alsa-lib-1.1.1/include/asoundlib.h"
#include "pcm_config.h"
#include "pcm_utils.h"

// but typically I get a callback which is called from a different thread. See how alsa
// does this. 
// But wait, let's do the wait method...

#define REQUESTED_SAMPLE_RATE 44100
#define REQUESTED_BUFFER_SIZE_US 500000
#define REQUESTED_PERIOD_SIZE_US 100000

// NB - sounds are assumed to be mono

static unsigned char *currentSoundBuf    = NULL;   // sound we're currently playing. 
static unsigned char *nextSoundBuf       = NULL;   // Next sound to play // XXX make these pointers to frames....
static int  currentSoundLength   = 0;     // length of current sound
static int  nextSoundLength      = 0;     // length of next sound
static int  nextSoundTimeBytes   = 0;     // how many bytes away from the current write pointer to start the next sound

static unsigned char *lastSoundBuf       = NULL;   // last sound we played, in case we have to re-play it
static int lastSoundLength      = 0;
static int lastSoundFreqMs      = 0;

static unsigned char *currentSoundBufPtr = NULL;
static pthread_t playbackThread;

static unsigned char *silentBuffer = NULL;

static const char *device = "default";    // hope this works.... XXX
static snd_pcm_t *handle    = NULL;
static snd_output_t *output = NULL;

static int sampleRate = REQUESTED_SAMPLE_RATE;
static snd_pcm_uframes_t  bufferSize = 0;
static snd_pcm_uframes_t  periodSize = 0; 
static int bytesPerPeriod = 0; // derived from periodSize and other values

static int bytesPerSample = 8; 
static int nChannels      = 1;
static int frameRate      = REQUESTED_SAMPLE_RATE; 

static mqd_t commandmq;

// XXX for the moment just to fix compliation errors
const char *snd_strerror(int err);

// Okay. It appears that we do select to receive events. So I do need
// a sound processing thread, and I'm going to select on buffer starve events
// as well as commands from an incoming message queue.

#define HB_FREQ_MIN 30
#define HB_FREQ_MAX 150

typedef struct {
    unsigned char *soundBuf;
    int bufLen;
    struct timespec startTime;
    int freqMs;
}PcmSound;

/*
static PcmSound currentSound;
static PcmSound lastSound;
static PcmSound nextSound;
*/

// initialization
static void createPcmSound(PcmSound *sound, unsigned char *pcmSoundBuf, int pcmBufLen, int freqMs); 
static int setPcmParams(snd_pcm_t *handle, snd_pcm_hw_params_t *hwParams, snd_pcm_sw_params_t *swParams); 
static int setHwParams(snd_pcm_t *handle, snd_pcm_hw_params_t *hwParams);
static int setSwParams(snd_pcm_t *handle, snd_pcm_sw_params_t *swParams);

// playback thread
static void* pcmPlaybackThread(void *arg);
static int feedBuffer(void); 
static snd_pcm_sframes_t getAudioDelay(void);
static int initializeSilentBuffer(int silentTimeMs); 
static int fillBufferWithSilence(int silentBytes, int *reInit); 
static int fillBufferWithSound(unsigned char *soundBuffer, int soundBytes, int *reInit);
static int xrun_recovery(snd_pcm_t *handle, int err);


int pcmPlaySound(unsigned char* pcmSoundBuf, int pcmBufLen, unsigned int freqMs)
{
    PcmSound newSound; 
    createPcmSound(&newSound, pcmSoundBuf, pcmBufLen, freqMs);
    if (mq_send(commandmq, (const char *)&newSound, sizeof(newSound), 0) != 0) {
        printf("Could not send message to play sound: %s\n", strerror(errno));
        return ERR_PCM_MISC;
    }
    
    return 0;
}

void pcmPlayHeartBeat(unsigned int freqBPS)
{
    HbData *sound;
    
    if (freqBPS > HB_FREQ_MAX) {
        freqBPS = HB_FREQ_MAX;
    } else if (freqBPS < HB_FREQ_MIN) {
        freqBPS = HB_FREQ_MIN;
    }
    
    // Get sound appropriate for this frequency
    sound = PcmConfig_getHbSound(freqBPS);
    pcmPlaySound(sound->data, sound->datalen, 1000*60/freqBPS);
}



int pcmPlaybackInit() 
{
    int err;
    pthread_attr_t attr;
    
    if ((err = PcmConfig_Read()) != 0){
        return err;
    }
    
    if ((err = initializeSilentBuffer(10000)) != 0) {
        return err;
    }
    
    snd_pcm_hw_params_t *hwParams = NULL;
    snd_pcm_sw_params_t *swParams = NULL;
    snd_pcm_hw_params_alloca(&hwParams);
    snd_pcm_sw_params_alloca(&swParams);
    
    if (hwParams == NULL || swParams == NULL) {
        printf("Could not allocate parameter buffers\n");
        return ERR_PCM_MISC;
    }
        
    if ((err = snd_output_stdio_attach(&output, stdout, 0)) < 0) {
        printf("Output failed: %s\n", snd_strerror(err)); 
        return ERR_PCM_MISC;
    }

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return ERR_PCM_OPEN; 
    }
    
    if ((err = setPcmParams(handle, hwParams, swParams)) < 0) {
        printf("Set parameter error: %s\n", snd_strerror(err));
        return ERR_PCM_PARAM;
    }
    
    if ((commandmq = mq_open("Pulse PCM MQ", O_RDWR|O_CREAT)) < 0) { 
        printf("Error creating message queue: %s\n", strerror(errno));
        return ERR_PCM_QUEUE;
    }
    
    if ((err = pthread_attr_init(&attr)) != 0) {
        printf("Error initializing thread: %s\n", strerror(err));
        return ERR_PCM_MISC;   
    }    
    
    if ((err = pthread_create(&playbackThread, &attr, &pcmPlaybackThread, NULL)) != 0) {
        printf("Error creating playback thread: %s\n", strerror(err));
        return ERR_PCM_THREAD_CREATE;
    } 
    
    pthread_attr_destroy(&attr); 
    
    return 0;
}


static void createPcmSound(PcmSound *sound, unsigned char *pcmSoundBuf, int pcmBufLen, int freqMs) 
{
    if (sound) {
        sound->soundBuf = pcmSoundBuf;
        sound->bufLen   = pcmBufLen;
        struct timespec currentTime;
        clock_gettime(CLOCK_MONOTONIC, &currentTime);  // XXX does CLOCKMONOTONIC exist?
        
        int playbackStartS  = freqMs/1000;
        int playbackStartMs = freqMs - (playbackStartS * 1000);
        int playbackStartNs = currentTime.tv_nsec + (playbackStartMs * 1000000);
        if (playbackStartNs < currentTime.tv_nsec) {  // do we ever actually wrap? check the resolution
            // XXX check can wrap happen
        } else if (playbackStartNs > 1000000000) {
            playbackStartS++;
            playbackStartNs -= 1000000000;
        }
        
        sound->startTime.tv_sec  = playbackStartS;
        sound->startTime.tv_nsec = playbackStartNs;
        sound->freqMs = freqMs;
    }
    
    return;
}


// XXX must have shorter periods in order to handle higher heart rates


static int setPcmParams(snd_pcm_t *handle, snd_pcm_hw_params_t *hwParams, snd_pcm_sw_params_t *swParams) 
{
    int err;
    if ((err == setHwParams(handle, hwParams)) < 0) {
        return err;
    }
    
    if ((err == setSwParams(handle, swParams)) < 0) {
        return err;
    }
    
    return 0;
}

static int setHwParams(snd_pcm_t *handle, snd_pcm_hw_params_t *hwParams) {

    int err, dir;
    
    // set hardware parameters first...
    if ((err = snd_pcm_hw_params_any(handle, hwParams)) < 0) {
        printf("No playback configurations available: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_rate_resample(handle, hwParams, 1)) < 0) {
        printf("No playback configurations available: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("Could not set raw interleaved access: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_format(handle, hwParams, SND_PCM_FORMAT_U8 )) < 0) {
        printf("Could not sent sound format to 8bit: %s\n", snd_strerror(err));
        return err;
    }

    if ((err = snd_pcm_hw_params_set_channels(handle, hwParams, 1)) < 0) {
        printf("Could not set mono mode: %s\n", snd_strerror(err));
        return err;
    }
    
    unsigned int rrate = REQUESTED_SAMPLE_RATE;
    if ((err = snd_pcm_hw_params_set_rate_near(handle, hwParams, &rrate, 0)) < 0) {
        printf("Could not set rate: %s\n", snd_strerror(err));
        return err;
    }
    if (rrate != REQUESTED_SAMPLE_RATE) {
        printf("Sample Rate %d not equal to requested rate %d\n", rrate, REQUESTED_SAMPLE_RATE);
    }
    sampleRate = rrate;
    
    printf("Init: Sample rate is %d\n", sampleRate);
    frameRate = sampleRate; // XXX clean this up
    unsigned int requestedBufferSize = REQUESTED_BUFFER_SIZE_US;
    unsigned int requestedPeriodSize = REQUESTED_PERIOD_SIZE_US;
    
    if ((err = snd_pcm_hw_params_set_buffer_time_near(handle, hwParams, &requestedBufferSize, &dir)) < 0) {
        printf("Could not set buffer size: %s\n", snd_strerror(err));
        return err;
    }// XXX check actual buffer size here
    if ((err = snd_pcm_hw_params_get_buffer_size(hwParams, &bufferSize)) < 0) {
        printf("Could not get buffer size: %s\n", snd_strerror(err));
        return err;
    }
    printf("Init: Buffer size is %d frames\n", (int)bufferSize);
    
    if ((err = snd_pcm_hw_params_set_period_time_near(handle, hwParams, &requestedPeriodSize, &dir)) < 0) {
        printf("Could not set period size: %s\n", snd_strerror(err));
        return err;
    } // XXX check actual period size returned
    periodSize = (REQUESTED_PERIOD_SIZE_US * rrate)/ 1000; // XXX check this...
    if ((err = snd_pcm_hw_params_get_period_size(hwParams, &periodSize, &dir)) < 0) {
        printf("Could not get period size: %s\n", snd_strerror(err));
        return err;
    }
    printf("Init: PeriodSize is %d frames\n", (int)periodSize);
    bytesPerPeriod = periodSize*bytesPerSample*nChannels;
    
    if ((err = snd_pcm_hw_params(handle, hwParams)) < 0) {
        printf("Cannot commit hw params: %s\n", snd_strerror(err));
        return err;
    }
    
    return 0;
}

static int setSwParams(snd_pcm_t *handle, snd_pcm_sw_params_t *swParams)
{
    int err;
    if ((err = snd_pcm_sw_params_current(handle, swParams)) < 0) {
        printf("Cannot determine current swParams: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    if ((err = snd_pcm_sw_params_set_start_threshold(handle, swParams, (bufferSize / periodSize) * periodSize)) < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least periodSize samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    if ((err = snd_pcm_sw_params_set_avail_min(handle, swParams, bufferSize )) < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    // trigger period events
    if ((err = snd_pcm_sw_params_set_period_event(handle, swParams, 1)) < 0) {
        printf("Unable to set period event: %s\n", snd_strerror(err));
        return err;
    }
    /* write the parameters to the playback device */
    if ((err = snd_pcm_sw_params(handle, swParams)) < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}


/* Everything from here down should only be called on the playback thread */
static void* pcmPlaybackThread(void *arg)
{
    struct pollfd *ufds;
    PcmSound soundMsg;
    int err;
    
    int pcmDescriptorsCount = snd_pcm_poll_descriptors_count (handle);
    if (pcmDescriptorsCount <= 0) {
        printf("PCM Playback Fatal: Invalid number of poll descriptors\n");
        return NULL;
    }
    ufds = malloc(sizeof(struct pollfd) * (pcmDescriptorsCount + 1));
    if (!ufds) {
        printf("PCM Playback Fatal: Could not allocate memory for poll descriptors \n");
        return NULL;
    }
    
    // setting up the message queue fd structure first
    ufds->fd = commandmq;
    ufds->events = POLLIN;
    ufds->revents = 0;
    
    // now the sound events
    if ((err = snd_pcm_poll_descriptors(handle, ufds+1, pcmDescriptorsCount)) < 0) {
        printf("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
        return NULL;
    }
    while (1) {
        int waitTime = -1;
        // XXX - from the sample code, it appears that POLL only works if you've 
        // done something to prime the pump and get the code into a running state
//        if (snd_pcm_state(handle) == SND_PCM_STATE_RUNNING) {
            struct timespec pollStartTime;
            clock_gettime(CLOCK_MONOTONIC, &pollStartTime);
            err = poll(ufds, pcmDescriptorsCount + 1, waitTime);
            if (err == 0) { // timeout, generate a new event
                if (lastSoundBuf) {
                    nextSoundBuf     = lastSoundBuf;
                    nextSoundLength  = lastSoundLength;
                    waitTime         = lastSoundFreqMs;
                    snd_pcm_sframes_t delayFrames = getAudioDelay();
                    int delayMs                   = delayFrames*1000/frameRate; // XXXX check this
                    nextSoundTimeBytes = ((lastSoundFreqMs - delayMs) * frameRate * nChannels)/1000;
                    nextSoundTimeBytes = MAX(0,nextSoundTimeBytes);
                }
            } else if (err < 0) { // actual error. Maybe bail
                printf("Warning: Poll() returns error %s\n", strerror(errno));
                // XXX check for error and decide what to do...
                // XXX set waittime...
            } else {
                // check fds for results
                unsigned short mqEvent = ufds[0].revents;
                if (mqEvent) {
                    printf("Receive poll event %d on message queue\n", mqEvent);
                    if (mqEvent & POLLIN) {
                        int bytesRead = mq_receive(commandmq, (char *)(&soundMsg), sizeof(soundMsg), NULL);
                        if (bytesRead < sizeof(soundMsg)) {
                            printf("Received malformed sound message, discarding\n");
                        } else {
                            struct timespec currentTime;
                            clock_gettime(CLOCK_MONOTONIC, &currentTime);  // XXX does CLOCKMONOTONIC exist?
                            if ((currentTime.tv_sec > soundMsg.startTime.tv_sec) || 
                                 ( (currentTime.tv_sec == soundMsg.startTime.tv_sec) && 
                                   (currentTime.tv_nsec > soundMsg.startTime.tv_nsec))) {
                                // This message has expired. Ignore it. 
                                printf("Received expired message, ignoring\n");  
                            } else {
                                nextSoundBuf    = soundMsg.soundBuf;
                                nextSoundLength = soundMsg.bufLen;
                                struct timespec nextTime;
                                nextTime.tv_sec  = soundMsg.startTime.tv_sec;
                                nextTime.tv_nsec = soundMsg.startTime.tv_nsec;
                                int deltaS = nextTime.tv_sec - currentTime.tv_sec;
                                if (nextTime.tv_nsec < currentTime.tv_nsec) {
                                    deltaS -= 1;
                                    nextTime.tv_nsec += 1000000000;
                                }
                                int deltaNs = nextTime.tv_nsec - currentTime.tv_nsec;
                                int nextSoundDelayMs = (deltaS * 1000) +  deltaNs/1000000; // how long to wait to play the next sound
                                snd_pcm_sframes_t delayFrames = getAudioDelay();
                                int delayMs = delayFrames*1000/frameRate; // XXXX check this
                                nextSoundTimeBytes = ((nextSoundDelayMs - delayMs) * frameRate * nChannels)/1000;
                                nextSoundTimeBytes = MAX(0, nextSoundTimeBytes);
                                waitTime = soundMsg.freqMs;
                                lastSoundBuf    = nextSoundBuf;
                                lastSoundLength = nextSoundLength;
                                lastSoundFreqMs = soundMsg.freqMs;
                            }
                        }
                    } 
                    ufds[0].revents = 0;
                } else {
                    unsigned short revents;
                    snd_pcm_poll_descriptors_revents(handle, ufds+1, pcmDescriptorsCount, &revents);
                    if (revents & POLLERR) {
                        printf("Warning: PCM poll returns IO error\n");
                        if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN ||
                            snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED) {
                            err = snd_pcm_state(handle) == SND_PCM_STATE_XRUN ? -EPIPE : -ESTRPIPE;
                            if (xrun_recovery(handle, err) < 0) {
                                printf("Cannot recover from error %s\n", snd_strerror(err));
                                exit(EXIT_FAILURE);// XXX not what I want...
                            }
                        }
                    }
                    if (revents & POLLOUT) {
                        feedBuffer();
                    }
                    struct timespec currentTime;
                    clock_gettime(CLOCK_MONOTONIC, &currentTime);  // XXX does CLOCKMONOTONIC exist?
                    int deltaS = currentTime.tv_sec - pollStartTime.tv_sec;
                    if (currentTime.tv_nsec < pollStartTime.tv_nsec) {
                        deltaS -= 1;
                        currentTime.tv_nsec += 1000000000;
                    }
                    int deltaNs = currentTime.tv_nsec - pollStartTime.tv_nsec;
                    waitTime = deltaS*1000 + deltaNs*1000000;
                    assert(waitTime >= 0); 
                }
            }
//        }
    }
        
}


static int feedBuffer(void) {
    int reInit = 0;
    int totalBytesFilled = 0;
    
    // if we're currently playing a sound, attempt to finish it
    if (currentSoundBuf) {
        assert(currentSoundBufPtr);
        int bytesToWrite = MIN(currentSoundLength - (currentSoundBufPtr - currentSoundBuf), bytesPerPeriod);
        int bytesFilled = fillBufferWithSound(currentSoundBufPtr, bytesToWrite, &reInit);
        if (bytesFilled >= 0) {
            totalBytesFilled += bytesFilled;
            currentSoundBufPtr += bytesFilled;
            if (currentSoundBufPtr - currentSoundBuf >= currentSoundLength) { 
                // we've finished the current sound. Reset variables
                currentSoundBuf = NULL;
                currentSoundBufPtr = NULL;
                currentSoundLength = 0;
            }
            if (bytesFilled != bytesToWrite) {
                printf("Did not write as many bytes as expected: %d written, vs %d expected\n", bytesFilled, bytesToWrite);
                if (reInit) {
                    if (nextSoundBuf) {
                        nextSoundTimeBytes = MAX(0, nextSoundTimeBytes - bytesFilled);
                    }
                    return -1;
                }
            }
        } else {
            // This is an unrecoverable error. Not sure what to do.
            printf("Audio system unrecoverable error. Ack Ack Ack\n"); // XXX FIXME
        }   
    }
        
    // if there's space in the buffer, add more stuff
    // silence and then the next sound, if there is a next sound.
    if (totalBytesFilled < bytesPerPeriod) {
        assert(!currentSoundBuf);
        int bytesFilled = 0;
        int bytesToWrite = 0;
        if (!nextSoundBuf) {
            bytesToWrite = bytesPerPeriod - totalBytesFilled;
            bytesFilled = fillBufferWithSilence(bytesToWrite, &reInit);
        } else if (nextSoundTimeBytes > totalBytesFilled) {
            bytesToWrite = MIN(bytesPerPeriod-totalBytesFilled, nextSoundTimeBytes - totalBytesFilled);
            bytesFilled = fillBufferWithSilence(bytesToWrite, &reInit);
        } 
        if (bytesFilled >= 0) {
            totalBytesFilled += bytesFilled;
            if (bytesFilled != bytesToWrite) {
                printf("Did not write as many bytes as expected: %d written, vs %d expected\n", bytesFilled, bytesToWrite);
                if (reInit) {
                    if (nextSoundBuf) {
                        nextSoundTimeBytes = MAX(0, nextSoundTimeBytes - bytesFilled);
                    }
                    return -1;
                } 
            }
        } else {
            // This is an unrecoverable error. Not sure what to do.
            printf("Audio system unrecoverable error. Ack Ack Ack\n"); // XXX FIXME
        }
        
        // Next sound, if there's space for it
        if (totalBytesFilled < bytesPerPeriod) {
            assert(nextSoundBuf);
            currentSoundBuf    = nextSoundBuf;
            currentSoundBufPtr = currentSoundBuf;
            currentSoundLength = nextSoundLength;
            nextSoundBuf       = NULL;
            nextSoundTimeBytes = 0;
            bytesToWrite = MIN(currentSoundLength, bytesPerPeriod-totalBytesFilled);
            bytesFilled = fillBufferWithSound(currentSoundBuf, bytesFilled, &reInit);
            if (bytesFilled >= 0) {
                totalBytesFilled += bytesFilled;
                if (currentSoundLength <= bytesFilled) {
                    currentSoundBuf    = NULL;
                    currentSoundBufPtr = NULL;
                    currentSoundLength = 0;
                } else {
                    currentSoundBufPtr += bytesFilled;
                }
                if (bytesFilled != bytesToWrite) {
                    printf("Did not write as many bytes as expected: %d written, vs %d expected\n", bytesFilled, bytesToWrite);
                    if (reInit) {
                        return -1;
                    } 
                } 
            } else {
                // This is an unrecoverable error. Not sure what to do.
                printf("Audio system unrecoverable error. Ack Ack Ack\n"); // XXX FIXME
            }
        }
        
        // And finally, silence, if there's space for it
        if (totalBytesFilled < bytesPerPeriod) {
            assert(!currentSoundBuf);
            assert(!nextSoundBuf);
            bytesToWrite = totalBytesFilled < bytesPerPeriod;
            bytesFilled = fillBufferWithSilence(bytesToWrite, &reInit);
            if (bytesFilled >= 0) {
                totalBytesFilled += bytesFilled;
                if (bytesFilled != bytesToWrite) {
                    printf("Did not write as many bytes as expected: %d written, vs %d expected\n", bytesFilled, bytesToWrite);
                    if (reInit) {
                        return -1;
                    } 
                }
            } else {
                // This is an unrecoverable error. Not sure what to do.
                printf("Audio system unrecoverable error. Ack Ack Ack\n"); // XXX FIXME
            }
        }
        
        // adjust nextSoundTimeBytes, if necessary
        if (nextSoundBuf) {
            nextSoundTimeBytes -= totalBytesFilled;
        }
    }
    
    return 0;
}

static snd_pcm_sframes_t getAudioDelay(void)
{
    int err;
    snd_pcm_sframes_t delayFrames = -1;;
    snd_pcm_status_t *status;
    snd_pcm_status_alloca(&status);
    
    if ((err = snd_pcm_status(handle, status)) < 0) {
        printf("Error getting pcm audio status\n");
    } else {
        delayFrames = snd_pcm_status_get_delay(status);
    }
    
    return delayFrames;
}

// XXX  - and the bytesPerSample, and the number of channels. These should all be global variables
// XXX - need to create a buffer with silence that I can just write from
static int initializeSilentBuffer(int silentTimeMs) 
{
    // get number of sample for silent time
    // bytes to frames, frames per second...
    int nSamples = (frameRate * nChannels * silentTimeMs)/1000;
    silentBuffer = (unsigned char *)malloc(nSamples * bytesPerSample);
    if (!silentBuffer) {
        return ERR_PCM_MEMORY;
    }
    snd_pcm_format_set_silence(SND_PCM_FORMAT_U8, silentBuffer, nSamples);
    return 0;
}

static int fillBufferWithSilence(int silentBytes, int *reInit) 
{
    return fillBufferWithSound(silentBuffer, silentBytes, reInit);
}
 

static int fillBufferWithSound(unsigned char *soundBuffer, int soundBytes, int *reInit)
{
    int nFramesWritten = 0;
    int err;
    
    if (reInit) {
        *reInit = 0;
    }
    int nFramesToWrite = soundBytes/(bytesPerSample*nChannels);
    while (nFramesToWrite > 0) {
        nFramesWritten = 0;
        err = snd_pcm_writei(handle, soundBuffer, nFramesToWrite);
        if (err < 0) {
            printf("Error writing to sound buffer, error is %s\n", strerror(err));
            if (xrun_recovery(handle, err) < 0) {
                return err;
            }
            if (reInit) {
                *reInit = 1;
            } 
            break;
        } else {
            nFramesWritten = err;
        }
        nFramesToWrite -= nFramesWritten;
    }
    
    return nFramesWritten * bytesPerSample * nChannels;
}

/*
 *   Underrun and suspend recovery
 */
 
static int verbose = 1;
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (verbose)
        printf("stream recovery\n");
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);       /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}



/* 
// XXX how does snd_pcm_recover differ from the huge horrid thing from the examples?
*/
/*
static int pcmWrite() 
{
    snd_pcm_sframes_t frames;
    frames = snd_pcm_writei(handle, buffer, sizeof(buffer));
    if (frames < 0)
        frames = snd_pcm_recover(handle, frames, 0);
    if (frames < 0) {
        printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
        return ERR_PCM_WRITE;
    }
    if (frames > 0 && frames < (long)sizeof(buffer))
        printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
    }
    
    return SND_OK; // XXX define this
} 
*/

void pcmPlaybackStop() 
{
    snd_pcm_close(handle);
}


