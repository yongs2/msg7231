/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Yann DIORCET
Belledonne Communications SARL, All rights reserved.
yann.diorcet@belledonne-communications.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <mediastreamer2/msfilter.h>

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>

// msacm.h 에 있어야 하는 데...
#define ACMERR_BASE							512
#define ACMERR_NOTPOSSIBLE					(ACMERR_BASE + 0)
#define ACMERR_BUSY							(ACMERR_BASE + 1)
#define ACMERR_UNPREPARED					(ACMERR_BASE + 2)
#define ACMERR_CANCELED						(ACMERR_BASE + 3)

#define ACM_STREAMOPENF_QUERY				0x00000001
#define ACM_STREAMOPENF_ASYNC				0x00000002
#define ACM_STREAMOPENF_NONREALTIME			0x00000004
#define ACM_FORMATSUGGESTF_WFORMATTAG		0x00010000L

#define ACMDRIVERDETAILS_SUPPORTF_CODEC		0x00000001L
#define ACMDRIVERDETAILS_SUPPORTF_CONVERTER	0x00000002L
#define ACMDRIVERDETAILS_SUPPORTF_FILTER	0x00000004L
#define ACMDRIVERDETAILS_SUPPORTF_HARDWARE	0x00000008L
#define ACMDRIVERDETAILS_SUPPORTF_ASYNC		0x00000010L
#define ACMDRIVERDETAILS_SUPPORTF_LOCAL		0x40000000L
#define ACMDRIVERDETAILS_SUPPORTF_DISABLED	0x80000000L

#define ACM_DRIVERENUMF_NOLOCAL				0x40000000L
#define ACM_DRIVERENUMF_DISABLED			0x80000000L

#define ACM_METRIC_COUNT_DRIVERS			1
#define ACM_METRIC_COUNT_CODECS				2
#define ACM_METRIC_COUNT_CONVERTERS			3
#define ACM_METRIC_COUNT_FILTERS			4
#define ACM_METRIC_COUNT_DISABLED			5
#define ACM_METRIC_COUNT_HARDWARE			6
#define ACM_METRIC_COUNT_LOCAL_DRIVERS		20
#define ACM_METRIC_COUNT_LOCAL_CODECS		21
#define ACM_METRIC_COUNT_LOCAL_CONVERTERS	22
#define ACM_METRIC_COUNT_LOCAL_FILTERS		23
#define ACM_METRIC_COUNT_LOCAL_DISABLED		24
#define ACM_METRIC_HARDWARE_WAVE_INPUT		30
#define ACM_METRIC_HARDWARE_WAVE_OUTPUT		31
#define ACM_METRIC_MAX_SIZE_FORMAT			50
#define ACM_METRIC_MAX_SIZE_FILTER			51
#define ACM_METRIC_DRIVER_SUPPORT			100
#define ACM_METRIC_DRIVER_PRIORITY			101


//#pragma comment(lib, "msacm32.lib")

#define G723_MAX_SIZE		24

typedef struct 
{
	WAVEFORMATEX wf;
	BYTE extra[10];
} WAVEFORMATMSG723;

WAVEFORMATMSG723 g7231format = {
	{ WAVE_FORMAT_MSG723, 1, 8000, 800, 24, 0, 10 },
	{ 2, 0, 0xce, 0x9a, 0x32, 0xf7, 0xa2, 0xae, 0xde, 0xac }
};

static const unsigned G7231PacketSizes[4] = { 24, 20, 4, 1 };

void SetPCMFormat(WAVEFORMATEX *pstWaveFormatEx, unsigned numChannels, unsigned sampleRate, unsigned bitsPerSample)
{
	if(pstWaveFormatEx == NULL)
		return;
	
	pstWaveFormatEx->wFormatTag = WAVE_FORMAT_PCM;
	pstWaveFormatEx->nChannels = (WORD)numChannels;
	pstWaveFormatEx->nSamplesPerSec = sampleRate;
	pstWaveFormatEx->wBitsPerSample = (WORD)bitsPerSample;
	pstWaveFormatEx->nBlockAlign = (WORD)(numChannels*(bitsPerSample+7)/8);
	pstWaveFormatEx->nAvgBytesPerSec = pstWaveFormatEx->nSamplesPerSec * pstWaveFormatEx->nBlockAlign;
	pstWaveFormatEx->cbSize = 0;
}

