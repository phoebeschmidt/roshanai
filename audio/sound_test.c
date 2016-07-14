#include <unistd.h>

#include "pcm_sound.h"

int main(int argc, char **argv)
{
	pcmPlaybackInit();
	pcmPlayHeartBeat(60);
	sleep(5*60);
	pcmPlaybackStop();
	
	return 0;
}
