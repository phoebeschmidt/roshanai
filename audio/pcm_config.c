#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pcm_config.h"
#include "pcm_utils.h"

// Read configuration file and load the various heartbeat sounds
// file format is defined as: <filename>, validRateStart, validRateEnd
// Both valid rate start and valid rate end are integers

typedef struct __LinkedList{
    struct __LinkedList *next;
    void *data;
} LinkedList;

static LinkedList *hbSoundList = NULL;


static LinkedList* LinkedListAdd(LinkedList *list, void *data)
{
    LinkedList *newList = (LinkedList *)malloc(sizeof(LinkedList));
    newList->data = data;
    newList->next = NULL;

    if (!list) {
        return newList;
    }
    
    LinkedList *tail;
    while(list) {
       tail = list; 
       list = list->next;
    }
    
    tail->next = newList;
    
    return list;
}


int PcmConfig_Read()
{
    char strBuf[255];
    FILE *configFile = fopen(HB_CONFIG_FILE, "r");
    if (configFile <=0) {
        printf("Error reading file %s: %s\n", HB_CONFIG_FILE, strerror(errno));
        return ERR_FILE_NOT_FOUND;
    }
    
    while (fgets(strBuf, 255, configFile)) {
        char *strPtr = strBuf;
        // skip past space
        while (*strPtr != '\0' && isspace(*strPtr)) {
            strPtr++;
        }
        if (*strPtr == '#') {
            continue;
        }
        
        char *tokStr = strPtr;
        char *filename = NULL;
        int  validFreqStart = 0;
        int  validFreqEnd   = 0;
        while ((strPtr = strtok(tokStr, ",")) != NULL) {
            tokStr = NULL;
            if (!filename) {
                filename = strPtr;
                continue;
            }
            if (!validFreqStart) {
                validFreqStart = atoi(strPtr);
                if (validFreqStart == -1) {
                    printf("Invalid frequency %s in config file\n", strPtr);
                    break;
                } 
                continue;
            }
            
            if (!validFreqEnd) {
                validFreqEnd = atoi(strPtr);
                if (validFreqEnd == -1) {
                    printf("Invalid frequency %s in config file\n", strPtr);
                    break;
                }
                // at this point, we should have all valid values for this entry.
                // Add to linked list
                HbData *data = (HbData *)malloc(sizeof(HbData));
                if (!data) {
                    printf("Error out of memory\n");
                    break;
                }
                data->filename = filename;
                data->data = NULL;
                data->validFreqStart = validFreqStart;
                data->validFreqEnd   = validFreqEnd;
                hbSoundList = LinkedListAdd(hbSoundList, data);
                continue;
            }
        }
        
        // at this point we should have our list of heartbeat sounds. Now read them in
        LinkedList *entry = hbSoundList;
        while (entry) {
            HbData *sound = (HbData *)(entry->data);
            if (sound && sound->filename && !sound->data) {
                FILE *dataFile = fopen(sound->filename, "rb");
                if (!dataFile) {
                    printf("Could not open heartbeat data file %s\n", sound->filename);
                    goto hb_sounds_iterate;
                }
                int fd = fileno(dataFile);
                if (fd < 0) {
                    printf("Heartbeat data file invalid %s\n", sound->filename);
                    goto hb_sounds_iterate;
                }
                struct stat sb;
                if (fstat(fd, &sb) != 0) {
                    printf("Could not stat file %s\n", sound->filename);
                }
                int filesize = sb.st_size;
                if (filesize <= 0) {
                    printf("Heartbeat data file size invalid %s, %d bytes\n", sound->filename, filesize);
                    goto hb_sounds_iterate;
                }
                sound->datalen = filesize;
                char *filedata = (char *)malloc(filesize);
                if (!filedata) {
                    printf("Could not allocate memory (%d bytes) for file %s\n", filesize, sound->filename);
                    goto hb_sounds_iterate;
                }
                
                int elemRead = fread(filedata, filesize, 1, dataFile);
                if (elemRead != 1) {
                    printf("Error reading data from file %s\n", sound->filename);
                    free(filedata);
                    goto hb_sounds_iterate;
                }
                
                entry->data = filedata;
hb_sounds_iterate:
                if (dataFile) {
                    fclose(dataFile);
                }
            }
            entry = entry->next;
        }
    }
    
    fclose(configFile);
    return 0;
}

// Iterate list of heartbeat sounds, looking for the right one
// Note that we return the *last* matching sound, rather than the first one.
// This allows me to easily set defaults in the config file
HbData* PcmConfig_getHbSound(int freq) 
{
    LinkedList *entry = hbSoundList;
    HbData *bestSound = NULL;
    while (entry) {
        HbData *sound = (HbData *)(entry->data);
        if (sound && sound->validFreqStart <= freq && sound->validFreqEnd >= freq) {
            bestSound = sound;
        }
        entry = entry->next;
    }
    return bestSound;
}
