

enum {
    CMACOS_AUDIO_BUFFER_COUNT  = 4,
    CMACOS_AUDIO_BUFFER_FRAMES = 1024, // ~23ms at 44.1kHz
};

struct ClassicAudioBuffer {
    ExtSoundHeader header;
    int16 samples[CMACOS_AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS];
};

static SndChannelPtr classicChannel        = NULL;
static SndCallBackUPP classicCallbackUPP   = NULL;
static ClassicAudioBuffer *classicBuffers  = NULL;
static volatile uint8 classicQueued[CMACOS_AUDIO_BUFFER_COUNT]; // set by the main thread, cleared at interrupt time
static int32 classicNextBuffer             = 0;
static SAMPLE_FORMAT *classicMixBuffer     = NULL;

uint8 AudioDevice::contextInitialized = false;

static pascal void BufferDone(SndChannelPtr chan, SndCommand *cmd)
{
    // mark the buffer as played
    (void)chan;
    uint32 index = (uint32)cmd->param2;
    if (index < CMACOS_AUDIO_BUFFER_COUNT)
        classicQueued[index] = 0;
}

bool32 AudioDevice::Init()
{
    if (!contextInitialized) {
        contextInitialized = true;
        InitAudioChannels();
    }

    audioState = false;

    classicBuffers   = (ClassicAudioBuffer *)NewPtrClear(sizeof(ClassicAudioBuffer) * CMACOS_AUDIO_BUFFER_COUNT);
    classicMixBuffer = (SAMPLE_FORMAT *)malloc(sizeof(SAMPLE_FORMAT) * CMACOS_AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS);
    if (!classicBuffers || !classicMixBuffer) {
        PrintLog(PRINT_NORMAL, "ERROR: Not enough memory for the Sound Manager buffers, continuing without sound");
        return true;
    }

    for (int32 i = 0; i < CMACOS_AUDIO_BUFFER_COUNT; ++i) {
        ExtSoundHeader *h = &classicBuffers[i].header;
        h->samplePtr      = (Ptr)classicBuffers[i].samples;
        h->numChannels    = AUDIO_CHANNELS;
        h->sampleRate     = (UnsignedFixed)((uint32)AUDIO_FREQUENCY << 16);
        h->loopStart      = 0;
        h->loopEnd        = 0;
        h->encode         = extSH;
        h->baseFrequency  = kMiddleC;
        h->numFrames      = CMACOS_AUDIO_BUFFER_FRAMES;
        h->sampleSize     = 16;
        classicQueued[i]  = 0;
    }

    classicCallbackUPP = NewSndCallBackUPP(BufferDone);
    classicChannel     = NULL;
    if (SndNewChannel(&classicChannel, sampledSynth, initStereo, classicCallbackUPP) != noErr) {
        PrintLog(PRINT_NORMAL, "ERROR: Unable to open a Sound Manager channel, continuing without sound");
        classicChannel = NULL;
        return true;
    }

    classicNextBuffer = 0;
    audioState        = true;
    FrameInit();

    // the game keeps running without sound
    return true;
}

void AudioDevice::Release()
{
    if (classicChannel)
        SndDisposeChannel(classicChannel, true);
    classicChannel = NULL;

    if (classicCallbackUPP)
        DisposeSndCallBackUPP(classicCallbackUPP);
    classicCallbackUPP = NULL;

    if (classicBuffers)
        DisposePtr((Ptr)classicBuffers);
    classicBuffers = NULL;

    free(classicMixBuffer);
    classicMixBuffer = NULL;

    AudioDeviceBase::Release();
    audioState = false;
}

void AudioDevice::InitAudioChannels() { AudioDeviceBase::InitAudioChannels(); }

void AudioDevice::QueueBuffer(int32 index)
{
    ClassicAudioBuffer *buf = &classicBuffers[index];

    ProcessAudioMixing(classicMixBuffer, CMACOS_AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS);
    for (int32 i = 0; i < CMACOS_AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS; ++i)
        buf->samples[i] = (int16)(CLAMP(classicMixBuffer[i], -1.0f, 1.0f) * 32767.0f);

    classicQueued[index] = 1;

    SndCommand cmd;
    cmd.cmd    = bufferCmd;
    cmd.param1 = 0;
    cmd.param2 = (long)&buf->header;
    SndDoCommand(classicChannel, &cmd, true);

    cmd.cmd    = callBackCmd;
    cmd.param1 = 0;
    cmd.param2 = index;
    SndDoCommand(classicChannel, &cmd, true);
}

void AudioDevice::FrameInit()
{
    if (!classicChannel)
        return;

    // buffers are queued (and finish) in order, so refill from classicNextBuffer on
    for (int32 i = 0; i < CMACOS_AUDIO_BUFFER_COUNT && !classicQueued[classicNextBuffer]; ++i) {
        QueueBuffer(classicNextBuffer);
        classicNextBuffer = (classicNextBuffer + 1) % CMACOS_AUDIO_BUFFER_COUNT;
    }
}
