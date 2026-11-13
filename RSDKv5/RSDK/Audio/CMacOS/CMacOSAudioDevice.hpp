// mixing gets queued and done on the main thread so there's nothing to lock/unlock lol
#define LockAudioDevice()   ;
#define UnlockAudioDevice() ;

namespace RSDK
{
class AudioDevice : public AudioDeviceBase
{
public:
    static bool32 Init();
    static void Release();

    // keeps the Sound Manager's queue topped up
    static void FrameInit();

    // no threads: streams always load synchronously
    inline static void HandleStreamLoad(ChannelInfo *channel, bool32 async)
    {
        (void)async;
        LoadStream(channel);
    }

private:
    static uint8 contextInitialized;

    static void InitAudioChannels();
    static void QueueBuffer(int32 index);
};
} // namespace RSDK
