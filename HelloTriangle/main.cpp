// HelloTriangle.cpp : Defines the entry point for the application.
//
#include "framework.h"
#include "HelloTriangle.h"
#include "Game.h"
#include "StepTimer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
//#include <glad/glad_wgl.h>
#include "lib/bel_ImgLoader/include/stb_image.h"
//#include "lib/bel_lodepng/include/lodepng.h"
#include "ApplicationContext.h"
#include "ISoundPlayer.h"
#include "ISoundLoader.h"
#include <Mmsystem.h>
#include "WinSoundManager.h"

#define MAX_LOADSTRING 100

DeclareScopedEnumWithOperatorDefined(VKEY, DUMMY_NAMESPACE(), uint32_t,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    ALT,
    SHIFT,
    LIGHTING);
DeclareConsecutiveType(Key, DUMMY_NAMESPACE(), int, -1)
// Mapping VKey Controller
std::unordered_multimap<VKEY, Key> key_mapping;

using unordered_multimap_citer = std::unordered_multimap<VKEY, Key>::const_iterator;
using unordered_multimap_iter = std::unordered_multimap<VKEY, Key>::iterator;
using key_pair = std::pair<unordered_multimap_citer, unordered_multimap_citer>;

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HDC   ghDC;
HGLRC ghRC;
GLsizei m_ctxWidth, m_ctxHeight;
std::unique_ptr<IApplicationContext> ctx;
ApplicationContext* applicationCtx;
std::unique_ptr<Game> g_game;
utils::unique_ref<utils::WorkerThread<double()>> calcThread(false, "Calc Thread Pool", utils::MODE::MESSAGE_QUEUE_MT, 50, true, 4);
utils::MessageQueue mainThreadQueue;
bool m_isExiting, m_isPause, isSignalSuspend, m_isWaiting, m_isInitDone;
bool firstMouse = true;
std::unordered_map<VKEY, bool> isKeyPressed;
struct SignalKey;
std::vector<utils::Connection> m_connections;
utils::Signal_mt<void(bool&), SignalKey> sig_onWaitingChanged;
utils::Signal<void(), SignalKey> sig_onExport;

#define BLACK_INDEX     0 
#define RED_INDEX       13 
#define GREEN_INDEX     14 
#define BLUE_INDEX      16

/* OpenGL globals, defines, and prototypes */
const int divide = 2;
const float divided = 1.0f / (float) divide;
constexpr int max_scale = 200;
float m_totalTicks = 0;
unsigned int shaderProgram;
unsigned int VBO, VAO, EBO, internalFormat;
unsigned int lightCubeVAO;
unsigned int texture;
unsigned int viewLoc, modelLoc, projectionLoc, lightPosLoc, lightColorLoc, objectColorLoc, viewPosLoc, ambientStrengthLoc;
// camera
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraDirect = glm::vec3(0.0f, 0.0f, 0.0f);
// lighting
glm::vec3 lightPos(1.0f, 1.0f, 1.0f);
glm::vec3 lightColor = glm::vec3(1.0f);
glm::vec3 Right;
glm::vec3 WorldUp = cameraUp;
GLfloat latitude, longitude, latinc, longinc, fov, lastX, lastY;
float ambientStrength = 0.8f;
float k_speed = 1.0f;
float yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
GLdouble radius;

#define GLOBE    1 
#define CYLINDER 2 
#define CONE     3

const char* vertexShaderSource = "#version 460 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec3 aNormal;\n"
//"layout (location = 2) in vec2 aTexCoord;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out vec3 FragPos;\n"
"out vec3 Normal;\n"
//"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
"   FragPos = vec3(model * vec4(aPos, 1.0));\n"
"   Normal = mat3(transpose(inverse(model))) * aNormal;\n"
"   gl_Position = projection * view * vec4(FragPos, 1.0);\n"
//"   TexCoord = aTexCoord;\n"
"}\n\0";

