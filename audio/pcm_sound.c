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

#include "pcm_config.h"
#include "pcm_utils.h"

// but typically I get a callback which is called from a different thread. See how alsa
// does this. 
// But wait, let's do the wait method...

#define REQUESTED_SAMPLE_RATE 44100
//#define REQUESTED_BUFFER_SIZE_US 500000
#define REQUESTED_BUFFER_SIZE_US 100000
//#define REQUESTED_PERIOD_SIZE_US 100000
#define REQUESTED_PERIOD_SIZE_US  20000

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

static int bytesPerSample = 2; 
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
    if (sound) {
        pcmPlaySound(sound->data, sound->datalen, 1000*60/freqBPS);
    } else {
        printf("No heartbeat sound to play\n");
    }
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
    
    struct mq_attr attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.mq_flags = 0;
    attrs.mq_maxmsg = 10;
    attrs.mq_msgsize = sizeof(PcmSound);
    
    printf("creating message queue with attrs!\n");
    
//    mq_unlink("/Pulse_PCM_MQ");

    if ((commandmq = mq_open("/Pulse_PCM_MQ", O_RDWR|O_CREAT, S_IRWXU | S_IRWXG, &attrs)) < 0) { 
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
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        
        int playbackStartDeltaS  = freqMs/1000;
        int playbackStartMs = freqMs - (playbackStartDeltaS * 1000);
        int playbackStartNs = currentTime.tv_nsec + (playbackStartMs * 1000000);
        if (playbackStartNs < currentTime.tv_nsec) {  // do we ever actually wrap? check the resolution
            printf("create pcm sound wraps!");
            // XXX check can wrap happen
        } else if (playbackStartNs > 1000000000) {
            printf("adding a second, subtracting ns\n");
            playbackStartDeltaS++;
            playbackStartNs -= 1000000000;
        }
        
        sound->startTime.tv_sec  = currentTime.tv_sec + playbackStartDeltaS;
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
//    if ((err = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_NONINTERLEAVED)) < 0) {
        printf("Could not set raw interleaved access: %s\n", snd_strerror(err));
        return err;
    }