const char *StrMmResult(MMRESULT result)
{
	switch(result)
	{
		case MMSYSERR_NOERROR :		return "Success";
#define MAKE_CODE_STR(_VALUE_)	case _VALUE_ : return #_VALUE_;
		MAKE_CODE_STR(ACMERR_NOTPOSSIBLE)
		MAKE_CODE_STR(ACMERR_BUSY)
		MAKE_CODE_STR(ACMERR_UNPREPARED)
		
		MAKE_CODE_STR(MMSYSERR_ERROR)
		MAKE_CODE_STR(MMSYSERR_BADDEVICEID)
		MAKE_CODE_STR(MMSYSERR_NOTENABLED)
		MAKE_CODE_STR(MMSYSERR_ALLOCATED)
		MAKE_CODE_STR(MMSYSERR_INVALHANDLE)
		MAKE_CODE_STR(MMSYSERR_NODRIVER)
		MAKE_CODE_STR(MMSYSERR_NOMEM)
		MAKE_CODE_STR(MMSYSERR_NOTSUPPORTED)
		MAKE_CODE_STR(MMSYSERR_BADERRNUM)
		MAKE_CODE_STR(MMSYSERR_INVALFLAG)
		MAKE_CODE_STR(MMSYSERR_INVALPARAM)
		MAKE_CODE_STR(MMSYSERR_HANDLEBUSY)
		MAKE_CODE_STR(MMSYSERR_INVALIDALIAS)
		MAKE_CODE_STR(MMSYSERR_BADDB)
		MAKE_CODE_STR(MMSYSERR_KEYNOTFOUND)
		MAKE_CODE_STR(MMSYSERR_READERROR)
		MAKE_CODE_STR(MMSYSERR_WRITEERROR)
		MAKE_CODE_STR(MMSYSERR_DELETEERROR)
		MAKE_CODE_STR(MMSYSERR_VALNOTFOUND)
		MAKE_CODE_STR(MMSYSERR_LASTERROR)
#undef MAKE_CODE_STR
		default :					return "Unknown Error.";
	}
}

static void dec_init(MSFilter *f)
{
	WAVEFORMATEX	*psrcFormat = NULL;
	WAVEFORMATEX	*pdstFormat = NULL;
	HACMSTREAM		hStream;
	MMRESULT		result = 0;
	
	psrcFormat = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATMSG723));
	pdstFormat = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
	
	memcpy(psrcFormat, &g7231format, sizeof(WAVEFORMATMSG723));
	SetPCMFormat(pdstFormat, 1, 8000, 16);
	
	result = acmStreamOpen(&hStream, 
				NULL, // driver
				psrcFormat, // source format
				pdstFormat, // destination format
				(LPWAVEFILTER)NULL, // no filter
				(DWORD_PTR)0, // no callback
				(DWORD_PTR)0, // instance data (not used)
				0); // flags
	
	if (result == 0)
	{
		f->data = (void *)hStream;
		ms_message("%s, acmStreamOpen(0x%p)=%d..........", __func__, (void *)hStream, result);
	}
	else
	{
		ms_message("%s, acmStreamOpen=%d.......FAIL", __func__, result);
	}
	free(psrcFormat);
	free(pdstFormat);
}

static void dec_uninit(MSFilter *f)
{
	HACMSTREAM hStream = (HACMSTREAM)f->data;
	if (hStream != NULL)
		acmStreamClose(hStream, 0);
}