const char* fragmentShaderSource = "#version 460 core\n"
"out vec4 FragColor;\n"
"in vec3 Normal;\n"
"in vec3 FragPos;\n"
"uniform vec3 lightPos;\n"
"uniform vec3 viewPos;\n"
"uniform vec3 lightColor;\n"
"uniform vec3 objectColor;\n"
"uniform float ambientStrength;\n"
//"in vec2 TexCoord;\n"
//"uniform sampler2D ourTexture;"
"void main()\n"
"{\n"
//"   FragColor = texture(ourTexture, TexCoord);\n"
// ambient
"     vec3 ambient = ambientStrength * lightColor;\n"
// diffuse
"     vec3 norm = normalize(Normal);\n"
"     vec3 lightDir = normalize(lightPos - FragPos);\n"
"     float diff = max(dot(norm, lightDir), 0.0);\n"
"     vec3 diffuse = diff * lightColor;\n"
// specular
"     float specularStrength = 0.5;\n"
"     vec3 viewDir = normalize(viewPos - FragPos);\n"
"     vec3 reflectDir = reflect(-lightDir, norm);\n"
"     float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
"     vec3 specular = specularStrength * spec * lightColor;\n"
"     vec3 result = (ambient + diffuse + specular) * objectColor;\n"
"     FragColor = vec4(result, 1.0);\n"
"}\n\0";

// Forward declarations of functions included in this code module:
void                CleanResource();
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
double              Calc_pi_MT(int n);
VKEY                GetVKeyMapping(int i_key);
double              Move(VKEY move);
int                 loadOpenGLFunctions();
void                ExitGame(HWND hWnd);
GLvoid              initializeShaderProgram(GLsizei ctxWidth, GLsizei ctxHeight);
GLvoid              drawScene(float deltaTime);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    utils::TimerDelayer suspendDelayer(10000);
    ctx = std::make_unique<ApplicationContext>();
    applicationCtx = static_cast<ApplicationContext*>(ctx.get());
    assert(applicationCtx);
    g_game = std::make_unique<Game>(*ctx);

    utils::unique_ref<ISoundLoader> soundLoader(new WinSoundManager());
    m_connections.push_back(g_game->sig_onTick.Connect(&drawScene));
    m_connections.push_back(sig_onWaitingChanged.Connect([](bool& o_isWaiting)
    {
        o_isWaiting = !o_isWaiting;
    }));
    m_connections.push_back(sig_onExport.Connect([&]()
    {
        if (!m_isWaiting)
        {
            utils::Access<SignalKey>(sig_onWaitingChanged).Emit(m_isWaiting);
            utils::WorkerThread().CreateWorkerThread([&]()
            {
                utils::unique_ref<ISoundPlayer> soundPlayer = soundLoader->Load("Assets/emotion.mp3");
                soundPlayer->PlayFromStart();
                DEBUG_LOG("Debug", "Meter Performance");
                double result = Calc_pi_MT(1E10);
                DEBUG_LOG("Debug", "test: {}", result);
                MessageBoxA(0, utils::Format("Pi: {}", result).c_str(), "Result", MB_SERVICE_NOTIFICATION);
                utils::Access<SignalKey>(sig_onWaitingChanged).Emit(m_isWaiting);
                return 0;
            }).expect("")->Detach();
        }
    }));
    m_connections.push_back(ctx->sig_onKeyDown.Connect([](int key)
    {
        VKEY vkey = GetVKeyMapping(key);
        if (utils::Contains(vkey, { VKEY::DOWN, VKEY::LEFT, VKEY::RIGHT, VKEY::UP }))
        {
            if (!isKeyPressed[vkey])
            {
                cameraFront = cameraDirect - cameraPos;
                calcThread->PushCallback(Move, vkey);
                isKeyPressed[vkey] = true;
            }
        }
    }));
    m_connections.push_back(ctx->sig_onKeyUp.Connect([](int key)
    {
        VKEY vkey = GetVKeyMapping(key);
        if (utils::Contains(vkey, { VKEY::DOWN, VKEY::LEFT, VKEY::RIGHT, VKEY::UP }))
        {
            isKeyPressed[vkey] = false;
        }
    }));

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_HELLOTRIANGLE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HELLOTRIANGLE));

    MSG msg = {};

    // Main message loop:
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            // main loop here
            if (!(isSignalSuspend || m_isPause))
            {
                g_game->SyncRenderThread();
                calcThread->Dispatch();
                mainThreadQueue.Dispatch();
            }
            else
            {
                Sleep(utils::k_updateIntervalMilliseconds);
            }
        }
    }

    utils::Log::d("Main Thread", "Exiting Game");
    g_game.reset();
    utils::Log::d("Main Thread", "Exited Render thread");
    calcThread->StopAsync();
    utils::Log::Wait();

    return (int) msg.wParam;
}

