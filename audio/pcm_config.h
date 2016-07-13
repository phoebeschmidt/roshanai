#ifndef PCM_CONFIG_H
#define PCM_CONFIG_H

#define HB_CONFIG_FILE "hbconfig.conf"

typedef struct {
    char          *filename;
    unsigned char *data;
    int            datalen;
    int            validFreqStart;
    int            validFreqEnd;
} HbData;

int PcmConfig_Read();
HbData* PcmConfig_getHbSound(int freq);

#endif // PCM_CONFIG_H

