using ShaderEntry = ShaderEntryBase;

class RenderDevice : public RenderDeviceBase
{
public:
    struct WindowInfo {
        struct {
            int32 width;
            int32 height;
            int32 refresh_rate;
        } * displays;
    };
    static WindowInfo displayInfo;

    static bool Init();
    static void CopyFrameBuffer();
    static void FlipScreen();
    static void Release(bool32 isRefresh);

    static void RefreshWindow();
    static void GetWindowSize(int32 *width, int32 *height);

    static void SetupImageTexture(int32 width, int32 height, uint8 *imagePixels);
    static void SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);
    static void SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);
    static void SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);

    static bool ProcessEvents();

    static void InitFPSCap();
    static bool CheckFPSCap();
    static void UpdateFPSCap();

    static bool InitShaders();
    static void LoadShader(const char *fileName, bool32 linear);

    static void ShowCursor(bool32 shown);
    static bool GetCursorPos(Vector2 *pos);

    static void SetWindowTitle();

    // milsko widgets (MwWidget)
    static void *window;
    static void *glWidget;

    // called from milsko's event handlers
    static void OnKey(int32 key, bool32 down);
    static void ReleaseKeys();
    static void UpdateViewport();

private:
    static bool SetupRendering();
    static bool InitGraphicsAPI();
    static bool InitOpenGL();
    static void GetDisplays();
    static void SetFullscreen(bool32 fullscreen);

    static uint64 targetFreq;
    static uint64 curTicks;
    static uint64 prevTicks;

    // fullscreen images and videos (ARGB8888)
    static uint32 *imageBuffer;
    static int32 imageWidth;
    static int32 imageHeight;
};
