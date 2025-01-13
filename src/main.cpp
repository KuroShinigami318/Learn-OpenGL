// HelloTriangle.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
#include "async_utils.h"
#include "ApplicationContext.h"
#include "FrameThread.h"
#include "framework.h"
#include "Game.h"
#include "LearnOpenGL.h"
#include "Log.h"
#include "StepTimer.h"
#include "system_clock.h"
#include "TimerDelayer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
//#include <glad/glad_wgl.h>
#include "stb_image.h"
//#include "lodepng.h"
#include "InputParser.h"
#include "ISoundPlayer.h"
#include "ISoundLoader.h"
#include "SoundManager.h"
#include "ThreadControl.h"
#include "HeartBeats.h"

#define MAX_LOADSTRING 100

DeclareScopedEnumWithOperatorDefined(VKEY, DUMMY_NAMESPACE, uint32_t,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    ALT,
    SHIFT,
    LIGHTING);
DeclareConsecutiveType(Key, int, -1)
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
std::mutex mutex;
std::condition_variable cv;
utils::unique_ref<utils::message_threadpool> calcThread(utils::threadpool_config{ 4, "Calc Thread Pool" });
std::unique_ptr<utils::IHeartBeats> heart(new utils::HeartBeats(0, utils::BPS()));
bool m_isExiting, m_isPause, isSignalSuspend, m_isInitDone = false;
bool firstMouse = true;
std::unordered_map<VKEY, bool> isKeyPressed;
utils::Signal<void()> sig_onExport;

#define BLACK_INDEX     0 
#define RED_INDEX       13 
#define GREEN_INDEX     14 
#define BLUE_INDEX      16

/* OpenGL globals, defines, and prototypes */
const int divide = 2;
const float divided = 1.0f / (float) divide;
constexpr int max_scale = 200;
float m_totalTicks = 0;
float m_clockRatio = 1.0;
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
void                Calc_pi_MT(double n, std::function<void(const double&)> callback);
VKEY                GetVKeyMapping(int i_key);
void                SetMiddleCursorPos(HWND hWnd);
void                Move(VKEY move, float deltaTime);
int                 loadOpenGLFunctions();
void                ExitGame(HWND hWnd);
GLvoid              initializeShaderProgram(GLsizei ctxWidth, GLsizei ctxHeight);
void                FramePrologue(float deltaTime);
void                FrameEpilogue();
void                MovementUpdate(float delta);
GLvoid              drawScene();

struct CalculateVirtualCursorPoint
{
    POINT Get(HWND hWnd)
    {
        POINT pCursor;
        if (GetCursorPos(&pCursor))
        {
            ScreenToClient(hWnd, &pCursor);
        }

        virtualPosition.x += pCursor.x - previousPosition.x;
        virtualPosition.y += pCursor.y - previousPosition.y;
        if (pCursor.x <= 0 || pCursor.y <= 0 || pCursor.x >= m_ctxWidth || pCursor.y >= m_ctxHeight)
        {
            SetMiddleCursorPos(hWnd);
            if (GetCursorPos(&pCursor))
            {
                ScreenToClient(hWnd, &pCursor);
            }
        }
        previousPosition = pCursor;
        return virtualPosition;
    }

    void Reset(POINT updatePosition)
    {
        if (firstReset)
        {
            previousPosition = updatePosition;
            virtualPosition = updatePosition;
            firstReset = false;
        }
    }

    POINT previousPosition{ 0, 0 };
    POINT virtualPosition{ 0, 0 };
    bool firstReset = true;
};

std::array<VKEY, 4> s_movementKeys = { VKEY::DOWN, VKEY::LEFT, VKEY::RIGHT, VKEY::UP };
utils::MessageSink_mt mainThreadQueue;
std::unique_ptr<utils::SystemClock> s_clock;
utils::FrameThread<void(float)> frameThread{ {FramePrologue}, {FrameEpilogue} };
utils::MessageSink_mt& nextFrameQueue = frameThread.GetNextFrameMessageQueue();
utils::MessageSink& thisFrameQueue = frameThread.GetFrameMessageQueue();
utils::unique_ref<CalculateVirtualCursorPoint> virtualCursorPoint{new CalculateVirtualCursorPoint()};
utils::message_thread s_inputThread{utils::thread_config("input thread")};
utils::async_waitable<std::string> s_inputResult;