void CleanResource()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
}

void InitKeyController()
{
    key_mapping.emplace(VKEY::UP, 0x57);
    key_mapping.emplace(VKEY::UP, VK_UP);
    key_mapping.emplace(VKEY::DOWN, 0x53);
    key_mapping.emplace(VKEY::DOWN, VK_DOWN);
    key_mapping.emplace(VKEY::LEFT, 0x41);
    key_mapping.emplace(VKEY::LEFT, VK_LEFT);
    key_mapping.emplace(VKEY::RIGHT, 0x44);
    key_mapping.emplace(VKEY::RIGHT, VK_RIGHT);
    key_mapping.emplace(VKEY::LIGHTING, 0x46);
    key_mapping.emplace(VKEY::ALT, VK_MENU);
}

std::vector<int> GetVKeyMapping(VKEY i_vkey)
{
    std::vector<int> result;
    key_pair _key = key_mapping.equal_range(i_vkey);
    unordered_multimap_citer const_iter = _key.first;
    for (; const_iter != _key.second; const_iter++)
    {
        result.push_back(const_iter->second.value);
    }
    return result;
}

VKEY GetVKeyMapping(int i_key)
{
    auto vkeyIt = std::find_if(key_mapping.begin(), key_mapping.end(), [i_key](const std::pair<VKEY, Key>& i_vkeyPair)
    {
        return i_vkeyPair.second == i_key && i_key != Key::k_invalid;
    });
    return vkeyIt != key_mapping.end() ? vkeyIt->first : VKEY::_LAST;
}