static void dec_process(MSFilter *f)
{
	static const int nsamples = 240;
	mblk_t			*im, *om;
	HACMSTREAM		hStream = (HACMSTREAM)f->data;
	static BYTE		frameBuffer[G723_MAX_SIZE];
	ACMSTREAMHEADER	header;
	MMRESULT		result;
	
	if(hStream == NULL)
	{
		ms_message("%s, hStream is NULL...", __func__);
		return;
	}
	
	while ((im = ms_queue_get(f->inputs[0])) != NULL)
	{
		int sz = msgdsize(im);
		if (sz < 2)
		{
			freemsg(im);
			continue;
		}
		
		om = allocb(nsamples * 2, 0);
		
		memset(&header, 0, sizeof(header));
		header.cbStruct = sizeof(header);
		
		header.pbSrc = (BYTE *)im->b_rptr;
		header.cbSrcLength = G7231PacketSizes[((unsigned char *)im->b_rptr)[0]&3];
		if(header.cbSrcLength < G723_MAX_SIZE)
		{
			memcpy(frameBuffer, header.pbSrc, header.cbSrcLength);
			header.cbSrcLength = G723_MAX_SIZE;
			header.pbSrc = frameBuffer;
		}
		
		header.pbDst = (unsigned char *)om->b_wptr;
		header.cbDstLength = nsamples * 2;
		
		result = acmStreamPrepareHeader(hStream, &header, 0);
		if(result != 0)
		{
			ms_message("%s, acmStreamPrepareHeader FAIL=%d(%s)...", __func__, (int)result, StrMmResult(result));
			freemsg(im);
			freemsg(om);
			continue;
		}
		
		result = acmStreamConvert(hStream, &header, 0);
		if(result == 0)
		{
			om->b_wptr += header.cbDstLength;
			ms_queue_put(f->outputs[0], om);
		}
		else
		{	// Error
			ms_message("%s, acmStreamConvert FAIL=%d(%s)...", __func__, (int)result, StrMmResult(result));
			freemsg(om);
		}
		
		result = acmStreamUnprepareHeader(hStream, &header,0);
		if(result != 0)
		{	// Error
			ms_message("%s, acmStreamUnprepareHeader(0x%p)=%d(%s)...", __func__, (void *)hStream, (int)result, StrMmResult(result));
		}
		
		freemsg(im);
	} // end of while
}

MSFilterDesc g7231_dec_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = "MSG7231Dec",
	.text = "G.723.1 decoder",
	.category = MS_FILTER_DECODER,
	.enc_fmt = "G723",
	.ninputs = 1,
	.noutputs = 1,
	.init = dec_init,
	.process = dec_process,
	.uninit = dec_uninit
};

typedef struct EncState
{
	void			*enc;
	MSBufferizer	*mb;
	uint32_t		ts;
	int				ptime;
} EncState;

static void enc_init(MSFilter *f)
{
	WAVEFORMATEX	*psrcFormat = NULL;
	WAVEFORMATEX	*pdstFormat = NULL;
	HACMSTREAM		hStream;
	MMRESULT		result = 0;
	
	EncState *s = ms_new0(EncState, 1);
	s->mb = ms_bufferizer_new();
	s->ts = 0;
	s->ptime = 30;
	s->enc = NULL;
	f->data = s;
	
	// 반드시 메모리 할당으로 WAVEFORMATEX 를 생성해야 한다. Local Memory로 사용하면 acmStreamOpen 호출 실패한다.
	psrcFormat = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
	pdstFormat = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATMSG723));
	
	SetPCMFormat(psrcFormat, 1, 8000, 16);
	memcpy(pdstFormat, &g7231format, sizeof(WAVEFORMATMSG723));
	
	result = acmStreamOpen(&hStream,
				NULL, // driver
				psrcFormat, // source format
				pdstFormat, // destination format
				(LPWAVEFILTER)NULL, // no filter
				(DWORD_PTR)0, // no callback
				(DWORD_PTR)0, // instance data (not used)
				0); // flags
	if (result == 0)
	{
		s->enc = (void *)hStream;
		ms_message("%s, acmStreamOpen()=0x%p..........", __func__, (void *)hStream);
	}
	else
	{
		
		ms_message("%s, acmStreamOpen=%d(%s).......FAIL", __func__, result, StrMmResult(result));
	}
	free(psrcFormat);
	free(pdstFormat);
}

static void enc_uninit(MSFilter *f)
{
	EncState *s = (EncState*) f->data;
	ms_bufferizer_destroy(s->mb);
	ms_free(s);
}