std::string GetInput();
void InitCommands();
void ProcessInput(const std::string&);

int main(int argc, char** argv)
{
    ctx = std::make_unique<ApplicationContext>();
    applicationCtx = static_cast<ApplicationContext*>(ctx.get());
    ASSERT(applicationCtx);
    ctx->sig_onFPSChanged.Connect([](short fps)
    {
        heart.reset(new utils::HeartBeats(fps, utils::BPS()));
    }).Detach();

    InputParser parser(argc, argv);
    std::string folder("assets");
    if (InputParser::position_t positionParser = parser.HaveInputOptions(InputOptions({ "-folder" })))
    {
        folder = parser.ExtractValue(positionParser);
    }

    InitCommands();
    g_game = std::make_unique<Game>(*ctx, nextFrameQueue);
    Game::LoadResult loadResult = g_game->LoadPlaylist(folder);
    ASSERT_PLAIN_MSG(loadResult.isOk(), "Load Playlist failed: {}", loadResult.unwrapErrOr(Game::LoadErrorCode::InvalidFolder));

    utils::async(nextFrameQueue, []()
    {
        s_clock = std::make_unique<utils::SystemClock>();
        ctx->soundManager.SetThreadId(utils::GetCurrentThreadID());
        s_clock->sig_onTick.Connect(&MovementUpdate).Detach();
        s_clock->sig_onTick.Connect(&SoundManager::Update, applicationCtx->soundManager).Detach();
        s_clock->sig_onTick.Connect([](float)
        {
            if (s_inputResult.HasFinished())
            {
                std::string& input = *s_inputResult.GetPtrResult().unwrap();
                std::transform(input.begin(), input.end(), input.begin(), [](char c) {return std::tolower(c); });
                ProcessInput(input);
            }
            if (!s_inputResult.IsInitialized() || s_inputResult.HasFinished())
            {
                s_inputResult = utils::async(s_inputThread, GetInput);
            }
        }).Detach();
    });

    utils::async_waitable<utils::async_waitable<void>> closeSoundWaitable;
    sig_onExport.Connect([&]()
    {
        if (closeSoundWaitable.IsInitialized() && !closeSoundWaitable.HasFinished()) return;
        utils::unique_ref<ISoundPlayer> soundPlayer = applicationCtx->soundManager.Load("assets/emotion.mp3").expect("Unexpected error");
        soundPlayer->PlayFromStart().ignoreResult();
        DEBUG_LOG("Debug", "Meter Performance");
        std::function<void(const double&)> callback = [](const double& result)
        {
            DEBUG_LOG("Debug", "test: {}", result);
            MessageBoxA(0, utils::Format("Pi: {}", result).c_str(), "Result", MB_SERVICE_NOTIFICATION);
        };
        Calc_pi_MT(1E9, callback);
        std::function<void(utils::unique_ref<ISoundPlayer>)> closedSound = [](utils::unique_ref<ISoundPlayer>) {};
        closeSoundWaitable = utils::async(*calcThread, [closedSound](utils::unique_ref<ISoundPlayer> soundPlayer)
        {
            return utils::async(mainThreadQueue, closedSound, std::move(soundPlayer));
        }, std::move(soundPlayer));
    }).Detach();
    g_game->sig_onExit.Connect([&closeSoundWaitable]()
    {
        closeSoundWaitable.SetPriority(utils::MessagePriority::Immediately);
        utils::RecursiveYielder yielder(nextFrameQueue, frameThread, *s_clock);
        auto asyncResult = yield_for_async(yielder, std::move(closeSoundWaitable));
        if (asyncResult.isOk())
        {
            yield_for_async(yielder, asyncResult.unwrap()).ignoreResult();
        }
    }).Detach();
    ctx->sig_onKeyDown.Connect([](int key)
    {
        VKEY vkey = GetVKeyMapping(key);
        if (vkey == VKEY::_COUNT)
        {
            return;
        }
        isKeyPressed[vkey] = true;
        cameraFront = cameraDirect - cameraPos;
    }).Detach();
    ctx->sig_onKeyUp.Connect([](int key)
    {
        VKEY vkey = GetVKeyMapping(key);
        if (vkey != VKEY::_COUNT)
        {
            isKeyPressed[vkey] = false;
        }
        if (vkey == VKEY::LIGHTING)
        {
            bool isLightOn = lightColor == glm::vec3(1.0f);
            lightColor = isLightOn ? glm::vec3(0.0f) : glm::vec3(1.0f);
        }
    }).Detach();
    ctx->sig_onSuspend.ConnectAsync(&mainThreadQueue, []() { isSignalSuspend = true; }).Detach();
    ctx->sig_onResume.ConnectAsync(&mainThreadQueue, []() { isSignalSuspend = false; }).Detach();

    // Initialize global strings
    LoadStringW(GetModuleHandle(NULL), IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(GetModuleHandle(NULL), IDC_HELLOTRIANGLE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(GetModuleHandle(NULL));

    // Perform application initialization:
    if (!InitInstance(GetModuleHandle(NULL), argc))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_HELLOTRIANGLE));

    MSG msg = {};

    // Main message loop:
    utils::steady_clock::time_point beginFrameTimePoint = utils::steady_clock::now();
    utils::steady_clock::time_point lastFrameTimePoint = utils::steady_clock::now();
    utils::nanosecs actualElapsed;
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
            actualElapsed = utils::steady_clock::now() - beginFrameTimePoint;
            beginFrameTimePoint = utils::steady_clock::now();
            float delta = std::chrono::duration_cast<utils::duration<float>>(actualElapsed).count();
            if (!isSignalSuspend)
            {
                frameThread.Submit(delta);
                g_game->Tick(delta);
                frameThread.Wait();
            }
            actualElapsed = utils::steady_clock::now() - beginFrameTimePoint;
            auto remaining = heart->cast_to_duration() - actualElapsed;
            lastFrameTimePoint += utils::IsInBounds(remaining.count(), utils::duration<double>::zero().count(), std::numeric_limits<double>::infinity()) ? actualElapsed + std::chrono::duration_cast<utils::nanosecs>(remaining) : actualElapsed;
            std::unique_lock lk(mutex);
            cv.wait_until(lk, lastFrameTimePoint, []() {return m_isExiting; });
            mainThreadQueue.dispatch();
        }
    }

    g_game.reset();
    utils::Log::d("Main Thread", "Exiting Game");
    utils::Log::Wait();

    return (int) msg.wParam;
}

