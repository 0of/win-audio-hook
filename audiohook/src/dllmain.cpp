#include "AudioStream.h"

#include "WaveOutHook.h"
#include "DSoundHook.h"

static AudioOutputStream *gStream = NULL;

BOOL WINAPI Patch()
{
    /* already patched */
    if (gStream)
        return FALSE;

    AudioOutputStream *stream = new AudioOutputStream;
    /* out of memory */
    if (!stream)
        return FALSE;

    if (stream->Open(L"audiohook"))
    {
        bool hooked = WaveOutHook::GetInstance().Hook(stream);
        hooked = DSoundHook::GetInstance().Hook(stream) || hooked;

        if (hooked)
        {
            gStream = stream;
            return TRUE;
        }
    }

    delete stream;
    return FALSE;
}

VOID Unpatch()
{
    if (gStream)
    {
        WaveOutHook::GetInstance().Unhook();
        DSoundHook::GetInstance().Unhook();

        delete gStream;
        gStream = NULL;
    }
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        if (FALSE == Patch())
            return FALSE;
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        Unpatch();
    }

	return TRUE;
}