static void enc_process(MSFilter *f)
{
	EncState 		*s = (EncState *)f->data;
	unsigned int	unitary_buff_size = sizeof (int16_t)*80*3;	// 10 ms 당 8Khz 이므로 80bytes, 30 ms 단위면 240 bytes 임. LinearPCM 는 int16_t 임. = 480
	unsigned int	buff_size = unitary_buff_size * s->ptime / 30;	// 480
	mblk_t			*im, *om;
	HACMSTREAM		hStream = NULL;
	ACMSTREAMHEADER	header;
	MMRESULT		result;
	uint8_t			tmp[G723_MAX_SIZE];
	int16_t			samples[buff_size / sizeof(int16_t)];
	int				nRead = 0, nSize = 0;
	
	hStream = (HACMSTREAM)s->enc;
	
	while ((im = ms_queue_get(f->inputs[0])) != NULL)
	{
		nRead = im->b_wptr - im->b_rptr;
		ms_bufferizer_put(s->mb, im);
		ms_message("%s, ms_queue_get.size=%d, buf->size=%d", __func__, nRead, s->mb->size);
	}
	if(hStream == NULL)
	{
		return;
	}
	while(ms_bufferizer_get_avail(s->mb) >= buff_size)
	{
		om = allocb(G723_MAX_SIZE + 1, 0);
		nRead = ms_bufferizer_read(s->mb, (uint8_t*) samples, buff_size);
		ms_message("%s, ms_bufferizer_read(%d)=%d, sizeof(samples)=%d, s->mb->size=%d", __func__, buff_size, nRead, sizeof(samples), s->mb->size);
		
		memset(&header, 0, sizeof(header));
		header.cbStruct = sizeof(header);
		
		header.pbSrc = (unsigned char *)samples;
		header.cbSrcLength = buff_size;
		header.pbDst = tmp;
		header.cbDstLength = sizeof(tmp);
		
		result = acmStreamPrepareHeader(hStream, &header, 0);
		if(result != 0)
		{
			freemsg(om);
			continue;
		}
		
		result = acmStreamConvert(hStream, &header, 0);
		if(result == 0)
		{
			nSize = G7231PacketSizes[((unsigned char *)tmp)[0]&3];
			memcpy(om->b_wptr, &tmp, nSize);
			om->b_wptr += nSize;
			mblk_set_timestamp_info(om, s->ts);
			ms_queue_put(f->outputs[0], om);
			s->ts += buff_size / sizeof (int16_t)/*sizeof(buf)/2*/;
		}
		else
		{	// error
			ms_message("%s, acmStreamConvert(0x%p)=%d(%s)...", __func__, (void *)hStream, (int)result, StrMmResult(result));
			freemsg(om);
		}
		
		result = acmStreamUnprepareHeader(hStream, &header,0);
		if(result != 0)
		{	// Error
			ms_message("%s, acmStreamUnprepareHeader(0x%p)=%d(%s)...", __func__, (void *)hStream, (int)result, StrMmResult(result));
		}
	}
}

static void enc_postprocess(MSFilter *f)
{
	EncState *s = (EncState*) f->data;
	
	HACMSTREAM hStream = (HACMSTREAM)s->enc;
	if (hStream != NULL)
		acmStreamClose(hStream, 0);
	
	ms_message("%s, s->mb->size=%d", __func__, s->mb->size);
	s->enc = NULL;
	ms_bufferizer_flush(s->mb);
}

static int enc_add_fmtp(MSFilter *obj, void *arg) {
	char buf[64];
	const char *fmtp = (const char *) arg;
	EncState *s = (EncState*) obj->data;
	
	memset(buf, '\0', sizeof (buf));
	if (fmtp_get_value(fmtp, "ptime", buf, sizeof (buf)))
	{
		s->ptime = atoi(buf);
		//if the ptime is not a mulptiple of 30, go to the next multiple
		if (s->ptime % 30)
			s->ptime = s->ptime - s->ptime % 30 + 30;
		
		ms_message("G.723.1: got ptime=%i", s->ptime);
	}
	return 0;
}

