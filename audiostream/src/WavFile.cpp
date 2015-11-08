 ////////////////////////////////////////////////////////////////////////////////
///
/// Classes for easy reading & writing of WAV sound files. 
///
/// For big-endian CPU, define _BIG_ENDIAN_ during compile-time to correctly
/// parse the WAV files with such processors.
/// 
/// Admittingly, more complete WAV reader routines may exist in public domain,
/// but the reason for 'yet another' one is that those generic WAV reader 
/// libraries are exhaustingly large and cumbersome! Wanted to have something
/// simpler here, i.e. something that's not already larger than rest of the
/// SoundTouch/SoundStretch program...
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date: 2012-09-01 11:03:26 +0300 (Sat, 01 Sep 2012) $
// File revision : $Revision: 4 $
//
// $Id: WavFile.cpp 154 2012-09-01 08:03:26Z oparviai $
//
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string>
#include <sstream>
#include <cstring>
#include <assert.h>
#include <limits.h>

#include "WavFile.h"

using namespace std;

static const char riffStr[] = "RIFF";
static const char waveStr[] = "WAVE";
static const char fmtStr[]  = "fmt ";
static const char dataStr[] = "data";


//////////////////////////////////////////////////////////////////////////////
//
// Helper functions for swapping byte order to correctly read/write WAV files 
// with big-endian CPU's: Define compile-time definition _BIG_ENDIAN_ to
// turn-on the conversion if it appears necessary. 
//
// For example, Intel x86 is little-endian and doesn't require conversion,
// while PowerPC of Mac's and many other RISC cpu's are big-endian.

#ifdef BYTE_ORDER
    // In gcc compiler detect the byte order automatically
    #if BYTE_ORDER == BIG_ENDIAN
        // big-endian platform.
        #define _BIG_ENDIAN_
    #endif
#endif
    
#ifdef _BIG_ENDIAN_
    // big-endian CPU, swap bytes in 16 & 32 bit words

    // helper-function to swap byte-order of 32bit integer
    static inline int _swap32(int &dwData)
    {
        dwData = ((dwData >> 24) & 0x000000FF) | 
               ((dwData >> 8)  & 0x0000FF00) | 
               ((dwData << 8)  & 0x00FF0000) | 
               ((dwData << 24) & 0xFF000000);
        return dwData;
    }   

    // helper-function to swap byte-order of 16bit integer
    static inline short _swap16(short &wData)
    {
        wData = ((wData >> 8) & 0x00FF) | 
                ((wData << 8) & 0xFF00);
        return wData;
    }

    // helper-function to swap byte-order of buffer of 16bit integers
    static inline void _swap16Buffer(short *pData, int numWords)
    {
        int i;

        for (i = 0; i < numWords; i ++)
        {
            pData[i] = _swap16(pData[i]);
        }
    }

#else   // BIG_ENDIAN
    // little-endian CPU, WAV file is ok as such

    // dummy helper-function
    static inline int _swap32(int &dwData)
    {
        // do nothing
        return dwData;
    }   

    // dummy helper-function
    static inline short _swap16(short &wData)
    {
        // do nothing
        return wData;
    }

    // dummy helper-function
    static inline void _swap16Buffer(short *pData, int numBytes)
    {
        // do nothing
    }

#endif  // BIG_ENDIAN


//////////////////////////////////////////////////////////////////////////////
//
// Class WavFileBase
//

WavFileBase::WavFileBase()
{
    convBuff = NULL;
    convBuffSize = 0;
}


WavFileBase::~WavFileBase()
{
    delete[] convBuff;
    convBuffSize = 0;
}


/// Get pointer to conversion buffer of at min. given size
void *WavFileBase::getConvBuffer(int sizeBytes)
{
    if (convBuffSize < sizeBytes)
    {
        delete[] convBuff;

        convBuffSize = (sizeBytes + 15) & -8;   // round up to following 8-byte bounday
        convBuff = new char[convBuffSize];
    }
    return convBuff;
}



// test if character code is between a white space ' ' and little 'z'
static int isAlpha(char c)
{
    return (c >= ' ' && c <= 'z') ? 1 : 0;
}


// test if all characters are between a white space ' ' and little 'z'
static int isAlphaStr(const char *str)
{
    char c;

    c = str[0];
    while (c) 
    {
        if (isAlpha(c) == 0) return 0;
        str ++;
        c = str[0];
    }

    return 1;
}

//////////////////////////////////////////////////////////////////////////////
//
// Class WavOutFile
//

WavOutFile *WavOutFile::Create(const wchar_t *fileName,    ///< Filename
    int sampleRate,          ///< Sample rate (e.g. 44100 etc)
    int bits,                ///< Bits per sample (8 or 16 bits)
    int channels             ///< Number of channels (1=mono, 2=stereo))
    )
{
    WavOutFile *file = new WavOutFile;
    if (file)
    {
        file->bytesWritten = 0;
        file->fptr = _wfopen(fileName, L"wb");
        if (file->fptr != NULL)
        {
            file->fillInHeader(sampleRate, bits, channels);
            file->writeHeader();
        }
        else
        {
            delete file;
            file = NULL;
        }
    }

    return file;
}

WavOutFile::WavOutFile()
{
   
}

WavOutFile::~WavOutFile()
{
    finishHeader();
    if (fptr) fclose(fptr);
    fptr = NULL;
}

