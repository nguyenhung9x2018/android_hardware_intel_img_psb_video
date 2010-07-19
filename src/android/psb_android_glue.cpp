#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <binder/MemoryHeapBase.h>
#include "psb_android_glue.h"

using namespace android;

sp<ISurface> isurface;
//ISurface::BufferHeap mBufferHeap;

unsigned char* psb_android_registerBuffers(void** android_isurface, int pid, int width, int height)
{
    sp<MemoryHeapBase> heap;
    int framesize = width * height * 2;

    heap = new MemoryHeapBase(framesize);
    if (heap->heapID() < 0)
    {
        printf("Error creating frame buffer heap");
	return 0;
    }

    //mBufferHeap = ISurface::BufferHeap(width, height, width, height, PIXEL_FORMAT_RGB_565, heap);

    isurface = static_cast<ISurface*>(*android_isurface);

//    isurface->registerBuffers(mBufferHeap);

//    return static_cast<uint8_t*>(mBufferHeap.heap->base());
    return (unsigned char *) 0xff;
}

void psb_android_postBuffer(int offset)
{
    isurface->postBuffer(offset);
}

//called in DestroySurfaces
void psb_android_clearHeap()
{
    if (isurface.get())
    {
	isurface->unregisterBuffers();
//	mBufferHeap.heap.clear();	
    }
}

void psb_android_register_isurface(void** android_isurface, int srcw, int srch)
{
    isurface = static_cast<ISurface*>(*android_isurface);
//bugbug mgross was here:     isurface->createTextureStreamSource();
//bugbug mgross was here:     isurface->setTextureStreamID(0);
//bugbug mgross was here:     isurface->setTextureStreamDim(srcw, srch);
}

void psb_android_texture_streaming_display(int buffer_index)
{
//bugbug mgross was here: refactored base classs we need to redo this    isurface->displayTextureStreamBuffer(buffer_index);
}