static int enc_add_attr(MSFilter *obj, void *arg)
{
	const char *fmtp = (const char *) arg;
	EncState *s = (EncState*) obj->data;
	
	if (strstr(fmtp, "ptime:10") != NULL) {
		s->ptime = 30;
	} else if (strstr(fmtp, "ptime:20") != NULL) {
		s->ptime = 30;
	} else if (strstr(fmtp, "ptime:30") != NULL) {
		s->ptime = 30;
	} else if (strstr(fmtp, "ptime:40") != NULL) {
		s->ptime = 60;
	} else if (strstr(fmtp, "ptime:50") != NULL) {
		s->ptime = 60;
	} else if (strstr(fmtp, "ptime:60") != NULL) {
		s->ptime = 60;
	} else if (strstr(fmtp, "ptime:70") != NULL) {
		s->ptime = 90;
	} else if (strstr(fmtp, "ptime:80") != NULL) {
		s->ptime = 90;
	} else if (strstr(fmtp, "ptime:90") != NULL) {
		s->ptime = 90; /* not allowed */
	} else if (strstr(fmtp, "ptime:100") != NULL) {
		s->ptime = 120;
	} else if (strstr(fmtp, "ptime:110") != NULL) {
		s->ptime = 120;
	} else if (strstr(fmtp, "ptime:120") != NULL) {
		s->ptime = 120;
	} else if (strstr(fmtp, "ptime:130") != NULL) {
		s->ptime = 150;
	} else if (strstr(fmtp, "ptime:140") != NULL) {
		s->ptime = 150;
	}
	
	ms_message("G.723.1: got ptime=%i", s->ptime);
	return 0;
}

static MSFilterMethod enc_methods[] = {
	{ MS_FILTER_ADD_FMTP, enc_add_fmtp},
	{ MS_FILTER_ADD_ATTR, enc_add_attr},
	{ 0, NULL}
};
MSFilterDesc g7231_enc_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = "MSG7231Enc",
	.text = "G.723.1 encoder",
	.category = MS_FILTER_ENCODER,
	.enc_fmt = "G723",
	.ninputs = 1,
	.noutputs = 1,
	.init = enc_init,
	.process = enc_process,
	.postprocess = enc_postprocess,
	.uninit = enc_uninit,
	.methods = enc_methods
};

void libmsg7231_init()
{
	ms_filter_register(&g7231_dec_desc);
	ms_filter_register(&g7231_enc_desc);
	ms_message("libmsg7231 " VERSION " plugin loaded");
}


#if 0	// 참고용 소스
BOOL _stdcall AcmAppDriverEnumCallback(
	HACMDRIVERID		hadid,
	DWORD			   dwInstance,
	DWORD			   fdwSupport )
{
	char gszFormatDriversList[] = "%.04Xh\t%s\t%lu%s\t%.08lXh\t%s";
	char gszNull[] = "";
	static char szBogus[] = "????";
	MMRESULT			mmr;
	char				ach[1024];
	ACMDRIVERDETAILS	add;
	BOOL				fDisabled;
	DWORD			   dwPriority;
	
	add.cbStruct = sizeof(add);
	mmr = acmDriverDetails(hadid, &add, 0L);
	if (MMSYSERR_NOERROR != mmr)
	{
		lstrcpy(add.szShortName, szBogus);
		lstrcpy(add.szLongName,  szBogus);
	}
	
	dwPriority = (DWORD)-1L;
	acmMetrics((HACMOBJ)hadid, ACM_METRIC_DRIVER_PRIORITY, &dwPriority);
	
	fDisabled = (0 != (ACMDRIVERDETAILS_SUPPORTF_DISABLED & fdwSupport));
	
	wsprintf(ach, gszFormatDriversList,
			 hadid,
			 (LPTSTR)add.szShortName,
			 dwPriority,
			 fDisabled ? (LPTSTR)TEXT(" (disabled)") : (LPTSTR)gszNull,
			 fdwSupport,
			 (LPTSTR)add.szLongName);
	
	ms_message("%s, %s...", __func__, ach);
/*
	DWORD  cbStruct; 
	FOURCC fccType; 
	FOURCC fccComp; 
	WORD   wMid; 
	WORD   wPid; 
	DWORD  vdwACM; 
	DWORD  vdwDriver; 
	DWORD  fdwSupport; 
	DWORD  cFormatTags; 
	DWORD  cFilterTags; 
	HICON  hicon; 
	char  szShortName[ACMDRIVERDETAILS_SHORTNAME_CHARS]; 
	char  szLongName[ACMDRIVERDETAILS_LONGNAME_CHARS]; 
	char  szCopyright[ACMDRIVERDETAILS_COPYRIGHT_CHARS]; 
	char  szLicensing[ACMDRIVERDETAILS_LICENSING_CHARS]; 
	char  szFeatures[ACMDRIVERDETAILS_FEATURES_CHARS]; 
*/
	ms_message("%s, ACM=%p, Driver=%p, Support=%ld, FormatTag=%ld, FilterTag=%ld", __func__, 
		(void *)add.vdwACM, (void *)add.vdwDriver, add.fdwSupport, add.cFormatTags, add.cFilterTags);
	ms_message("%s, Short=[%s], Long=[%s], Copyright=[%s], Licensing=[%s], Features=[%s]", __func__, 
		add.szShortName, add.szLongName, add.szCopyright, add.szLicensing, add.szFeatures);

	//
	//  return TRUE to continue with enumeration (FALSE will stop the
	//  enumerator)
	//
	return (TRUE);
} // AcmAppDriverEnumCallback()

