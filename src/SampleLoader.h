/***** SampleLoader.h *****/

#include <stdio.h>
#include <sndfile.h>				// to load audio files
#include <stdlib.h>

// Load samples from file
int getSamples(const char* file, float *buf, int channel, int startFrame, int endFrame)
{
	SNDFILE *sndfile ;
	SF_INFO sfinfo ;
	sfinfo.format = 0;
	if (!(sndfile = sf_open (file, SFM_READ, &sfinfo))) {
		fprintf(stderr, "Couldn't open file %s: %s\n", file, sf_strerror(sndfile));
		return 1;
	}

	int numChannelsInFile = sfinfo.channels;
	if(numChannelsInFile < channel+1)
	{
		fprintf(stderr, "Error: %s doesn't contain requested channel", file);
		return 1;
	}
    
    int frameLen = endFrame-startFrame;
    
    if(frameLen <= 0 || startFrame < 0 || endFrame <= 0 || endFrame > sfinfo.frames)
	{
	    fprintf(stderr, "Error: %s invalid frame range requested\n", file);
		return 1;
	}
    
    sf_seek(sndfile,startFrame,SEEK_SET);
    
    float* tempBuf = (float*)malloc(sizeof(float) * frameLen * numChannelsInFile);
    
	int subformat = sfinfo.format & SF_FORMAT_SUBMASK;
	int readcount = sf_read_float(sndfile, tempBuf, frameLen*numChannelsInFile); //FIXME

	// Pad with zeros in case we couldn't read whole file
	for(int k = readcount; k <frameLen*numChannelsInFile; k++)
		tempBuf[k] = 0;

	if (subformat == SF_FORMAT_FLOAT || subformat == SF_FORMAT_DOUBLE) {
		double	scale ;
		int 	m ;

		sf_command (sndfile, SFC_CALC_SIGNAL_MAX, &scale, sizeof (scale)) ;
		if (scale < 1e-10)
			scale = 1.0 ;
		else
			scale = 32700.0 / scale ;
		printf("File samples scale = %f\n", scale);

		for (m = 0; m < frameLen; m++)
			tempBuf[m] *= scale;
	}
	
	for(int n=0;n<frameLen;n++)
	    buf[n] = tempBuf[n*numChannelsInFile+channel];

	sf_close(sndfile);

	return 0;
}

int getNumChannels(const char* file) {
    
    SNDFILE *sndfile ;
	SF_INFO sfinfo ;
	sfinfo.format = 0;
	if (!(sndfile = sf_open (file, SFM_READ, &sfinfo))) {
		fprintf(stderr, "Couldn't open file i %s: %s\n", file, sf_strerror(sndfile));
		return -1;
	}

	return sfinfo.channels;
}

int getNumFrames(const char* file) {
    
    SNDFILE *sndfile ;
	SF_INFO sfinfo ;
	sfinfo.format = 0;
	if (!(sndfile = sf_open (file, SFM_READ, &sfinfo))) {
		fprintf(stderr, "Couldn't open file %s : %s\n", file, sf_strerror(sndfile));
		return -1;
	}

	return sfinfo.frames;
}

