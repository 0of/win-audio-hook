#ifdef AUDIOHOOK_EXPORTS
#define AUDIOHOOK_API __declspec(dllexport)
#else
#define AUDIOHOOK_API __declspec(dllimport)
#endif