int	AcmDriverList(void)
{
	MMRESULT	mmr;
	DWORD		dwInstance = 0;
	
	mmr = acmDriverEnum(AcmAppDriverEnumCallback, dwInstance, ACM_DRIVERENUMF_DISABLED);
	if (MMSYSERR_NOERROR != mmr)
	{
		ms_message("this will let us know something is wrong!");
	}
	ms_message("%s, Start..................", __func__);

	return 0;
}

typedef struct
{
	HACMDRIVERID	hadid;
	WORD			wFormatTag;
} FIND_DRIVER_INFO;

// callback function for format enumeration
BOOL CALLBACK find_format_enum(HACMDRIVERID hadid, LPACMFORMATDETAILS pafd, DWORD dwInstance, DWORD fdwSupport)
{
	FIND_DRIVER_INFO* pdi = (FIND_DRIVER_INFO*) dwInstance;
	
	if (pafd->dwFormatTag == (DWORD)pdi->wFormatTag)
	{	// found it
		pdi->hadid = hadid;
		ms_message("%s,   %4.4lXH, %s", __func__, pafd->dwFormatTag, pafd->szFormat);
		return FALSE; // stop enumerating
	}
	//printf( "   FORMAT not MATCH.\n ");
	return TRUE; // continue enumerating
} 

// callback function for driver enumeration
BOOL CALLBACK find_driver_enum(HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
{
	FIND_DRIVER_INFO	*pdi = (FIND_DRIVER_INFO *)dwInstance;
	HACMDRIVER 			had = NULL;
	DWORD 				dwSize = 0;
	HRESULT 			mmr = 0;
	WAVEFORMATEX		*pwf = (WAVEFORMATEX *)NULL;
	ACMFORMATDETAILS	fd;
	
	// open the driver
	mmr = acmDriverOpen(&had, hadid, 0);
	if(mmr)
	{	// some error
		return FALSE; // stop enumerating
	}
	
	// enumerate the formats it supports
	mmr = acmMetrics((HACMOBJ)had, ACM_METRIC_MAX_SIZE_FORMAT, &dwSize);
	if (dwSize < sizeof(WAVEFORMATEX)) dwSize = sizeof(WAVEFORMATEX); // for MS-PCM
	
	pwf = (WAVEFORMATEX*) malloc(dwSize);
	memset(pwf, 0, dwSize);
	pwf->cbSize = LOWORD(dwSize) - sizeof(WAVEFORMATEX);
	pwf->wFormatTag = pdi->wFormatTag;
	
	memset(&fd, 0, sizeof(fd));
	fd.cbStruct = sizeof(fd);
	fd.pwfx = pwf;
	fd.cbwfx = dwSize;
	fd.dwFormatTag = pdi->wFormatTag;
	
	mmr = acmFormatEnum(had, &fd, find_format_enum, (DWORD)(VOID*)pdi, 0); 
	free(pwf);
	acmDriverClose(had, 0);
	if (pdi->hadid || mmr)
	{	// found it or some error
		return FALSE; // stop enumerating
	}
	return TRUE; // continue enumeration
}

HACMDRIVERID find_driver(WORD wFormatTag)
{
	FIND_DRIVER_INFO fdi;
	MMRESULT 		result;
	
	fdi.hadid = NULL;
	fdi.wFormatTag = wFormatTag;
	
	result = acmDriverEnum(find_driver_enum, (DWORD)(VOID*)&fdi, 0);
	if(result)	return NULL;
	return fdi.hadid;
}
#endif