//    if ((err = snd_pcm_hw_params_set_format(handle, hwParams, SND_PCM_FORMAT_U8 )) < 0) {
    if ((err = snd_pcm_hw_params_set_format(handle, hwParams, SND_PCM_FORMAT_S16_LE )) < 0) {
        printf("Could not sent sound format to 16bit: %s\n", snd_strerror(err));
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
    
    if ((err = snd_pcm_hw_params_set_buffer_time_near(handle, hwParams, &requestedBufferSize, &dir)) < 0) {
        printf("Could not set buffer time: %s\n", snd_strerror(err));
        return err;
    }// XXX check actual buffer size here
    printf("Init: requested buffersize is %d usec\n", requestedBufferSize);
    if ((err = snd_pcm_hw_params_get_buffer_size(hwParams, &bufferSize)) < 0) {
        printf("Could not get buffer size: %s\n", snd_strerror(err));
        return err;
    }
    printf("Init: Actual buffer size is %d frames\n", (int)bufferSize);
    // XXX TODO - since we're now using 16 bit samples, I have to care about endianness
    // Fortunately, since WAV files appear to be little endian and this processor is little-endian,
    // all should be well. But.
    
    unsigned int requestedPeriodSize = requestedBufferSize/5;
    printf("Init: Requested Period Size is %d\n", requestedPeriodSize);
    if ((err = snd_pcm_hw_params_set_period_time_near(handle, hwParams, &requestedPeriodSize, &dir)) < 0) {
        printf("Could not set period size: %s\n", snd_strerror(err));
        return err;
    } // XXX check actual period size returned
    printf("Init: Actual Period time is %d usec\n", requestedPeriodSize);
    
    periodSize = (REQUESTED_PERIOD_SIZE_US * rrate)/ 1000; // XXX check this...
    if ((err = snd_pcm_hw_params_get_period_size(hwParams, &periodSize, &dir)) < 0) {
        printf("Could not get period size: %s\n", snd_strerror(err));
        return err;
    }
    printf("Init: PeriodSize is %d frames\n", (int)periodSize);
    bytesPerPeriod = periodSize*bytesPerSample*nChannels;
    printf("Init: Bytes per period is %d\n", bytesPerPeriod);
    
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
    int nPeriods = bufferSize/periodSize;
    if (nPeriods > 1) nPeriods++;
    if ((err = snd_pcm_sw_params_set_start_threshold(handle, swParams,  0U)) < 0) {
//    if ((err = snd_pcm_sw_params_set_start_threshold(handle, swParams,  nPeriods * periodSize)) < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least periodSize samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    if ((err = snd_pcm_sw_params_set_avail_min(handle, swParams, periodSize )) < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }

    
    /*
    if ((err = snd_pcm_sw_params_set_avail_min (playback_handle, sw_params, 4096)) < 0) {
        printf (stderr, "cannot set minimum available count (%s)\n", snd_strerror (err));
    }
    */
    /*
    // trigger period events
    if ((err = snd_pcm_sw_params_set_period_event(handle, swParams, 1)) < 0) {
        printf("Unable to set period event: %s\n", snd_strerror(err));
        return err;
    }
    */
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
    printf("Have %d descriptors\n", pcmDescriptorsCount);
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
    int waitTime = -1;
    printf("Open state is %d\n", SND_PCM_STATE_OPEN);
    printf("Setup state is %d\n", SND_PCM_STATE_SETUP);
    printf("Prepared state is %d\n", SND_PCM_STATE_PREPARED);
    printf("Running state is %d\n", SND_PCM_STATE_RUNNING);
    printf("Xrun state is %d\n", SND_PCM_STATE_XRUN);
    printf("Draining state is %d\n", SND_PCM_STATE_DRAINING);
    printf("Paused state is %d\n", SND_PCM_STATE_PAUSED);
    printf("Suspended state is %d\n", SND_PCM_STATE_SUSPENDED);
    printf("Disconnected state is %d\n", SND_PCM_STATE_DISCONNECTED);
    while (1) {
        int snd_state = snd_pcm_state(handle);
//        printf("LOOP START: Sound state is %d\n", snd_state);
        if (snd_state == SND_PCM_STATE_PREPARED) {
/*            feedBuffer();
            feedBuffer();
            feedBuffer();
            feedBuffer();
            feedBuffer();
            snd_pcm_start(handle);
*/
        }
//        printf("LOOP START: Final sound state is %d\n", snd_state);
//        int waitTime = 1000;
        // XXX - from the sample code, it appears that POLL only works if you've 
        // done something to prime the pump and get the code into a running state
//        if (snd_pcm_state(handle) == SND_PCM_STATE_RUNNING) {
            struct timespec pollStartTime;
            clock_gettime(CLOCK_MONOTONIC, &pollStartTime);
            err = poll(ufds, pcmDescriptorsCount + 1, waitTime);
            if (err == 0) { // timeout, generate a new event
                if (lastSoundBuf) {
                    printf("Timeout, generating new event\n");
                    nextSoundBuf     = lastSoundBuf;
                    nextSoundLength  = lastSoundLength;
                    //waitTime         = lastSoundFreqMs; // XXX need to generate a poll wakeup here.
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
//                    printf("Receive poll event %d on message queue\n", mqEvent);
                    if (mqEvent & POLLIN) {
                        struct mq_attr attr;
                        mq_getattr(commandmq, &attr);
//                        printf("sound message message size is %ld, buffer size is %d\n", attr.mq_msgsize, sizeof(soundMsg));
                        int bytesRead = mq_receive(commandmq, (char *)(&soundMsg), sizeof(soundMsg), NULL);
//                        printf("Read %d bytes from message queue\n", bytesRead);
                        if (bytesRead == -1) {
                            printf("Warning: Message queue returns error, %s\n", strerror(errno));
                        } else if (bytesRead < sizeof(soundMsg)) {
                            printf("Received malformed sound message, %d bytes, discarding\n", bytesRead);
                        } else {
//                            printf("Buflen is %d\n", soundMsg.bufLen);
//                            printf("FreqMs is %d\n", soundMsg.freqMs);
//                               int bufLen;
//    struct timespec startTime;
//    int freqMs;
                            struct timespec currentTime;
                            clock_gettime(CLOCK_MONOTONIC, &currentTime);
//                            printf("Current time is %lds, %ldns\n", currentTime.tv_sec, currentTime.tv_nsec);
//                            printf("Message time is %lds, %ldns\n", soundMsg.startTime.tv_sec, soundMsg.startTime.tv_nsec);
                            if ((currentTime.tv_sec > soundMsg.startTime.tv_sec) || 
                                 ( (currentTime.tv_sec == soundMsg.startTime.tv_sec) && 
                                   (currentTime.tv_nsec > soundMsg.startTime.tv_nsec))) {
                                // This message has expired. Ignore it. 
                                printf("Received expired message, ignoring\n");  
                            } else {
                                printf("SOUND MESSAGE RECEIVED\n");
                                nextSoundBuf    = soundMsg.soundBuf;
                                nextSoundLength = soundMsg.bufLen;
                                /*
                                printf("Just going to fucking play the sound... or attempt to\n");
                                unsigned char *soundPtr = soundMsg.soundBuf;
                                unsigned char *periodBuffer = (unsigned char *)malloc(bytesPerPeriod);
                                snd_pcm_start(handle);
                                while (1) {
                                    if (soundPtr >= soundMsg.soundBuf + soundMsg.bufLen) {
                                        soundPtr = soundMsg.soundBuf;
                                    }
                                    // stuff period buffer
                                    int remainingSoundBytes = soundMsg.bufLen - (soundPtr - soundMsg.soundBuf);
                                    if (remainingSoundBytes >= bytesPerPeriod) {
                                        memcpy(periodBuffer, soundPtr, bytesPerPeriod);
                                        soundPtr += bytesPerPeriod;
                                    } else {
                                        int nFramesSilence = (bytesPerPeriod - remainingSoundBytes)/(bytesPerSample * nChannels);
                                        memcpy(periodBuffer, soundPtr, remainingSoundBytes); 
                                        snd_pcm_format_set_silence(SND_PCM_FORMAT_S16_LE, periodBuffer + remainingSoundBytes, nFramesSilence);
                                        soundPtr = soundMsg.soundBuf;
                                    }
                                    int framesWritten = snd_pcm_writei(handle, periodBuffer, periodSize);
                                    if (framesWritten < 0) {
                                        printf("Error writing frame, %s\n", strerror(framesWritten));
                                        printf("Epipe error %d, estrpipe error %d, ebfd error %d\n", EPIPE, ESTRPIPE, EBADFD);
                                        printf("Eagain error %d, eio  %d, einval error %d\n", EAGAIN, EIO, EINVAL);
                                        sleep(1);
                                    }
                                    if (framesWritten != periodSize) {
                                        printf("Attempted to write %d frames, actually wrote %d frames\n", (int)periodSize, framesWritten);
                                    }
                                    int snd_state = snd_pcm_state(handle); 
                                    printf("Sound system state is %d\n", snd_state);
                                    if (snd_state == SND_PCM_STATE_XRUN) {
                                        printf("XRUN, exiting\n");
                                        return NULL;
                                    }
                                }*/
                                
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
                                printf("next sound delay ms is %d\n", nextSoundDelayMs);
                                snd_pcm_sframes_t delayFrames = getAudioDelay();
                                int delayMs = delayFrames*1000/frameRate; // XXXX check this
                                printf("Audio buffer delayMs is %d\n", delayMs);
                                nextSoundTimeBytes = ((nextSoundDelayMs - delayMs) * frameRate * nChannels * 2 )/1000; // XXX 2 bytes per sample
                                nextSoundTimeBytes = MAX(0, nextSoundTimeBytes);
                                nextSoundTimeBytes = (nextSoundTimeBytes >> 4) << 4;
                                //waitTime = soundMsg.freqMs;
                                lastSoundBuf    = nextSoundBuf;
                                lastSoundLength = nextSoundLength;
                                lastSoundFreqMs = soundMsg.freqMs;
                            }
                        }
                    } 
                    ufds[0].revents = 0;
                } else {
//                    printf("!!Possible sound system event\n");
                    unsigned short revents;
                    snd_pcm_poll_descriptors_revents(handle, ufds+1, pcmDescriptorsCount, &revents);
                    if (revents & POLLERR) {
                        printf("Warning: PCM poll returns IO error\n");
                        if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN ||
                            snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED) {
                            err = snd_pcm_state(handle) == SND_PCM_STATE_XRUN ? -EPIPE : -ESTRPIPE;
                            printf("PCM state is %d\n", snd_pcm_state(handle));
                            if (xrun_recovery(handle, err) < 0) {
                                printf("Cannot recover from error %s\n", snd_strerror(err));
                                exit(EXIT_FAILURE);// XXX not what I want...
                            }
                        }
                    }
                    if (revents & POLLOUT) {
                        printf("Sound system requires feeding\n");
                        feedBuffer();
                    }
                    /*
                    struct timespec currentTime;
                    clock_gettime(CLOCK_MONOTONIC, &currentTime);
                    int deltaS = currentTime.tv_sec - pollStartTime.tv_sec;
                    //printf("deltaS is %d\n", deltaS);
                    //printf("detalNs is %ld\n", (long)(currentTime.tv_nsec - pollStartTime.tv_nsec));
                    if (currentTime.tv_nsec < pollStartTime.tv_nsec) {
                        deltaS -= 1;
                        currentTime.tv_nsec += 1000000000;
                    }
                    long deltaNs = currentTime.tv_nsec - pollStartTime.tv_nsec;
                    //printf("new deltaNS is %ld\n", deltaNs);
                    waitTime = deltaS*1000 + deltaNs/1000000;
                    printf("waittime is %d ms\n", waitTime);
                    assert(waitTime >= 0); 
                    */
                }
            }
//        }
    }
    return NULL;
        
}

