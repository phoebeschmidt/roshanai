#ifndef PCM_SOUND_H
#define PCM_SOUND_H

int pcmPlaySound(unsigned char* pcmSoundBuf, int pcmBufLen, unsigned int freqMs);
void pcmPlayHeartBeat(unsigned int freqBPS);
int pcmPlaybackInit(void);
int pcmPlaybackStop(void);

#endif //PCM_SOUND_H