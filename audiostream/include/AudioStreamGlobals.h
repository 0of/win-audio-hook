#pragma once

#ifndef AUDIOSTREAM_EXPORT
#define AUDIOSTREAM_EXPORT __declspec(dllexport)
#else
#define AUDIOSTREAM_EXPORT __declspec(dllimport)
#endif