void SetVKeyMapping(VKEY i_vkey, int i_mapKey)
{
    key_mapping.erase(i_vkey);
    key_mapping.emplace(i_vkey, i_mapKey);
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HELLOTRIANGLE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_HELLOTRIANGLE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

GLvoid resize(GLsizei width, GLsizei height)
{
    m_ctxWidth = width;
    m_ctxHeight = height;
    glViewport(0, 0, width, height);
    g_game->ChangeModeRenderThread(utils::MODE::UPDATE_CALLBACK);
    g_game->PauseRenderThread(m_isPause);
}

void createGLContext()
{
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
        PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
        32,                   // Colordepth of the framebuffer.
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,                   // Number of bits for the depthbuffer
        8,                    // Number of bits for the stencilbuffer
        0,                    // Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    int  PixelFormat;
    PixelFormat = ChoosePixelFormat(ghDC, &pfd);
    SetPixelFormat(ghDC, PixelFormat, &pfd);

    ghRC = wglCreateContext(ghDC);
    wglMakeCurrent(ghDC, ghRC);

    //---create real context
    if (loadOpenGLFunctions() == -1)
    {
        MessageBoxA(0, "glad fail", "loadOpenGLFunctions", 0);
        throw std::runtime_error("ERROR::LOAD_OPEN_GL_FUNCTIONS::FAILED");
    }
}

int loadOpenGLFunctions() 
{
    // glad: load all OpenGL function pointers
    // ---------------------------------------
    /*if (!gladLoadWGL(ghDC))
    {
        utils::Log::e("gladLoader", "Failed to initialize GLADLoadWGL");
        return -1;
    }*/

    if (!gladLoadGL())
    {
        utils::Log::e("gladLoader", "Failed to initialize GLADLoadGL");
        return -1;
    }
    return 1;
}

double Calc_pi(int n, int start, int skip)
{
    double result = 0.0;
    for (int i = start; i < n; i+= skip)
    {
        int sign = std::pow(-1, i);
        double term = 1.0 / (i * 2 + 1);
        result += sign * term;
    }
    return result * 4;
}

double Calc_pi_MT(int n)
{
    double result = 0.0;
    unsigned int CONCURRENCY = std::thread::hardware_concurrency();
    std::vector<utils::MessageHandle<double>> results;
    for (unsigned int i = 0; i < CONCURRENCY; i++)
    {
        utils::MessageHandle<double> result = calcThread->PushCallback(&Calc_pi, n, i, CONCURRENCY);
        results.push_back(std::move(result));
    }
    utils::Connection connection = calcThread->sig_onRunFinished.Connect([&results]()
    {
        for (utils::MessageHandle<double>& result : results)
        {
            result.Cancel();
        }
    });
    for (utils::MessageHandle<double>& futureResult : results)
    {
        Result<double, utils::MessageHandleERR> messageHandleResult = futureResult.GetResult();
        if (messageHandleResult.isOk())
        {
            result += messageHandleResult.unwrap();
        }
        else
        {
            ERROR_LOG("Debug", "error: {}", messageHandleResult.unwrapErr());
        }
    }
    return result;
}

double Move(VKEY move)
{
    float deltaTime = 0;
    float cameraSpeed = 0;
    float timeSleep = 0;
    utils::clock_timepoint<utils::steady_clock, utils::duration<double>> currentTP = utils::steady_clock::now();
    while (!m_isExiting && isKeyPressed[move])
    {
        deltaTime = g_game->GetElapsedSeconds();
        currentTP += utils::duration<double>(deltaTime);
        std::this_thread::sleep_until(currentTP);
        cameraSpeed = k_speed * deltaTime;
        switch (move)
        {
        case VKEY::UP:
            if (isKeyPressed[VKEY::SHIFT] && !(isKeyPressed[VKEY::LEFT] || isKeyPressed[VKEY::RIGHT]))
            {
                cameraSpeed = 2.5 * k_speed * deltaTime;
            }
            if (isKeyPressed[VKEY::DOWN])
            {
                cameraSpeed = 0;
            }
            cameraPos += cameraSpeed * cameraFront;
            cameraDirect = cameraPos + cameraFront;
        break;
        case VKEY::LEFT:
            if (isKeyPressed[VKEY::RIGHT])
            {
                cameraSpeed = 0;
            }
            cameraPos -= Right * cameraSpeed;
            cameraDirect = cameraPos + cameraFront;
        break;
        case VKEY::DOWN:
            if (isKeyPressed[VKEY::UP])
            {
                cameraSpeed = 0;
            }
            cameraPos -= cameraSpeed * cameraFront;
            cameraDirect = cameraPos + cameraFront;
        break;
        case VKEY::RIGHT:
            if (isKeyPressed[VKEY::LEFT])
            {
                cameraSpeed = 0;
            }
            cameraPos += Right * cameraSpeed;
            cameraDirect = cameraPos + cameraFront;
        break;
        }
    }
    return 0;
}

void SetMiddleCursorPos(HWND hWnd)
{
    RECT rect;
    POINT ptClientUL;              // client upper left corner 
    POINT ptClientLR;              // client lower right corner
    GetClientRect(hWnd, &rect);
    ptClientUL.x = rect.left;
    ptClientUL.y = rect.top;
    // Add one to the right and bottom sides, because the 
    // coordinates retrieved by GetClientRect do not 
    // include the far left and lowermost pixels.
    ptClientLR.x = rect.right + 1;
    ptClientLR.y = rect.bottom + 1;
    ClientToScreen(hWnd, &ptClientUL);
    ClientToScreen(hWnd, &ptClientLR);
    // Copy the client coordinates of the client area 
    // to the rcClient structure. Confine the mouse cursor 
    // to the client area by passing the rcClient structure 
    // to the ClipCursor function.
    SetRect(&rect, ptClientUL.x, ptClientUL.y,
        ptClientLR.x, ptClientLR.y);
    SetCursorPos(rect.left + (m_ctxWidth / 2), rect.top + (m_ctxHeight / 2));
}

void LockCursor(bool i_isLock, HWND hWnd)
{
    if (i_isLock)
    {
        RECT rect;
        POINT ptClientUL;              // client upper left corner 
        POINT ptClientLR;              // client lower right corner
        // Capture mouse input.
        SetCapture(hWnd);
        ShowCursor(false);
        // Retrieve the screen coordinates of the client area, 
        // and convert them into client coordinates. 
        GetClientRect(hWnd, &rect);
        ptClientUL.x = rect.left;
        ptClientUL.y = rect.top;
        // Add one to the right and bottom sides, because the 
        // coordinates retrieved by GetClientRect do not 
        // include the far left and lowermost pixels.
        ptClientLR.x = rect.right + 1;
        ptClientLR.y = rect.bottom + 1;
        ClientToScreen(hWnd, &ptClientUL);
        ClientToScreen(hWnd, &ptClientLR);
        // Copy the client coordinates of the client area 
        // to the rcClient structure. Confine the mouse cursor 
        // to the client area by passing the rcClient structure 
        // to the ClipCursor function.
        SetRect(&rect, ptClientUL.x, ptClientUL.y,
            ptClientLR.x, ptClientLR.y);
        ClipCursor(&rect);
    }
    else
    {
        firstMouse = true;
        SetMiddleCursorPos(hWnd);
        ClipCursor(NULL);
        ShowCursor(true);
        ReleaseCapture();
    }
}

void MouseCallback(HWND hWnd, double i_xpos, double i_ypos)
{
    /*if (cameraDirect == glm::vec3(0.0f, 0.0f, 0.0f))
    {
        return;
    }*/
    float xpos = static_cast<float>(i_xpos);
    float ypos = static_cast<float>(i_ypos);
    POINT pCursor;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;


    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
    cameraDirect = cameraPos + cameraFront;
    // also re-calculate the Right and Up vector
    Right = glm::normalize(glm::cross(cameraFront, WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    cameraUp = glm::normalize(glm::cross(Right, cameraFront));
}

void ScrollCallback(HWND hWnd, double yoffset)
{
    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    short zDelta = 0;
    RECT rect;
    switch (message)
    {
    case WM_CREATE:
    {
        InitKeyController();
        ghDC = GetDC(hWnd);
        g_game->ChangeModeRenderThread(utils::MODE::MESSAGE_QUEUE, 2);
        g_game->PushMessage(&createGLContext);

        GetClientRect(hWnd, &rect);
            
        g_game->PushMessage(Game::MessageType(&initializeShaderProgram, (GLsizei)rect.right, (GLsizei)rect.bottom));
        m_isInitDone = false;
    }
    break;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
        //if (MessageBoxA(0, "Do you want to exit game?", "Warning", MB_YESNO) == IDYES)
        {
            ExitGame(hWnd);
        }
        break;
        case IDM_EXPORT:
        {
            utils::Access<SignalKey>(sig_onExport).Emit();
        }
        break;
        case IDM_PAUSE:
        {
            m_isPause = !g_game->GetThread()->IsPaused();
            m_isPause ? applicationCtx->Suspend() : applicationCtx->Resume();
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_MOUSEMOVE:
        POINTS posCursor = MAKEPOINTS(lParam);
        if (posCursor.x <= 0 || posCursor.y <= 0 || posCursor.x >= m_ctxWidth || posCursor.y >= m_ctxHeight)
        {
            SetMiddleCursorPos(hWnd);
            firstMouse = true;
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        if (!m_isPause)
        {
            mainThreadQueue.PushCallback(&MouseCallback, hWnd, posCursor.x, posCursor.y);
        }
    break;
    case WM_MOUSEWHEEL:
        zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        mainThreadQueue.PushCallback(&ScrollCallback, hWnd, zDelta);
    break;
    case WM_KEYDOWN:
        applicationCtx->InvokeKeyDown(wParam);
        if (wParam == VK_SHIFT && !isKeyPressed[VKEY::SHIFT])
        {
            isKeyPressed[VKEY::SHIFT] = true;
        }
    break;
    case WM_KEYUP:
        applicationCtx->InvokeKeyUp(wParam);
        if (wParam == VK_SHIFT)
        {
            isKeyPressed[VKEY::SHIFT] = false;
        }
        if (g_game->IsAny(wParam, GetVKeyMapping(VKEY::LIGHTING)))
        {
            bool isLightOn = lightColor == glm::vec3(1.0f);
            lightColor = isLightOn ? glm::vec3(0.0f) : glm::vec3(1.0f);
        }
    break;
    case WM_SYSKEYDOWN:
        if (wParam == VK_MENU && !isKeyPressed[VKEY::ALT])
        {
            isKeyPressed[VKEY::ALT] = true;
            LockCursor(false, hWnd);
        }
    break;
    case WM_SYSKEYUP:
        if (wParam == VK_MENU)
        {
            isKeyPressed[VKEY::ALT] = false;
            LockCursor(true, hWnd);
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_SIZE:
    {
        if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
        {
            GetClientRect(hWnd, &rect);
            g_game->ChangeModeRenderThread(utils::MODE::MESSAGE_QUEUE, 1);
            if (!g_game->GetThread()->IsEmptyQueue())
            {
                if (!m_isInitDone)
                {
                    g_game->SyncRenderThread();
                    m_isInitDone = true;
                }
                g_game->CleanQueueInRenderThread();
            }
            g_game->PushMessage(Game::MessageType(& resize, (GLsizei)rect.right, (GLsizei)rect.bottom));
        }
    }
    break;
    case WM_ACTIVATEAPP:
    {
        if (wParam)
        {
            isKeyPressed[VKEY::ALT] = false;
            LockCursor(true, hWnd);
            isSignalSuspend = false;
            if (!m_isPause)
            {
                applicationCtx->Resume();
                for (VKEY iterKey = VKEY::_FIRST; iterKey != VKEY::_LAST; ++iterKey)
                {
                    isKeyPressed[iterKey] = false;
                }
            }
        }
        else
        {
            isSignalSuspend = true;
            applicationCtx->Suspend();
            if (!isKeyPressed[VKEY::ALT])
            {
                LockCursor(false, hWnd);
            }
        }
    }
    break;
    case WM_DESTROY:
    PostQuitMessage(0);
    break;
    case WM_CLOSE:
    if (MessageBoxA(0, "Do you want to exit game?", "Warning", MB_YESNO) == IDYES)
    {
        ExitGame(hWnd);
    }
    return 0;
    break;
    default:
    return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    HWND hwndCtrl = NULL;
    std::string ambientStr{};
    std::string fpsStr{};
    unsigned int result = 0;
    bool isValid = false;
    float fps = g_game->GetFramesPerSecond();
    switch (message)
    {
    case WM_INITDIALOG:
        isKeyPressed[VKEY::ALT] = false;
        hwndCtrl = GetDlgItem(hDlg, IDC_KEYUP);
        SendMessage(hwndCtrl, HKM_SETHOTKEY, MAKEWORD(GetVKeyMapping(VKEY::UP)[0], 0), 0);
        hwndCtrl = GetDlgItem(hDlg, IDC_KEYDOWN);
        SendMessage(hwndCtrl, HKM_SETHOTKEY, MAKEWORD(GetVKeyMapping(VKEY::DOWN)[0], 0), 0);
        hwndCtrl = GetDlgItem(hDlg, IDC_KEYLEFT);
        SendMessage(hwndCtrl, HKM_SETHOTKEY, MAKEWORD(GetVKeyMapping(VKEY::LEFT)[0], 0), 0);
        hwndCtrl = GetDlgItem(hDlg, IDC_KEYRIGHT);
        SendMessage(hwndCtrl, HKM_SETHOTKEY, MAKEWORD(GetVKeyMapping(VKEY::RIGHT)[0], 0), 0);
        hwndCtrl = GetDlgItem(hDlg, IDC_LIGHT);
        SendMessage(hwndCtrl, HKM_SETHOTKEY, MAKEWORD(GetVKeyMapping(VKEY::LIGHTING)[0], 0), 0);
        hwndCtrl = GetDlgItem(hDlg, IDC_EDIT1);
        SetWindowTextA(hwndCtrl, utils::Format("{}", ambientStrength).c_str());
        hwndCtrl = GetDlgItem(hDlg, IDC_EDIT2);
        SetWindowTextA(hwndCtrl, utils::Format("{}", fps).c_str());
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            WORD wHotkey;
            hwndCtrl = GetDlgItem(hDlg, IDC_KEYUP);
            wHotkey = (WORD)SendMessage(hwndCtrl, HKM_GETHOTKEY, 0, 0);
            SetVKeyMapping(VKEY::UP, wHotkey);
            hwndCtrl = GetDlgItem(hDlg, IDC_KEYDOWN);
            wHotkey = (WORD)SendMessage(hwndCtrl, HKM_GETHOTKEY, 0, 0);
            SetVKeyMapping(VKEY::DOWN, wHotkey);
            hwndCtrl = GetDlgItem(hDlg, IDC_KEYLEFT);
            wHotkey = (WORD)SendMessage(hwndCtrl, HKM_GETHOTKEY, 0, 0);
            SetVKeyMapping(VKEY::LEFT, wHotkey);
            hwndCtrl = GetDlgItem(hDlg, IDC_KEYRIGHT);
            wHotkey = (WORD)SendMessage(hwndCtrl, HKM_GETHOTKEY, 0, 0);
            SetVKeyMapping(VKEY::RIGHT, wHotkey);
            hwndCtrl = GetDlgItem(hDlg, IDC_LIGHT);
            wHotkey = (WORD)SendMessage(hwndCtrl, HKM_GETHOTKEY, 0, 0);
            SetVKeyMapping(VKEY::LIGHTING, wHotkey);
            hwndCtrl = GetDlgItem(hDlg, IDC_EDIT1);
            result = GetWindowTextA(hwndCtrl, ambientStr.data(), 5);
            isValid = !std::regex_search(ambientStr.data(), std::regex("([^.0-9]+)"), std::regex_constants::match_any);
            if (result > 0 && isValid)
            {
                ambientStrength = std::stof(ambientStr);
            }
            else
            {
                ERROR_LOG("About", "Get Ambient Strength Failed!");
            }
            hwndCtrl = GetDlgItem(hDlg, IDC_EDIT2);
            result = GetWindowTextA(hwndCtrl, fpsStr.data(), 5);
            isValid = !std::regex_search(fpsStr.data(), std::regex("([^.0-9]+)"), std::regex_constants::match_any);
            if (result > 0 && isValid)
            {
                fps = std::stof(fpsStr);
                applicationCtx->ChangeFPSLimit((short)fps);
            }
            else
            {
                ERROR_LOG("About", "Get FPS Failed!");
            }
            EndDialog(hDlg, LOWORD(wParam));
            LockCursor(true, GetParent(hDlg));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            LockCursor(true, GetParent(hDlg));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void ExitGame(HWND hWnd)
{
    m_isExiting = true;
    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    g_game->ChangeModeRenderThread(utils::MODE::MESSAGE_QUEUE);
    g_game->PushMessage(&CleanResource);

    if (ghRC)
    {
        g_game->PushMessage(Game::MessageType(&wglDeleteContext, ghRC));
    }
    if (ghDC)
    {
        g_game->PushMessage(Game::MessageType(&ReleaseDC, hWnd, ghDC));
    }
    g_game->SyncRenderThread();
    DestroyWindow(hWnd);
}

GLvoid drawScene(float deltaTime)
{
    m_totalTicks += deltaTime;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind Texture
    //glBindTexture(GL_TEXTURE_2D, texture);
    glUseProgram(shaderProgram);

    // camera/view transformation
    glm::mat4 view = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
    //lightPos = cameraPos;

    glm::mat4 projection = glm::perspective(glm::radians(fov), (float)m_ctxWidth / (float)m_ctxHeight, 0.1f, 100.0f);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    view = glm::lookAt(cameraPos, cameraDirect, cameraUp);

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 transformColor = glm::vec3(0.39f, 0.0f, 0.39f);
    // make sure to initialize matrix to identity matrix first
    //float scale = ((sin(timer.GetTotalSeconds()) + 1) * 0.5f * divided) + (1 - (double)divided);
    /*float ratio = ctxWidth * 1.0f / ctxHeight;
    float new_width = 1.0f, new_height = 1.0f;
    if (ratio > 1)
    {
        new_width = 1.0f / ratio;
    }
    else
    {
        new_height = ratio;
    }
    transform = glm::scale(transform, glm::vec3(new_width, new_height, 1.0f));*/
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(objectColorLoc, 1, glm::value_ptr(transformColor));
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
    glUniform1f(ambientStrengthLoc, ambientStrength);
    //setup light position
    glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
    // Render cube
    glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
    glDrawArrays(GL_TRIANGLES, 0, 36);

    //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    //glBindVertexArray(0); // no need to unbind it every time

    //front mini model
    float modelAspect = 0.4;
    float radius = 2.0f;
    float miniModelX = static_cast<float>(sin(m_totalTicks) * radius);
    float miniModelZ = static_cast<float>(cos(m_totalTicks) * radius);
    model = glm::translate(model, glm::vec3(miniModelX, (modelAspect - 1.0f) / 2, miniModelZ));
    model = glm::scale(model, glm::vec3(modelAspect, modelAspect, modelAspect));
    glm::vec3 convertToVec3(model[3].x, model[3].y, model[3].z);
    lightPos = convertToVec3;
    transformColor = glm::vec3(1.0f, 1.0f, 1.0f);

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(objectColorLoc, 1, glm::value_ptr(transformColor));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    SwapBuffers(ghDC);
}

GLvoid initializeShaderProgram(GLsizei ctxWidth, GLsizei ctxHeight)
{
    m_ctxWidth = ctxWidth;
    m_ctxHeight = ctxHeight;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    // build and compile our shader program
    // ------------------------------------
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        utils::Log::e("OPENGLERR", std::format("ERROR::SHADER::VERTEX::COMPILATION_FAILED ", infoLog).c_str());
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        utils::Log::e("OPENGLERR", std::format("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED ", infoLog).c_str());
    }
    // link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        utils::Log::e("OPENGLERR", std::format("ERROR::SHADER::PROGRAM::LINKING_FAILED {}", infoLog).c_str());
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    //float vertices[] = {
    //    // postions            // norm              // texCoords
    //     0.5f,  0.5f,  0.5f,   1.0f,  1.0f,  1.0f,  //3.0f, 3.0f,     // top right outer
    //     0.5f, -0.5f,  0.5f,   1.0f, -1.0f,  1.0f,  //3.0f, 0.0f,     // bottom right outer
    //    -0.5f, -0.5f,  0.5f,  -1.0f, -1.0f,  1.0f,  //0.0f, 0.0f,     // bottom left outer
    //    -0.5f,  0.5f,  0.5f,  -1.0f,  1.0f,  1.0f,  //0.0f, 3.0f,     // top left outer
    //     0.5f,  0.5f, -0.5f,   1.0f,  1.0f, -1.0f,  //3.0f, 3.0f,     // top right inner
    //     0.5f, -0.5f, -0.5f,   1.0f, -1.0f, -1.0f,  //3.0f, 0.0f,     // bottom right inner
    //    -0.5f, -0.5f, -0.5f,  -1.0f, -1.0f, -1.0f,  //0.0f, 0.0f,     // bottom left inner
    //    -0.5f,  0.5f, -0.5f,  -1.0f,  1.0f, -1.0f,  //0.0f, 3.0f,     // top left inner
    //};

    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    //glGenBuffers(1, &EBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    /*glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);*/

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    //glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(6 * sizeof(float)));
    //glEnableVertexAttribArray(2);

    //glGenTextures(1, &texture);
    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, texture);

    //// set the texture wrapping parameters
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //// set texture filtering parameters
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //enum class imageChannel
    //{
    //    RGB = 3,
    //    RGBA
    //};

    //int width, height, nrChannels, desiredChannel;
    //stbi_set_flip_vertically_on_load(true);
    //unsigned char* data = stbi_load("Assets/OK!.png", &width, &height, &nrChannels, 0);
    //desiredChannel = nrChannels; // format color channels
    //switch (desiredChannel)
    //{
    //case static_cast<int>(imageChannel::RGBA):
    //{
    //    internalFormat = GL_RGBA;
    //}
    //break;
    //case static_cast<int>(imageChannel::RGB):
    //{
    //    internalFormat = GL_RGB;
    //}
    //break;
    //}
    //if (data)
    //{
    //    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, internalFormat, GL_UNSIGNED_BYTE, data);
    //    glGenerateMipmap(GL_TEXTURE_2D);
    //    utils::Log::i("initializeShaderProgram", std::format("width:{}, height:{}, nrChannels:{}", width, height, nrChannels).c_str());
    //}
    //else
    //{
    //    utils::Log::e("initializeShaderProgram", "Failed to load Texture!!!");
    //}
    //// note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    //stbi_image_free(data);

    // pass projection matrix to shader (as projection matrix rarely changes there's no need to do this per frame)
    // -----------------------------------------------------------------------------------------------------------
    fov = 45.0f;
    lastX = ctxWidth / 2.0f;
    lastY = ctxHeight / 2.0f;

    viewLoc = glGetUniformLocation(shaderProgram, "view");
    modelLoc = glGetUniformLocation(shaderProgram, "model");
    objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
    viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    ambientStrengthLoc = glGetUniformLocation(shaderProgram, "ambientStrength");
    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    //glBindVertexArray(0);


    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}