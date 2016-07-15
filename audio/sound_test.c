#include <unistd.h>
#include <stdio.h>

#include "pcm_sound.h"

int main(int argc, char **argv)
{
	if (pcmPlaybackInit() < 0) {
	    printf("Failure to initialize sound system, aborting\n");
	    return -1;
	}
	pcmPlayHeartBeat(60);
	sleep(5*60);
	pcmPlaybackStop();
	
	return 0;
}