std::string GetInput()
{
    std::string input;
    std::getline(std::cin, input);
    return input;
}

DeclareScopedEnumWithOperatorDefined(Command, DUMMY_NAMESPACE, uint8_t, Help, ChangeFolder, SuspendSound, ResumeSound, ChangeClockRatio);
using CommandMapType = std::unordered_map<std::string, Command>;
CommandMapType s_commandMap;
std::vector<std::string> s_commandStringList;

void InitCommands()
{
    s_commandMap.emplace("help", Command::Help);
    s_commandMap.emplace("change folder:", Command::ChangeFolder);
    s_commandMap.emplace("suspend sound", Command::SuspendSound);
    s_commandMap.emplace("resume sound", Command::ResumeSound);
    s_commandMap.emplace("change clock ratio:", Command::ChangeClockRatio);
    std::for_each(s_commandMap.begin(), s_commandMap.end(), [](CommandMapType::const_reference kv) { s_commandStringList.push_back(kv.first); });
}

void ProcessInput(const std::string& input)
{
    InputParser inputParser({ input });
    InputOptions inputOptions(s_commandStringList);
    if (InputParser::position_t foundPos = inputParser.HaveInputOptions(inputOptions))
    {
        Command command = s_commandMap[foundPos.inputOptions.foundToken.value()];
        switch (command)
        {
        case Command::Help:
        {
            INFO_LOG("HELP", "commands: {}", inputOptions.to_string());
            break;
        }
        case Command::ChangeFolder:
        {
            g_game->LoadPlaylist(inputParser.ExtractValue(foundPos)).ignoreResult();
            break;
        }
        case Command::SuspendSound:
        {
            g_game->SuspendSound();
            break;
        }
        case Command::ResumeSound:
        {
            g_game->ResumeSound();
            break;
        }
        case Command::ChangeClockRatio:
        {
            const float ratioExtracted = atof(inputParser.ExtractValue(foundPos).c_str());
            if (ratioExtracted != 0)
            {
                m_clockRatio = ratioExtracted;
            }
            break;
        }
        default: CRASH_PLAIN_MSG("This should have never happened! Maybe you forgot handle new added command: {}?", command);
        }
    }
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
    key_mapping.emplace(VKEY::SHIFT, VK_SHIFT);
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
    return vkeyIt != key_mapping.end() ? vkeyIt->first : VKEY::_COUNT;
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
    if (!m_isInitDone)
    {
        return;
    }
    m_ctxWidth = width;
    m_ctxHeight = height;
    glViewport(0, 0, width, height);
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

double Calc_pi(double n, double start, double skip)
{
    double result = 0.0;
    for (double i = start; i < n; i+= skip)
    {
        double sign = std::pow(-1, i);
        double term = 1.0 / (i * 2 + 1);
        result += sign * term;
    }
    return result * 4;
}

utils::Connection s_exitConnection;

void Calc_pi_MT(double n, std::function<void(const double&)> callback)
{
    using MessageHandle = utils::async_waitable<double>;
    unsigned int CONCURRENCY = std::thread::hardware_concurrency();
    std::shared_ptr<std::vector<MessageHandle>> results = std::make_shared<std::vector<MessageHandle>>();
    for (unsigned int i = 0; i < CONCURRENCY; i++)
    {
        results->push_back(utils::async(*calcThread, &Calc_pi, n, i, CONCURRENCY));
    }
    s_exitConnection = g_game->sig_onExit.Connect([results]()
    {
        for (MessageHandle& result : *results)
        {
            auto cancelResult = result.Cancel();
            if (cancelResult != utils::MessageHandleStatus::SUCCESS)
            {
                ERROR_LOG("Cancel Result", "cancel failed: {}", cancelResult);
                continue;
            }
            INFO_LOG("Cancel", "Cancel Successfully");
        }
    });

    using RecursionWrapperType = utils::RecursionWrapper<double, std::shared_ptr<std::vector<MessageHandle>>, std::function<void(const double&)>>;
    using SharedRecursionWrapperType = std::shared_ptr<RecursionWrapperType>;

    auto finalCalculation = [](SharedRecursionWrapperType i_sharedWrapper, double previousResult, std::shared_ptr<std::vector<MessageHandle>> results, std::function<void(const double&)> callback)
    {
        double result = previousResult;
        for (auto futureIt = results->cbegin(); futureIt != results->cend();)
        {
            const MessageHandle& futureResult = *futureIt;
            if (!futureResult.HasFinished())
            {
                utils::async(*calcThread, &RecursionWrapperType::operator(), i_sharedWrapper.get(), i_sharedWrapper, result, results, callback);
                return;
            }
            Result<double, utils::MessageHandleStatus> messageHandleResult = futureResult.GetResult();
            if (messageHandleResult.isOk())
            {
                result += messageHandleResult.unwrap();
            }
            else
            {
                ERROR_LOG("Debug", "error: {}", messageHandleResult.unwrapErr());
            }
            futureIt = results->erase(futureIt);
        }
        callback(result);
    };
    utils::async(*calcThread, finalCalculation, RecursionWrapperType::MakeShared(finalCalculation), 0, results, callback);
}

void Move(VKEY move, float deltaTime)
{
    float cameraSpeed = 0;
    cameraSpeed = k_speed * deltaTime;
    switch (move)
    {
    case VKEY::UP:
        if (isKeyPressed[VKEY::SHIFT] && !(isKeyPressed[VKEY::LEFT] || isKeyPressed[VKEY::RIGHT]))
        {
            cameraSpeed = 2.5f * k_speed * deltaTime;
        }
        cameraPos += cameraSpeed * cameraFront;
        cameraDirect = cameraPos + cameraFront;
    break;
    case VKEY::LEFT:
        cameraPos -= Right * cameraSpeed;
        cameraDirect = cameraPos + cameraFront;
    break;
    case VKEY::DOWN:
        cameraPos -= cameraSpeed * cameraFront;
        cameraDirect = cameraPos + cameraFront;
    break;
    case VKEY::RIGHT:
        cameraPos += Right * cameraSpeed;
        cameraDirect = cameraPos + cameraFront;
    break;
    }
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
    POINT cursorPosition(rect.left + (m_ctxWidth / 2), rect.top + (m_ctxHeight / 2));
    SetCursorPos(cursorPosition.x, cursorPosition.y);
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
        POINT pCursor;
        if (GetCursorPos(&pCursor))
        {
            ScreenToClient(hWnd, &pCursor);
        }
        virtualCursorPoint->Reset(pCursor);
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

void MouseCallback(float xpos, float ypos)
{
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
        utils::async(nextFrameQueue, &createGLContext);

        GetClientRect(hWnd, &rect);
            
        utils::async(nextFrameQueue, &initializeShaderProgram, (GLsizei)rect.right, (GLsizei)rect.bottom);
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
            isSignalSuspend = false;
            utils::async(nextFrameQueue, &ExitGame, hWnd);
        }
        break;
        case IDM_EXPORT:
        {
            sig_onExport.Emit();
        }
        break;
        case IDM_PAUSE:
        {
            m_isPause = !m_isPause;
            m_isPause ? applicationCtx->SuspendAsync() : applicationCtx->ResumeAsync();
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_MOUSEMOVE:
        if (!m_isPause)
        {
            POINT pCursor = virtualCursorPoint->Get(hWnd);
            utils::async(nextFrameQueue, &MouseCallback, (float) pCursor.x, (float) pCursor.y);
        }
    break;
    case WM_MOUSEWHEEL:
        zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        utils::async(nextFrameQueue, &ScrollCallback, hWnd, zDelta);
    break;
    case WM_KEYDOWN:
        applicationCtx->InvokeKeyDown((int)wParam);
    break;
    case WM_KEYUP:
        applicationCtx->InvokeKeyUp((int)wParam);
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
        if (wParam == SIZE_MINIMIZED)
        {
            if (!isSignalSuspend)
            {
                applicationCtx->SuspendAsync();
            }
        }
        else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
        {
            GetClientRect(hWnd, &rect);
            utils::async(nextFrameQueue, &resize, (GLsizei)rect.right, (GLsizei)rect.bottom);
        }
    }
    break;
    case WM_ACTIVATEAPP:
    {
        if (wParam)
        {
            if (isSignalSuspend && !m_isPause)
            {
                applicationCtx->ResumeAsync();
            }
            isKeyPressed[VKEY::ALT] = false;
            LockCursor(true, hWnd);
            for (VKEY iterKey = VKEY::_FIRST; iterKey != VKEY::_COUNT; ++iterKey)
            {
                isKeyPressed[iterKey] = false;
            }
        }
        else
        {
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
        isSignalSuspend = false;
        utils::async(nextFrameQueue, &ExitGame, hWnd);
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
    float fps = (float)heart->beats;
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
    g_game->RequestExit();
    if (s_clock->IsUpdating())
    {
        utils::async(nextFrameQueue, ExitGame, hWnd);
        return;
    }
    m_isExiting = true;
    cv.notify_one();
    ASSERT(!s_clock->IsUpdating());
    if (!s_inputResult.HasFinished())
    {
        s_inputThread.terminate();
    }
    thisFrameQueue.cancel();
    s_clock.reset();
    applicationCtx->soundManager.Shutdown();
    applicationCtx->soundManager.SetThreadId(utils::details::threading::thread_id_t::k_invalid);
    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    CleanResource();

    if (ghRC)
    {
        wglDeleteContext(ghRC);
    }
    if (ghDC)
    {
        ReleaseDC(hWnd, ghDC);
    }
    utils::async(mainThreadQueue, &DestroyWindow, hWnd);
}

GLvoid drawScene()
{
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
    float modelAspect = 0.4f;
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

void FramePrologue(float deltaTime)
{
    float gameplayDeltaTime = m_clockRatio * deltaTime;
    if (s_clock)
    {
        utils::async(thisFrameQueue, &utils::SystemClock::Update, s_clock.get(), gameplayDeltaTime);
    }
    m_totalTicks += gameplayDeltaTime;
}

void FrameEpilogue()
{
    if (m_isInitDone)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawScene();
    }
}

void MovementUpdate(float delta)
{
    for (VKEY vkey : s_movementKeys)
    {
        if (!isKeyPressed[vkey])
        {
            continue;
        }
        Move(vkey, delta);
    }
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
    //unsigned char* data = stbi_load("assets/OK!.png", &width, &height, &nrChannels, 0);
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
    m_isInitDone = true;
}