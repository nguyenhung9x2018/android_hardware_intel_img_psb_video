
#ifdef __cplusplus
extern "C" {
#endif
unsigned char* psb_android_registerBuffers(void** surface, int pid, int width, int height);

void psb_android_postBuffer(int offset);

void psb_android_clearHeap();

void psb_android_texture_streaming_display(int buffer_index);

void psb_android_register_isurface(void** surface, int srcw, int srch);

#ifdef __cplusplus
}
#endif