// write one period to the buffer
static int feedBuffer(void) {
    int reInit = 0;
    int totalBytesFilled = 0;
    /*
    // for the moment... just write a period of silence....
    int bytesToWrite = bytesPerPeriod;
    
    int bytesWritten = fillBufferWithSilence(bytesToWrite, NULL);
    if (bytesWritten != bytesToWrite) {
        printf("unexpected - did not write full period. %d bytes expected, %d bytes written\n", bytesToWrite, bytesWritten);
    }
    */
    
    
    
//    printf("Bytes per period is %d\n", bytesPerPeriod);
    
    // if we're currently playing a sound, attempt to finish it
    if (currentSoundBuf) {
        assert(currentSoundBufPtr);
        int bytesToWrite = MIN(currentSoundLength - (currentSoundBufPtr - currentSoundBuf), bytesPerPeriod);
        printf("WRITE SOUND DATA 1\n");
        int bytesFilled = fillBufferWithSound(currentSoundBufPtr, bytesToWrite, &reInit);
        if (bytesFilled >= 0) {
            printf("Wrote %d bytes to sound buffer\n", bytesFilled);
            totalBytesFilled += bytesFilled;
            currentSoundBufPtr += bytesFilled;
            if (currentSoundBufPtr - currentSoundBuf >= currentSoundLength) { 
                // we've finished the current sound. Reset variables
                currentSoundBuf    = NULL;
                currentSoundBufPtr = NULL;
                currentSoundLength = 0;
            }
            if (bytesFilled != bytesToWrite) {
                printf("Primary write: Did not write as many bytes as expected: %d written, vs %d expected\n", bytesFilled, bytesToWrite);
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
        assert(!currentSoundBuf); // XXX This assertion fails if we couldn't write all the bytes above...
        int bytesFilled = 0;
        int bytesToWrite = 0;
        if (!nextSoundBuf) {
            // no next sound - fill the rest of the thing with silence...
            bytesToWrite = bytesPerPeriod - totalBytesFilled;
            printf("Silence fill 1\n");
            bytesFilled = fillBufferWithSilence(bytesToWrite, &reInit);
        } else if (nextSoundTimeBytes > totalBytesFilled) {
            // if next sound starts after the end of what we've got, fill the bits between
            // with silence.
            bytesToWrite = MIN(bytesPerPeriod-totalBytesFilled, nextSoundTimeBytes - totalBytesFilled);
            printf("Silence fill 2\n");
            bytesFilled = fillBufferWithSilence(bytesToWrite, &reInit);
        } 
        if (bytesFilled >= 0) {
            totalBytesFilled += bytesFilled;
            if (bytesFilled != bytesToWrite) {
                printf("Fill silence: Did not write as many bytes as expected: %d written, vs %d expected\n", bytesFilled, bytesToWrite);
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
            printf("WRITE SOUND DATA 2\n");
            bytesFilled = fillBufferWithSound(currentSoundBuf, bytesToWrite, &reInit);
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
                    printf("2ndary Fill sound: Did not write as many bytes as expected: %d written, vs %d expected\n", bytesFilled, bytesToWrite);
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
            printf("nextSoundTimeBytes is %d\n", nextSoundTimeBytes);
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
    
    printf("Audio delay is %ld frames\n", (long)delayFrames);
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
    snd_pcm_format_set_silence(SND_PCM_FORMAT_S16_LE, silentBuffer, nSamples);
    return 0;
}

static int fillBufferWithSilence(int silentBytes, int *reInit) 
{
    printf("Writing some silence...\n");
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
        printf("writing %d bytes, %d frames\n", soundBytes, nFramesToWrite);
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
            printf("Wrote %d frames\n", err);
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