void WavOutFile::fillInHeader(uint sampleRate, uint bits, uint channels)
{
    // fill in the 'riff' part..

    // copy string 'RIFF' to riff_char
    memcpy(&(header.riff.riff_char), riffStr, 4);
    // package_len unknown so far
    header.riff.package_len = 0;
    // copy string 'WAVE' to wave
    memcpy(&(header.riff.wave), waveStr, 4);

    // fill in the 'format' part..

    // copy string 'fmt ' to fmt
    memcpy(&(header.format.fmt), fmtStr, 4);

    header.format.format_len = 0x10;
    header.format.fixed = 1;
    header.format.channel_number = (short)channels;
    header.format.sample_rate = (int)sampleRate;
    header.format.bits_per_sample = (short)bits;
    header.format.byte_per_sample = (short)(bits * channels / 8);
    header.format.byte_rate = header.format.byte_per_sample * (int)sampleRate;
    header.format.sample_rate = (int)sampleRate;

    // fill in the 'data' part..

    // copy string 'data' to data_field
    memcpy(&(header.data.data_field), dataStr, 4);
    // data_len unknown so far
    header.data.data_len = 0;
}


void WavOutFile::finishHeader()
{
    // supplement the file length into the header structure
    header.riff.package_len = bytesWritten + 36;
    header.data.data_len = bytesWritten;

    writeHeader();
}



void WavOutFile::writeHeader()
{
    WavHeader hdrTemp;
    int res;

    // swap byte order if necessary
    hdrTemp = header;
    _swap32((int &)hdrTemp.riff.package_len);
    _swap32((int &)hdrTemp.format.format_len);
    _swap16((short &)hdrTemp.format.fixed);
    _swap16((short &)hdrTemp.format.channel_number);
    _swap32((int &)hdrTemp.format.sample_rate);
    _swap32((int &)hdrTemp.format.byte_rate);
    _swap16((short &)hdrTemp.format.byte_per_sample);
    _swap16((short &)hdrTemp.format.bits_per_sample);
    _swap32((int &)hdrTemp.data.data_len);

    // write the supplemented header in the beginning of the file
    fseek(fptr, 0, SEEK_SET);
    res = (int)fwrite(&hdrTemp, sizeof(hdrTemp), 1, fptr);
    if (res != 1)
    {
        throw "IO exception";
    }

    // jump back to the end of the file
    fseek(fptr, 0, SEEK_END);
}



void WavOutFile::write(const unsigned char *buffer, int numElems)
{
    int res;

    if (header.format.bits_per_sample != 8)
    {
        throw ("Error: WavOutFile::write(const char*, int) accepts only 8bit samples.");
    }
    assert(sizeof(char) == 1);

    res = (int)fwrite(buffer, 1, numElems, fptr);
    if (res != numElems) 
    {
        throw ("Error while writing to a wav file.");
    }

    bytesWritten += numElems;
}



void WavOutFile::write(const short *buffer, int numElems)
{
    int res;

    // 16 bit samples
    if (numElems < 1) return;   // nothing to do

    switch (header.format.bits_per_sample)
    {
        case 8:
        {
            int i;
            unsigned char *temp = (unsigned char *)getConvBuffer(numElems);
            // convert from 16bit format to 8bit format
            for (i = 0; i < numElems; i ++)
            {
                temp[i] = (unsigned char)(buffer[i] / 256 + 128);
            }
            // write in 8bit format
            write(temp, numElems);
            break;
        }

        case 16:
        {
            // 16bit format

            // use temp buffer to swap byte order if necessary
            short *pTemp = (short *)getConvBuffer(numElems * sizeof(short));
            memcpy(pTemp, buffer, numElems * 2);
            _swap16Buffer(pTemp, numElems);

            res = (int)fwrite(pTemp, 2, numElems, fptr);

            if (res != numElems) 
            {
                throw ("Error while writing to a wav file.");
            }
            bytesWritten += 2 * numElems;
            break;
        }

        default:
        {
            stringstream ss;
            ss << "\nOnly 8/16 bit sample WAV files supported in integer compilation. Can't open WAV file with ";
            ss << (int)header.format.bits_per_sample;
            ss << " bit sample format. ";
            throw (ss.str().c_str());
        }
    }
}


/// Convert from float to integer and saturate
inline int saturate(float fvalue, float minval, float maxval)
{
    if (fvalue > maxval) 
    {
        fvalue = maxval;
    } 
    else if (fvalue < minval)
    {
        fvalue = minval;
    }
    return (int)fvalue;
}


void WavOutFile::write(const float *buffer, int numElems)
{
    int numBytes;
    int bytesPerSample;

    if (numElems == 0) return;

    bytesPerSample = header.format.bits_per_sample / 8;
    numBytes = numElems * bytesPerSample;
    short *temp = (short*)getConvBuffer(numBytes);

    switch (bytesPerSample)
    {
        case 1:
        {
            unsigned char *temp2 = (unsigned char *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                temp2[i] = (unsigned char)saturate(buffer[i] * 128.0f + 128.0f, 0.0f, 255.0f);
            }
            break;
        }

        case 2:
        {
            short *temp2 = (short *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                short value = (short)saturate(buffer[i] * 32768.0f, -32768.0f, 32767.0f);
                temp2[i] = _swap16(value);
            }
            break;
        }

        case 3:
        {
            char *temp2 = (char *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                int value = saturate(buffer[i] * 8388608.0f, -8388608.0f, 8388607.0f);
                *((int*)temp2) = _swap32(value);
                temp2 += 3;
            }
            break;
        }

        case 4:
        {
            int *temp2 = (int *)temp;
            for (int i = 0; i < numElems; i ++)
            {
                int value = saturate(buffer[i] * 2147483648.0f, -2147483648.0f, 2147483647.0f);
                temp2[i] = _swap32(value);
            }
            break;
        }

        default:
            assert(false);
    }

    int res = (int)fwrite(temp, 1, numBytes, fptr);

    if (res != numBytes) 
    {
        throw("Error while writing to a wav file.");
    }
    bytesWritten += numBytes;
}
