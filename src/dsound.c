#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <assert.h>
#include <MurmurHash3.h>

#include "audiolog.h"

#define FRAME_PREFIX "arec_"

typedef struct IDirectSound_hook IDirectSound_hook;

struct IDirectSound_hook
{
	IDirectSound *real;
	
	IDirectSound obj;
	IDirectSoundVtbl vtbl;
};

void __cdecl IDirectSound_hook_init_vtable(IDirectSound_hook*);

typedef struct IDirectSoundBuffer_hook IDirectSoundBuffer_hook;

struct IDirectSoundBuffer_hook
{
	IDirectSoundBuffer *real;
	
	IDirectSoundBuffer obj;
	IDirectSoundBufferVtbl vtbl;
	
	uint32_t buf_id;
	
	void *buf_start;
};

void __cdecl IDirectSoundBuffer_hook_init_vtable(IDirectSoundBuffer_hook*);

/* Function pointer to DirectSoundCreate() */
typedef HRESULT(__stdcall *DirectSoundCreate_t)(LPCGUID, IDirectSound**, LPUNKNOWN);

const char *capture_dir  = NULL;

FILE *audio_log = NULL;

HMODULE real_dsound = NULL;

static DirectSoundCreate_t real_DirectSoundCreate = NULL;

/* Returns the number of frames exported so far. */
unsigned int get_frame_count(void)
{
	static unsigned int frame_count = 0;
	
	while(1)
	{
		char path[MAX_PATH];
		
		snprintf(path, MAX_PATH, "%s\\" FRAME_PREFIX "%06u.png", capture_dir, frame_count);
		
		if(GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
		{
			frame_count++;
		}
		else{
			break;
		}
	}
	
	return frame_count;
}

void write_event(uint32_t id, uint32_t op, uint32_t arg)
{
	if(capture_dir)
	{
		unsigned int frames = get_frame_count();
		
		struct audio_event event;
		
		event.buf_id = id;
		event.frame  = frames;
		
		event.op  = op;
		event.arg = arg;
		
		fwrite(&event, sizeof(event), 1, audio_log);
	}
}

/* Wrap an IDirectSoundBuffer instance within a new IDirectSoundBuffer_hook.
 * 
 * Replaces the IDirectSoundBuffer at *obj and returns a pointer to the
 * IDirectSoundBuffer_hook structure.
*/
IDirectSoundBuffer_hook *wrap_IDirectSoundBuffer(IDirectSoundBuffer **obj)
{
	IDirectSoundBuffer_hook *hook = malloc(sizeof(IDirectSoundBuffer_hook));
	assert(hook);
	
	hook->real = *obj;
	
	hook->obj.lpVtbl = &(hook->vtbl);
	IDirectSoundBuffer_hook_init_vtable(hook);
	
	/* Each IDirectSoundBuffer_hook instance is given a serial number to
	 * differentiate buffers in the log.
	*/
	static uint32_t buf_id = 0;
	
	hook->buf_id    = ++buf_id;
	hook->buf_start = NULL;
	
	*obj = &(hook->obj);
	
	return hook;
}

ULONG __stdcall IDirectSound_hook_Release(IDirectSound_hook *self)
{
	ULONG refcount = IDirectSound_Release(self->real);
	
	if(refcount == 0)
	{
		free(self);
	}
	
	return refcount;
}

HRESULT __stdcall IDirectSound_hook_CreateSoundBuffer(struct IDirectSound_hook *self, const DSBUFFERDESC *lpcDSBufferDesc, IDirectSoundBuffer **lplpDirectSoundBuffer, IUnknown FAR *pUnkOuter)
{
	HRESULT result = IDirectSound_CreateSoundBuffer(self->real, lpcDSBufferDesc, lplpDirectSoundBuffer, pUnkOuter);
	
	if(result == DS_OK)
	{
		IDirectSoundBuffer_hook *hook = wrap_IDirectSoundBuffer(lplpDirectSoundBuffer);
		
		write_event(hook->buf_id, AUDIO_OP_INIT, 0);
	}
	
	return result;
}

HRESULT __stdcall IDirectSound_hook_DuplicateSoundBuffer(IDirectSound_hook *self, LPDIRECTSOUNDBUFFER lpDsbOriginal, LPLPDIRECTSOUNDBUFFER lplpDsbDuplicate)
{
	IDirectSoundBuffer_hook *old_hook = (IDirectSoundBuffer_hook*)((char*)(lpDsbOriginal) - sizeof(void*));
	
	HRESULT result = IDirectSound_DuplicateSoundBuffer(self->real, old_hook->real, lplpDsbDuplicate);
	
	if(result == DS_OK)
	{
		IDirectSoundBuffer_hook *new_hook = wrap_IDirectSoundBuffer(lplpDsbDuplicate);
		
		write_event(old_hook->buf_id, AUDIO_OP_CLONE, new_hook->buf_id);
	}
	
	return result;
}

ULONG __stdcall IDirectSoundBuffer_hook_Release(IDirectSoundBuffer_hook *self)
{
	ULONG refcount = IDirectSoundBuffer_Release(self->real);
	
	if(refcount == 0)
	{
		write_event(self->buf_id, AUDIO_OP_FREE, 0);
		free(self);
	}
	
	return refcount;
}

HRESULT __stdcall IDirectSoundBuffer_hook_Lock(IDirectSoundBuffer_hook *self, DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID lplpvAudioPtr1, LPDWORD lpdwAudioBytes1, LPVOID lplpvAudioPtr2, LPDWORD lpdwAudioBytes2, DWORD dwFlags)
{
	HRESULT result = IDirectSoundBuffer_Lock(self->real, dwWriteCursor, dwWriteBytes, lplpvAudioPtr1, lpdwAudioBytes1, lplpvAudioPtr2, lpdwAudioBytes2, dwFlags);
	
	/* We're only interested in hashing from the start of a buffer. This
	 * prevents attempts at hashing streaming buffers.
	*/
	
	if(result == DS_OK && dwWriteCursor == 0 && !(dwFlags & DSBLOCK_FROMWRITECURSOR))
	{
		self->buf_start = *(void**)(lplpvAudioPtr1);
	}
	
	return result;
}

static void _hash_buf_copy(char *h_data, size_t *h_size, const void *a_data, size_t a_size)
{
	if(a_data && a_size > 0)
	{
		size_t max_copy = AUDIO_HASH_MAX_DATA - *h_size;
		size_t do_copy  = a_size <= max_copy ? a_size : max_copy;
		
		memcpy(h_data + *h_size, a_data, do_copy);
		*h_size += do_copy;
	}
}

HRESULT __stdcall IDirectSoundBuffer_hook_Unlock(IDirectSoundBuffer_hook *self, LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2)
{
	if(self->buf_start == lpvAudioPtr1)
	{
		char data[AUDIO_HASH_MAX_DATA];
		size_t size = 0;
		
		_hash_buf_copy(data, &size, lpvAudioPtr1, dwAudioBytes1);
		_hash_buf_copy(data, &size, lpvAudioPtr2, dwAudioBytes2);
		
		uint32_t hash;
		MurmurHash3_x86_32(data, size, 0, &hash);
		
		write_event(self->buf_id, AUDIO_OP_LOAD, hash);
		
		self->buf_start = NULL;
	}
	
	return IDirectSoundBuffer_Unlock(self->real, lpvAudioPtr1, dwAudioBytes1, lpvAudioPtr2, dwAudioBytes2);
}

HRESULT __stdcall IDirectSoundBuffer_hook_SetCurrentPosition(IDirectSoundBuffer_hook *self, DWORD dwNewPosition)
{
	write_event(self->buf_id, AUDIO_OP_JMP, dwNewPosition);
	
	return IDirectSoundBuffer_SetCurrentPosition(self->real, dwNewPosition);
}

HRESULT __stdcall IDirectSoundBuffer_hook_Play(IDirectSoundBuffer_hook *self, DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags)
{
	write_event(self->buf_id, AUDIO_OP_START, dwFlags & DSBPLAY_LOOPING ? AUDIO_FLAG_REPEAT : 0);
	
	return IDirectSoundBuffer_Play(self->real, dwReserved1, dwPriority, dwFlags);
}

HRESULT __stdcall IDirectSoundBuffer_hook_Stop(IDirectSoundBuffer_hook *self)
{
	write_event(self->buf_id, AUDIO_OP_STOP, 0);
	
	return IDirectSoundBuffer_Stop(self->real);
}

HRESULT __stdcall IDirectSoundBuffer_hook_SetFrequency(IDirectSoundBuffer_hook *self, DWORD dwFrequency)
{
	write_event(self->buf_id, AUDIO_OP_FREQ, dwFrequency);
	
	return IDirectSoundBuffer_SetFrequency(self->real, dwFrequency);
}

HRESULT __stdcall IDirectSoundBuffer_hook_SetVolume(IDirectSoundBuffer_hook *self, LONG lVolume)
{
	write_event(self->buf_id, AUDIO_OP_VOLUME, lVolume + 10000);
	
	return IDirectSoundBuffer_SetVolume(self->real, lVolume);
}

HRESULT __stdcall DirectSoundCreate(LPCGUID lpcGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
{
	HRESULT result = real_DirectSoundCreate(lpcGuid, ppDS, pUnkOuter);
	
	if(result == DS_OK)
	{
		struct IDirectSound_hook *hook_ds = malloc(sizeof(struct IDirectSound_hook));
		
		hook_ds->real = *ppDS;
		
		hook_ds->obj.lpVtbl = &(hook_ds->vtbl);
		IDirectSound_hook_init_vtable(hook_ds);
		
		*ppDS = &(hook_ds->obj);
	}
	
	return result;
}

BOOL WINAPI DllMain(HINSTANCE me, DWORD why, LPVOID res)
{
	if(why == DLL_PROCESS_ATTACH)
	{
		char path[1024];
		
		/* Load the system dsound.dll and get the real DirectSoundCreate
		 * function from it.
		*/
		
		GetSystemDirectory(path, sizeof(path));
		strcat(path, "\\dsound.dll");
		
		assert((real_dsound = LoadLibrary(path)));
		
		assert((real_DirectSoundCreate = (DirectSoundCreate_t)(GetProcAddress(real_dsound, "DirectSoundCreate"))));
		
		if((capture_dir = getenv("AREC_CAPTURE_DIRECTORY")))
		{
			snprintf(path, sizeof(path), "%s\\" FRAME_PREFIX "audio.dat", capture_dir);
			
			assert((audio_log = fopen(path, "wb")));
		}
		
		if(getenv("AREC_LOAD_WORMKIT"))
		{
			LoadLibrary("HookLib.dll");
		}
	}
	else if(why == DLL_PROCESS_DETACH)
	{
		if(capture_dir)
		{
			fclose(audio_log);
		}
		
		FreeLibrary(real_dsound);
	}
	
	return TRUE;
}

void is_dsound_wrapper(void)
{
	
}
