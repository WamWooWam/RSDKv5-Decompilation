#if defined(macintosh) && !defined(__APPLE__)
#include <gl.h>
#define MW_OPENGL_NO_INCLUDE
#endif

// avoid clash with RSDK::FileInfo
#define FileInfo ToolboxFileInfo
#include <Mw/Milsko.h>
#include <Mw/Widget/OpenGL.h>
#undef FileInfo

#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

#define MILSKO_GL_FUNCS(X)                                                                                                                           \
    X(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h))                                                                                   \
    X(void, glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a))                                                                         \
    X(void, glClear, (GLbitfield mask))                                                                                                              \
    X(void, glEnable, (GLenum cap))                                                                                                                  \
    X(void, glDisable, (GLenum cap))                                                                                                                 \
    X(void, glBlendFunc, (GLenum s, GLenum d))                                                                                                       \
    X(void, glGenTextures, (GLsizei n, GLuint * tex))                                                                                                \
    X(void, glDeleteTextures, (GLsizei n, const GLuint *tex))                                                                                        \
    X(void, glBindTexture, (GLenum target, GLuint tex))                                                                                              \
    X(void, glTexParameteri, (GLenum target, GLenum pname, GLint param))                                                                            \
    X(void, glTexEnvi, (GLenum target, GLenum pname, GLint param))                                                                                  \
    X(void, glTexImage2D,                                                                                                                            \
      (GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum fmt, GLenum type, const GLvoid *px))                      \
    X(void, glTexSubImage2D, (GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, const GLvoid *px))      \
    X(void, glPixelStorei, (GLenum pname, GLint param))                                                                                              \
    X(void, glMatrixMode, (GLenum mode))                                                                                                             \
    X(void, glLoadIdentity, (void))                                                                                                                  \
    X(void, glOrtho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f))                                                       \
    X(void, glBegin, (GLenum mode))                                                                                                                  \
    X(void, glEnd, (void))                                                                                                                           \
    X(void, glTexCoord2f, (GLfloat s, GLfloat t))                                                                                                    \
    X(void, glVertex2f, (GLfloat x, GLfloat y))                                                                                                      \
    X(void, glColor4f, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                                                                                 \
    X(const GLubyte *, glGetString, (GLenum name))

#define MILSKO_GL_DECLARE(ret, name, args)                                                                                                           \
    typedef ret(GLAPIENTRY *PFN_mw_##name) args;                                                                                                     \
    static PFN_mw_##name mw_##name = NULL;
MILSKO_GL_FUNCS(MILSKO_GL_DECLARE)
#undef MILSKO_GL_DECLARE

void *RenderDevice::window   = NULL;
void *RenderDevice::glWidget = NULL;

uint64 RenderDevice::targetFreq = 0;
uint64 RenderDevice::curTicks   = 0;
uint64 RenderDevice::prevTicks  = 0;

uint32 *RenderDevice::imageBuffer = NULL;
int32 RenderDevice::imageWidth    = 0;
int32 RenderDevice::imageHeight   = 0;

enum { MILSKO_TEXTURE_SLOTS = SCREEN_COUNT + 1, MILSKO_IMAGE_SLOT = SCREEN_COUNT };

struct MilskoTexture {
    GLuint id;
    int32 width, height; // allocated (power of two) size
    GLenum format, type;
};

static MilskoTexture mwTextures[MILSKO_TEXTURE_SLOTS];

// whether or not we have GL_APPLE_packed_pixels so we can upload RGB565 and ARGB8888
// or if we just convert to RGB8
static bool hasPackedPixels    = true;

static uint8 *mwConvertBuffer = NULL;

static bool32 mwQuit         = false;
static bool32 mwFullscreen   = false;
static bool32 mwAltDown      = false;
static int32 mwWindowedX     = 0;
static int32 mwWindowedY     = 0;
static int32 mwWindowWidth   = 0;
static int32 mwWindowHeight  = 0;
static int32 mwContentX      = 0;
static int32 mwContentY      = 0;
static int32 mwContentWidth  = 0;
static int32 mwContentHeight = 0;
static int32 mwHeldKeys[0x20];
static int32 mwHeldKeyCount = 0;

// milsko key codes (MwKEY_* and characters) to Windows virtual keys
static int32 MilskoKeyToVK(int32 key)
{
    switch (key) {
        case MwKEY_BACKSPACE: return 0x08;
        case MwKEY_LEFT: return 0x25;
        case MwKEY_RIGHT: return 0x27;
        case MwKEY_UP: return 0x26;
        case MwKEY_DOWN: return 0x28;
        case MwKEY_ENTER: return 0x0D;
        case MwKEY_ESCAPE: return 0x1B;
        case MwKEY_LEFTSHIFT: return 0xA0;
        case MwKEY_RIGHTSHIFT: return 0xA1;
        case MwKEY_ALT: return 0xA4;
        case MwKEY_CONTROL: return 0xA2;
        default: break;
    }

    if (key & MwKEY_FLAG) {
        // control characters arrive as control + letter (tab = control-i)
        if ((key & (MwKEY_CONTROL_FLAG)) == (MwKEY_CONTROL_FLAG) && (key & 0xFF) == 'i')
            return 0x09;
        return 0;
    }

    if (key >= 'a' && key <= 'z')
        return key - 'a' + 'A';
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return key;

    switch (key) {
        case ' ': return 0x20;
        case '\t': return 0x09;
        case '\r':
        case '\n': return 0x0D;
        case ';':
        case ':': return 0xBA;
        case '=':
        case '+': return 0xBB;
        case ',':
        case '<': return 0xBC;
        case '-':
        case '_': return 0xBD;
        case '.':
        case '>': return 0xBE;
        case '/':
        case '?': return 0xBF;
        case '`':
        case '~': return 0xC0;
        case '[':
        case '{': return 0xDB;
        case '\\':
        case '|': return 0xDC;
        case ']':
        case '}': return 0xDD;
        case '\'':
        case '"': return 0xDE;
        default: return 0;
    }
}

static void MWAPI MilskoOnKey(MwWidget handle, void *user, void *call)
{
    (void)handle;
    (void)user;
    RenderDevice::OnKey(*(int *)call, true);
}

static void MWAPI MilskoOnKeyRelease(MwWidget handle, void *user, void *call)
{
    (void)handle;
    (void)user;
    RenderDevice::OnKey(*(int *)call, false);
}

static void MWAPI MilskoOnClose(MwWidget handle, void *user, void *call)
{
    (void)handle;
    (void)user;
    (void)call;
    mwQuit = true;
}

static void MWAPI MilskoOnFocusIn(MwWidget handle, void *user, void *call)
{
    (void)handle;
    (void)user;
    (void)call;
    engine.inFocus = 1;
}

static void MWAPI MilskoOnFocusOut(MwWidget handle, void *user, void *call)
{
    (void)handle;
    (void)user;
    (void)call;
    engine.inFocus = 0;
    RenderDevice::ReleaseKeys();
}

static void MWAPI MilskoOnResize(MwWidget handle, void *user, void *call)
{
    (void)user;
    (void)call;
    mwWindowWidth  = MwGetInteger(handle, MwNwidth);
    mwWindowHeight = MwGetInteger(handle, MwNheight);
    RenderDevice::UpdateViewport();
}

static void MWAPI MilskoOnError(int code, const char *message, void *user)
{
    (void)code;
    (void)user;
    PrintLog(PRINT_NORMAL, "milsko: %s", message);
}

static int32 MilskoNextPow2(int32 v)
{
    int32 p = 1;
    while (p < v) p <<= 1;
    return p;
}

static void MilskoUpload(int32 slot, const void *pixels, int32 pitch, int32 width, int32 height, GLenum ifmt, GLenum format, GLenum type)
{
    MilskoTexture *tex = &mwTextures[slot];
    int32 texW         = MilskoNextPow2(width);
    int32 texH         = MilskoNextPow2(height);

    if (!tex->id)
        mw_glGenTextures(1, &tex->id);
    mw_glBindTexture(GL_TEXTURE_2D, tex->id);

    if (tex->width < texW || tex->height < texH || tex->format != format || tex->type != type) {
        tex->width  = MAX(texW, tex->width);
        tex->height = MAX(texH, tex->height);
        tex->format = format;
        tex->type   = type;
        mw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        mw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        mw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        mw_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        mw_glTexImage2D(GL_TEXTURE_2D, 0, ifmt, tex->width, tex->height, 0, format, type, NULL);
    }

    mw_glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
    mw_glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    mw_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);
    mw_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

static void MilskoQuad(int32 slot, int32 width, int32 height, float x0, float y0, float x1, float y1)
{
    MilskoTexture *tex = &mwTextures[slot];
    float u            = (float)width / tex->width;
    float v            = (float)height / tex->height;

    mw_glEnable(GL_TEXTURE_2D);
    mw_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    mw_glBegin(GL_QUADS);
    mw_glTexCoord2f(0, 0);
    mw_glVertex2f(x0, y0);
    mw_glTexCoord2f(u, 0);
    mw_glVertex2f(x1, y0);
    mw_glTexCoord2f(u, v);
    mw_glVertex2f(x1, y1);
    mw_glTexCoord2f(0, v);
    mw_glVertex2f(x0, y1);
    mw_glEnd();
    mw_glDisable(GL_TEXTURE_2D);
}

static uint8 *MilskoConvertBuffer(int32 width, int32 height)
{
    if (width * height > RETRO_VIDEO_TEXTURE_W * RETRO_VIDEO_TEXTURE_H)
        return NULL;
    if (!mwConvertBuffer)
        mwConvertBuffer = (uint8 *)malloc(RETRO_VIDEO_TEXTURE_W * RETRO_VIDEO_TEXTURE_H * 4);
    return mwConvertBuffer;
}

static void MilskoDrawRGB565(int32 slot, const uint16 *pixels, int32 pitch, int32 width, int32 height, float x0, float y0, float x1, float y1)
{
    if (hasPackedPixels) {
        MilskoUpload(slot, pixels, pitch, width, height, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
    }
    else {
        uint8 *out = MilskoConvertBuffer(width, height);
        if (!out)
            return;
        for (int32 y = 0; y < height; ++y) {
            const uint16 *src = pixels + y * pitch;
            for (int32 x = 0; x < width; ++x) {
                uint16 c = src[x];
                *out++   = (uint8)(((c >> 11) & 0x1F) << 3);
                *out++   = (uint8)(((c >> 5) & 0x3F) << 2);
                *out++   = (uint8)((c & 0x1F) << 3);
                *out++   = 0xFF;
            }
        }
        MilskoUpload(slot, mwConvertBuffer, width, width, height, GL_RGB, GL_RGBA, GL_UNSIGNED_BYTE);
    }
    MilskoQuad(slot, width, height, x0, y0, x1, y1);
}

static void MilskoDrawARGB8888(const uint32 *pixels, int32 pitch, int32 width, int32 height)
{
    if (hasPackedPixels) {
        MilskoUpload(MILSKO_IMAGE_SLOT, pixels, pitch, width, height, GL_RGB, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
    }
    else {
        uint8 *out = MilskoConvertBuffer(width, height);
        if (!out)
            return;
        for (int32 y = 0; y < height; ++y) {
            const uint32 *src = pixels + y * pitch;
            for (int32 x = 0; x < width; ++x) {
                uint32 c = src[x];
                *out++   = (uint8)(c >> 16);
                *out++   = (uint8)(c >> 8);
                *out++   = (uint8)c;
                *out++   = 0xFF;
            }
        }
        MilskoUpload(MILSKO_IMAGE_SLOT, mwConvertBuffer, width, width, height, GL_RGB, GL_RGBA, GL_UNSIGNED_BYTE);
    }
    MilskoQuad(MILSKO_IMAGE_SLOT, width, height, 0.0f, 0.0f, 1.0f, 1.0f);
}

bool RenderDevice::InitOpenGL()
{

    glWidget = MwCreateWidget(MwOpenGLClass, "gl", (MwWidget)window, 0, 0, mwWindowWidth, mwWindowHeight);
    if (!glWidget) {
        PrintLog(PRINT_NORMAL, "ERROR: could not create an OpenGL context (is OpenGL installed?)");
        return false;
    }

    MwOpenGLMakeCurrent((MwWidget)glWidget);
#define MILSKO_GL_LOAD(ret, name, args)                                                                                                              \
    mw_##name = (PFN_mw_##name)MwOpenGLGetProcAddress((MwWidget)glWidget, #name);                                                                    \
    if (!mw_##name) {                                                                                                                                \
        PrintLog(PRINT_NORMAL, "ERROR: OpenGL is missing " #name);                                                                                   \
        return false;                                                                                                                                \
    }
    MILSKO_GL_FUNCS(MILSKO_GL_LOAD)
#undef MILSKO_GL_LOAD

    const char *glRenderer   = (const char *)mw_glGetString(GL_RENDERER);
    const char *glVersion    = (const char *)mw_glGetString(GL_VERSION);
    const char *glExtensions = (const char *)mw_glGetString(GL_EXTENSIONS);
    PrintLog(PRINT_NORMAL, "OpenGL renderer: %s / %s", glRenderer ? glRenderer : "?", glVersion ? glVersion : "?");

    bool is12      = glVersion && (glVersion[0] > '1' || (glVersion[0] == '1' && glVersion[2] >= '2'));
    hasPackedPixels = is12 || (glExtensions && strstr(glExtensions, "GL_APPLE_packed_pixels"));
    if (!hasPackedPixels)
        PrintLog(PRINT_NORMAL, "OpenGL: no packed pixel formats, converting to RGBA8");

    memset(mwTextures, 0, sizeof(mwTextures));
    mw_glDisable(GL_DEPTH_TEST);
    mw_glDisable(GL_LIGHTING);
    mw_glDisable(GL_DITHER);
    mw_glClearColor(0, 0, 0, 1);
    return true;
}

bool RenderDevice::Init()
{
    if (MwLibraryInit() != 0) {
        PrintLog(PRINT_NORMAL, "ERROR: milsko could not be initialized");
        return false;
    }
    MwSetErrorHandler(MilskoOnError, NULL);

    MwRect screen;
    MwGetScreenSize(NULL, &screen);

    // default to the biggest integer scale of the game screen that fits on the screen
    if (videoSettings.windowWidth <= 0 || videoSettings.windowHeight <= 0 || videoSettings.windowWidth > screen.width
        || videoSettings.windowHeight > screen.height - 40) {
        int32 scale = 1;
        while (videoSettings.pixWidth * (scale + 1) <= screen.width && SCREEN_YSIZE * (scale + 1) <= screen.height - 60) scale++;
        videoSettings.windowWidth  = videoSettings.pixWidth * scale;
        videoSettings.windowHeight = SCREEN_YSIZE * scale;
    }

    mwWindowWidth  = videoSettings.windowWidth;
    mwWindowHeight = videoSettings.windowHeight;

    window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, mwWindowWidth, mwWindowHeight, MwNtitle,
                              gameVerInfo.gameTitle, NULL);
    if (!window) {
        PrintLog(PRINT_NORMAL, "ERROR: failed to create window!");
        return false;
    }

    MwAddUserHandler((MwWidget)window, MwNcloseHandler, MilskoOnClose, NULL);
    MwAddUserHandler((MwWidget)window, MwNresizeHandler, MilskoOnResize, NULL);
    MwAddUserHandler((MwWidget)window, MwNkeyHandler, MilskoOnKey, NULL);
    MwAddUserHandler((MwWidget)window, MwNkeyReleaseHandler, MilskoOnKeyRelease, NULL);
    MwAddUserHandler((MwWidget)window, MwNfocusInHandler, MilskoOnFocusIn, NULL);
    MwAddUserHandler((MwWidget)window, MwNfocusOutHandler, MilskoOnFocusOut, NULL);

    /* TODO: who feels brave enough to learn Quickdraw3D/RAVE */
    if (!InitOpenGL()) {
        Release(false);
        return false;
    }

    UpdateViewport();
    if (!videoSettings.windowed)
        SetFullscreen(true);

    PrintLog(PRINT_NORMAL, "w: %d h: %d windowed: %d", videoSettings.windowWidth, videoSettings.windowHeight, videoSettings.windowed);

    // let the window appear before the first frame
    while (MwPending((MwWidget)window)) MwStep((MwWidget)window);

    if (!SetupRendering() || !AudioDevice::Init())
        return false;

    InitInputDevices();
    return true;
}

void RenderDevice::CopyFrameBuffer()
{
    // the screens are uploaded straight from their frame buffers in FlipScreen
}

void RenderDevice::FlipScreen()
{
    if (windowRefreshDelay > 0) {
        windowRefreshDelay--;
        if (!windowRefreshDelay)
            UpdateGameWindow();
        return;
    }

    uint64 flipStart = (uint64)MwTimeGetTick();

    MwOpenGLMakeCurrent((MwWidget)glWidget);
    mw_glViewport(0, 0, mwWindowWidth, mwWindowHeight);
    mw_glClear(GL_COLOR_BUFFER_BIT);
    mw_glViewport(mwContentX, mwWindowHeight - mwContentY - mwContentHeight, mwContentWidth, mwContentHeight);
    mw_glMatrixMode(GL_PROJECTION);
    mw_glLoadIdentity();
    mw_glOrtho(0, 1, 1, 0, -1, 1);
    mw_glMatrixMode(GL_MODELVIEW);
    mw_glLoadIdentity();

    if (videoSettings.screenCount == 0) {
        if (imageBuffer && imageWidth > 0 && imageHeight > 0)
            MilskoDrawARGB8888(imageBuffer, imageWidth, imageWidth, imageHeight);
    }
    else {
        int32 count = videoSettings.screenCount > SCREEN_COUNT ? SCREEN_COUNT : videoSettings.screenCount;
        for (int32 s = 0; s < count; ++s) {
            int32 startVert = 0;
            switch (count) {
                default:
                case 1: startVert = 0; break;
#if RETRO_REV02
                case 2: startVert = startVertex_2P[s]; break;
                case 3: startVert = startVertex_3P[s]; break;
#else
                case 2: startVert = s ? 12 : 6; break;
#endif
                case 4: startVert = 30 + s * 6; break;
            }

            // bounding box of the 6 vertices, from normalized device coordinates to 0..1 (y down)
            float minX = 1.0f, minY = 1.0f, maxX = -1.0f, maxY = -1.0f;
            for (int32 v = 0; v < 6; ++v) {
                const RenderVertex *vert = &rsdkVertexBuffer[startVert + v];
                minX                     = MIN(minX, vert->pos.x);
                maxX                     = MAX(maxX, vert->pos.x);
                minY                     = MIN(minY, vert->pos.y);
                maxY                     = MAX(maxY, vert->pos.y);
            }

            float x0 = (minX + 1.0f) * 0.5f;
            float x1 = (maxX + 1.0f) * 0.5f;
            float y0 = 1.0f - (maxY + 1.0f) * 0.5f;
            float y1 = 1.0f - (minY + 1.0f) * 0.5f;

            MilskoDrawRGB565(s, screens[s].frameBuffer, screens[s].pitch, screens[s].size.x, screens[s].size.y, x0, y0, x1, y1);
        }
    }

    float brightness = CLAMP(videoSettings.dimMax * videoSettings.dimPercent, 0.0f, 1.0f);
    if (brightness < 1.0f) {
        mw_glEnable(GL_BLEND);
        mw_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        mw_glColor4f(0, 0, 0, 1.0f - brightness);
        mw_glBegin(GL_QUADS);
        mw_glVertex2f(0, 0);
        mw_glVertex2f(1, 0);
        mw_glVertex2f(1, 1);
        mw_glVertex2f(0, 1);
        mw_glEnd();
        mw_glDisable(GL_BLEND);
        mw_glColor4f(1, 1, 1, 1);
    }

    MwOpenGLSwapBuffer((MwWidget)glWidget);

    // frame rate in the log every 10 seconds, with the time spent presenting frames
    static uint64 fpsStart = 0;
    static uint64 flipTime = 0;
    static int32 fpsFrames = 0;
    uint64 now             = (uint64)MwTimeGetTick();
    if (!fpsStart)
        fpsStart = now;
    flipTime += now - flipStart;
    ++fpsFrames;
    if (now - fpsStart >= 10000) {
        int32 fps10   = (int32)(fpsFrames * 10000ULL / (now - fpsStart));
        int32 frameUS = (int32)((now - fpsStart) * 1000 / fpsFrames);
        int32 flipUS  = (int32)(flipTime * 1000 / fpsFrames);
        PrintLog(PRINT_NORMAL, "FPS: %d.%d (frame %d.%dms: present %d.%dms, engine %d.%dms)", fps10 / 10, fps10 % 10, frameUS / 1000,
                 (frameUS / 100) % 10, flipUS / 1000, (flipUS / 100) % 10, (frameUS - flipUS) / 1000, ((frameUS - flipUS) / 100) % 10);
        fpsStart  = now;
        flipTime  = 0;
        fpsFrames = 0;
    }
}

void RenderDevice::Release(bool32 isRefresh)
{
    if (isRefresh)
        return;

    if (glWidget) {
        MwOpenGLMakeCurrent((MwWidget)glWidget);
        for (int32 i = 0; i < MILSKO_TEXTURE_SLOTS; ++i) {
            if (mwTextures[i].id && mw_glDeleteTextures)
                mw_glDeleteTextures(1, &mwTextures[i].id);
        }
        memset(mwTextures, 0, sizeof(mwTextures));
    }

    if (mwFullscreen)
        SetFullscreen(false);

    if (window)
        MwDestroyWidget((MwWidget)window);
    window   = NULL;
    glWidget = NULL;

    free(mwConvertBuffer);
    mwConvertBuffer = NULL;

    if (displayInfo.displays)
        free(displayInfo.displays);
    displayInfo.displays = NULL;

    if (scanlines)
        free(scanlines);
    scanlines = NULL;

    if (imageBuffer)
        free(imageBuffer);
    imageBuffer = NULL;

    isRunning = false;
}

void RenderDevice::UpdateViewport()
{
    if (!mwFullscreen) {
        mwContentX      = 0;
        mwContentY      = 0;
        mwContentWidth  = mwWindowWidth;
        mwContentHeight = mwWindowHeight;
    }
    else {
        // letterboxed: biggest integer scale that fits (sharp pixels), or plain aspect fit if none does
        int32 aspectW = screens[0].size.x > 0 ? screens[0].size.x : videoSettings.pixWidth;
        int32 aspectH = screens[0].size.y > 0 ? screens[0].size.y : SCREEN_YSIZE;
        int32 scale   = 1;
        while (aspectW * (scale + 1) <= mwWindowWidth && aspectH * (scale + 1) <= mwWindowHeight) scale++;
        if (aspectW * scale <= mwWindowWidth && aspectH * scale <= mwWindowHeight) {
            mwContentWidth  = aspectW * scale;
            mwContentHeight = aspectH * scale;
        }
        else if (mwWindowWidth * aspectH > mwWindowHeight * aspectW) {
            mwContentHeight = mwWindowHeight;
            mwContentWidth  = mwWindowHeight * aspectW / aspectH;
        }
        else {
            mwContentWidth  = mwWindowWidth;
            mwContentHeight = mwWindowWidth * aspectH / aspectW;
        }
        mwContentX = (mwWindowWidth - mwContentWidth) / 2;
        mwContentY = (mwWindowHeight - mwContentHeight) / 2;
    }

    if (glWidget)
        MwVaApply((MwWidget)glWidget, MwNx, 0, MwNy, 0, MwNwidth, mwWindowWidth, MwNheight, mwWindowHeight, NULL);
}

void RenderDevice::SetFullscreen(bool32 fullscreen)
{
    if (!window || fullscreen == mwFullscreen)
        return;

    if (fullscreen) {
        mwWindowedX = MwGetInteger((MwWidget)window, MwNx);
        mwWindowedY = MwGetInteger((MwWidget)window, MwNy);

        MwRect screen;
        MwGetScreenSize((MwWidget)window, &screen);

        mwFullscreen = true;
        MwWindowMakeBorderless((MwWidget)window, 1);
        MwVaApply((MwWidget)window, MwNx, screen.x, MwNy, screen.y, MwNwidth, screen.width, MwNheight, screen.height, NULL);
        mwWindowWidth  = screen.width;
        mwWindowHeight = screen.height;
        MwHideCursor((MwWidget)window);
    }
    else {
        mwFullscreen = false;
        MwWindowMakeBorderless((MwWidget)window, 0);
        mwWindowWidth  = videoSettings.windowWidth;
        mwWindowHeight = videoSettings.windowHeight;
        MwVaApply((MwWidget)window, MwNx, mwWindowedX, MwNy, mwWindowedY, MwNwidth, mwWindowWidth, MwNheight, mwWindowHeight, NULL);
    }

    UpdateViewport();
}

void RenderDevice::RefreshWindow()
{
    videoSettings.windowState = WINDOWSTATE_UNINITIALIZED;

    Release(true);

    GetDisplays();

    if (videoSettings.windowed) {
        SetFullscreen(false);
        if (window && (videoSettings.windowWidth != mwWindowWidth || videoSettings.windowHeight != mwWindowHeight)) {
            mwWindowWidth  = videoSettings.windowWidth;
            mwWindowHeight = videoSettings.windowHeight;
            MwVaApply((MwWidget)window, MwNwidth, mwWindowWidth, MwNheight, mwWindowHeight, NULL);
            UpdateViewport();
        }
    }
    else {
        SetFullscreen(true);
    }

    if (!InitGraphicsAPI() || !InitShaders())
        return;

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
}

void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
    if (width)
        *width = mwWindowWidth;
    if (height)
        *height = mwWindowHeight;
}

void RenderDevice::InitFPSCap()
{
    targetFreq = 1000000 / (videoSettings.refreshRate > 0 ? videoSettings.refreshRate : 60);
    curTicks   = 0;
    prevTicks  = 0;
}

bool RenderDevice::CheckFPSCap()
{
    curTicks = (uint64)MwTimeGetTick() * 1000;
    return curTicks >= prevTicks + targetFreq;
}

void RenderDevice::UpdateFPSCap()
{
    // don't try to catch up after a long stall (loading), just carry on from now
    if (curTicks > prevTicks + targetFreq * 4)
        prevTicks = curTicks;
    else
        prevTicks += targetFreq;
}

bool RenderDevice::InitGraphicsAPI()
{
    videoSettings.shaderSupport = false;

    viewSize.x = mwWindowWidth;
    viewSize.y = mwWindowHeight;

    int32 maxPixHeight = 0;
    int32 screenWidth  = 0;
    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        if (videoSettings.pixHeight > maxPixHeight)
            maxPixHeight = videoSettings.pixHeight;

        screens[s].size.y = videoSettings.pixHeight;

        float viewAspect = viewSize.x / viewSize.y;
        screenWidth      = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
        if (screenWidth < videoSettings.pixWidth)
            screenWidth = videoSettings.pixWidth;

        if (customSettings.maxPixWidth && screenWidth > customSettings.maxPixWidth)
            screenWidth = customSettings.maxPixWidth;

        memset(&screens[s].frameBuffer, 0, sizeof(screens[s].frameBuffer));
        SetScreenSize(s, screenWidth, screens[s].size.y);
    }

    pixelSize.x = screens[0].size.x;
    pixelSize.y = screens[0].size.y;
    UpdateViewport();

    if (screenWidth <= 512 && maxPixHeight <= 256) {
        textureSize.x = 512.0;
        textureSize.y = 256.0;
    }
    else {
        textureSize.x = 1024.0;
        textureSize.y = 512.0;
    }

    lastShaderID            = -1;
    engine.inFocus          = 1;
    videoSettings.viewportX = 0;
    videoSettings.viewportY = 0;
    videoSettings.viewportW = 1.0 / viewSize.x;
    videoSettings.viewportH = 1.0 / viewSize.y;

    return true;
}

void RenderDevice::LoadShader(const char *fileName, bool32 linear)
{
    (void)fileName;
    (void)linear;
    PrintLog(PRINT_NORMAL, "This render device does not support shaders!");
}

bool RenderDevice::InitShaders()
{
    for (int32 s = 0; s < SHADER_COUNT; ++s) shaderList[s].linear = false;

    shaderCount            = 1;
    videoSettings.shaderID = 0;
    return true;
}

bool RenderDevice::SetupRendering()
{
    GetDisplays();

    if (!InitGraphicsAPI() || !InitShaders())
        return false;

    int32 size = videoSettings.pixWidth >= SCREEN_YSIZE ? videoSettings.pixWidth : SCREEN_YSIZE;
    scanlines  = (ScanlineInfo *)malloc(size * sizeof(ScanlineInfo));
    memset(scanlines, 0, size * sizeof(ScanlineInfo));

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.dimMax      = 1.0;
    videoSettings.dimPercent  = 1.0;

    return true;
}

void RenderDevice::GetDisplays()
{
    if (!displayInfo.displays)
        displayInfo.displays = (decltype(displayInfo.displays))malloc(sizeof(*displayInfo.displays));

    MwRect screen;
    MwGetScreenSize((MwWidget)window, &screen);
    displayInfo.displays[0].width        = screen.width;
    displayInfo.displays[0].height       = screen.height;
    displayInfo.displays[0].refresh_rate = 60;

    displayCount     = 1;
    displayWidth[0]  = screen.width;
    displayHeight[0] = screen.height;
}

void RenderDevice::ShowCursor(bool32 shown)
{
    if (!shown)
        MwHideCursor((MwWidget)window);
    else
        /* milsko doesn't have a function named MwShowCursor (oops! I should add that!)
         * but MwSetDefault does the same thing as of writing */
        MwSetDefault((MwWidget)window);
}

bool RenderDevice::GetCursorPos(Vector2 *pos)
{
    if (!window)
        return false;
    MwPoint p;
    MwGetCursorCoord((MwWidget)window, &p);
    pos->x = p.x;
    pos->y = p.y;
    return true;
}

void RenderDevice::SetWindowTitle()
{
    MwVaApply((MwWidget)window, MwNtitle, gameVerInfo.gameTitle, NULL);
}

bool RenderDevice::ProcessEvents()
{
    if (!window)
        return false;

    while (MwPending((MwWidget)window)) {
        if (MwStep((MwWidget)window) != 0) {
            // the window was destroyed
            window   = NULL;
            glWidget = NULL;
            mwQuit   = true;
            break;
        }
    }

    if (mwQuit)
        isRunning = false;
    return isRunning;
}

void RenderDevice::ReleaseKeys()
{
    for (int32 k = 0; k < mwHeldKeyCount; ++k) {
#if RETRO_INPUTDEVICE_KEYBOARD
        SKU::ClearKeyState(mwHeldKeys[k]);
#endif
    }
    mwHeldKeyCount = 0;
    mwAltDown      = false;
}

void RenderDevice::OnKey(int32 key, bool32 down)
{
    int32 vk = MilskoKeyToVK(key);
    if (!vk)
        return;

    if (vk == 0xA4)
        mwAltDown = down;

    // keep track of held keys, so they can all be let go when the window loses focus
    int32 held = -1;
    for (int32 k = 0; k < mwHeldKeyCount; ++k) {
        if (mwHeldKeys[k] == vk)
            held = k;
    }

    if (!down) {
        if (held < 0)
            return;
        mwHeldKeys[held] = mwHeldKeys[--mwHeldKeyCount];
#if !RETRO_REV02
        --RSDK::SKU::buttonDownCount;
        if (vk == 0x1B)
            RSDK::SKU::specialKeyStates[0] = false;
        if (vk == 0x0D)
            RSDK::SKU::specialKeyStates[1] = false;
#endif
        if (vk == 0x08)
            engine.gameSpeed = 1;
#if RETRO_INPUTDEVICE_KEYBOARD
        SKU::ClearKeyState(vk);
#endif
        return;
    }

    // key repeat
    if (held >= 0)
        return;
    if (mwHeldKeyCount < (int32)(sizeof(mwHeldKeys) / sizeof(mwHeldKeys[0])))
        mwHeldKeys[mwHeldKeyCount++] = vk;

#if !RETRO_REV02
    ++RSDK::SKU::buttonDownCount;
#endif

    switch (vk) {
        case 0x0D: // return
            if (mwAltDown) {
                videoSettings.windowed ^= 1;
                UpdateGameWindow();
                changedVideoSettings = false;
                break;
            }
#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
            RSDK::SKU::specialKeyStates[1] = true;
#endif
#if RETRO_INPUTDEVICE_KEYBOARD
            SKU::UpdateKeyState(vk);
#endif
            break;

        case 0x1B: // escape
            if (engine.devMenu) {
                if (sceneInfo.state == ENGINESTATE_DEVMENU)
                    CloseDevMenu();
                else
                    OpenDevMenu();
            }
            else {
#if RETRO_INPUTDEVICE_KEYBOARD
                SKU::UpdateKeyState(vk);
#endif
            }
#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
            RSDK::SKU::specialKeyStates[0] = true;
#endif
            break;

        case 0x08: // backspace
            if (engine.devMenu)
                engine.gameSpeed = engine.fastForwardSpeed;
            break;

        default:
#if RETRO_INPUTDEVICE_KEYBOARD
            SKU::UpdateKeyState(vk);
#endif
            break;
    }
}

static bool MilskoEnsureImageBuffer(uint32 **buffer, int32 width, int32 height)
{
    if (width > RETRO_VIDEO_TEXTURE_W || height > RETRO_VIDEO_TEXTURE_H)
        return false;
    if (!*buffer)
        *buffer = (uint32 *)malloc(RETRO_VIDEO_TEXTURE_W * RETRO_VIDEO_TEXTURE_H * sizeof(uint32));
    return *buffer != NULL;
}

void RenderDevice::SetupImageTexture(int32 width, int32 height, uint8 *imagePixels)
{
    if (!MilskoEnsureImageBuffer(&imageBuffer, width, height))
        return;

    memcpy(imageBuffer, imagePixels, width * height * sizeof(uint32));
    imageWidth  = width;
    imageHeight = height;
}

// BT.601 YUV -> ARGB8888. chromaShiftX/Y: 1 for subsampled planes.
static void MilskoConvertYUV(uint32 *dst, int32 width, int32 height, const uint8 *yPlane, const uint8 *uPlane, const uint8 *vPlane, int32 strideY,
                             int32 strideU, int32 strideV, int32 chromaShiftX, int32 chromaShiftY)
{
    for (int32 y = 0; y < height; ++y) {
        const uint8 *yRow = yPlane + y * strideY;
        const uint8 *uRow = uPlane + (y >> chromaShiftY) * strideU;
        const uint8 *vRow = vPlane + (y >> chromaShiftY) * strideV;
        uint32 *out       = dst + y * width;

        for (int32 x = 0; x < width; ++x) {
            int32 c = 298 * (yRow[x] - 16);
            int32 d = uRow[x >> chromaShiftX] - 128;
            int32 e = vRow[x >> chromaShiftX] - 128;

            int32 r = CLAMP((c + 409 * e + 128) >> 8, 0, 255);
            int32 g = CLAMP((c - 100 * d - 208 * e + 128) >> 8, 0, 255);
            int32 b = CLAMP((c + 516 * d + 128) >> 8, 0, 255);

            out[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }
}

void RenderDevice::SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
    if (!MilskoEnsureImageBuffer(&imageBuffer, width, height))
        return;
    MilskoConvertYUV(imageBuffer, width, height, yPlane, uPlane, vPlane, strideY, strideU, strideV, 1, 1);
    imageWidth  = width;
    imageHeight = height;
}

void RenderDevice::SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
    if (!MilskoEnsureImageBuffer(&imageBuffer, width, height))
        return;
    MilskoConvertYUV(imageBuffer, width, height, yPlane, uPlane, vPlane, strideY, strideU, strideV, 1, 0);
    imageWidth  = width;
    imageHeight = height;
}

void RenderDevice::SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
    if (!MilskoEnsureImageBuffer(&imageBuffer, width, height))
        return;
    MilskoConvertYUV(imageBuffer, width, height, yPlane, uPlane, vPlane, strideY, strideU, strideV, 0, 0);
    imageWidth  = width;
    imageHeight = height;
}
