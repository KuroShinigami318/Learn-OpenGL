// HelloTriangle.cpp : Defines the entry point for the application.
//
#include "framework.h"
#include "HelloTriangle.h"
#include "Sample.h"
#include "common/StepTimer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
//#include <glad/glad_wgl.h>
#include "common/Log.h"
#include "common/WorkerThread.h"
#include "lib/bel_ImgLoader/include/stb_image.h"
//#include "lib/bel_lodepng/include/lodepng.h"

#define MAX_LOADSTRING 100

enum class VKEY
{
    _BEGIN,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    ALT,
    SHIFT,
    _END
};

VKEY& operator ++ (VKEY& e)
{
    if (e == VKEY::_END) {
        throw std::out_of_range("Out of range VKEY");
    }
    e = VKEY(static_cast<std::underlying_type<VKEY>::type>(e) + 1);
    return e;
}

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HDC   ghDC;
HGLRC ghRC;
GLsizei m_ctxWidth, m_ctxHeight;
std::shared_ptr<Sample*> g_sample;
std::mutex m_mutex;
std::condition_variable cv;
std::unique_ptr<utils::WorkerThread<void()>> registerNotify;
std::unique_ptr<utils::WorkerThread<void()>> processLifeCycle;
utils::WorkerThread<double()> calcThread(false, "Calc Thread Pool", utils::MODE::MESSAGE_QUEUE_MT, 50, true, 4);
HANDLE g_plmSuspendComplete = nullptr;
HANDLE g_plmSignalResume = nullptr;
PAPPSTATE_REGISTRATION hPLM = {};
bool m_isExiting, m_isInitDone, m_isPause, isSignalSuspend, m_isWaiting;
bool firstMouse = true;
std::unordered_map<VKEY, bool> isKeyPressed;

#define BLACK_INDEX     0 
#define RED_INDEX       13 
#define GREEN_INDEX     14 
#define BLUE_INDEX      16

/* OpenGL globals, defines, and prototypes */
const int divide = 2;
const float divided = 1.0f / (float) divide;
constexpr int max_scale = 200;
constexpr int initial_up = max_scale / divide;
int up = initial_up;
bool isDown = false;
unsigned int shaderProgram;
unsigned int VBO, VAO, EBO, internalFormat;
unsigned int texture;
unsigned int viewLoc, transformColorLoc, modelLoc;
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraDirect = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 Right;
glm::vec3 WorldUp = cameraUp;
GLfloat latitude, longitude, latinc, longinc, fov, lastX, lastY;
float k_speed = 1.0f;
float yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
GLdouble radius;

#define GLOBE    1 
#define CYLINDER 2 
#define CONE     3

const char* vertexShaderSource = "#version 460 core\n"
"layout (location = 0) in vec3 aPos;\n"
//"layout (location = 1) in vec2 aTexCoord;\n"
"uniform mat4 transform;\n"
"uniform mat4 u_transform_color;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out mat4 transform_color;\n"
//"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
"   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
"   transform_color = u_transform_color;\n"
//"   TexCoord = aTexCoord;\n"
"}\0";

const char* fragmentShaderSource = "#version 460 core\n"
"out vec4 FragColor;\n"
"in mat4 transform_color;\n"
//"in vec2 TexCoord;\n"
//"uniform sampler2D ourTexture;"
"void main()\n"
"{\n"
//"   FragColor = texture(ourTexture, TexCoord);\n"
"   FragColor = transform_color * vec4(1.0f);\n"
"}\n\0";

// Forward declarations of functions included in this code module:
void                Wait();
void                StopWaiting();
void                CleanResource();
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
int                 loadOpenGLFunctions();
void                ExitGame(HWND hWnd);
GLvoid drawScene(DX::StepTimer const& timer);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    g_sample = std::make_shared<Sample*>(new Sample());
    registerNotify = std::make_unique<utils::WorkerThread<void()>>(false, "State Notification Thread", utils::MODE::MESSAGE_QUEUE);
    processLifeCycle = std::make_unique<utils::WorkerThread<void()>>(true, "Life Cycle Update Thread");
    processLifeCycle->CreateWorkerThread([&]() {
        while (!m_isExiting && m_isInitDone)
        {
            if (isSignalSuspend)
            {
                (*g_sample)->OnSuspending();
                // Complete deferral
                SetEvent(g_plmSuspendComplete);
                (void)WaitForSingleObject(g_plmSignalResume, INFINITE);
                {
                    std::lock_guard<std::mutex> lk_guard(m_mutex);
                    isSignalSuspend = false;
                }
                if (!m_isPause)
                {
                    (*g_sample)->OnResuming();
                    for (VKEY iterKey = VKEY::_BEGIN; iterKey != VKEY::_END; ++iterKey)
                    {
                        isKeyPressed[iterKey] = false;
                    }
                }
            }

            // update thread loop            
            if (!m_isPause && (*g_sample)->GetThread()->GetCurrentMode() == utils::MODE::UPDATE_CALLBACK)
            {
                if (m_isExiting)
                {
                    utils::Log::e("UpdateThreadLoop", FORMAT("m_isExiting={}, should not be occured here!!!", m_isExiting));
                    break;
                }
                (*g_sample)->GetThread()->Pause(false);
                (*g_sample)->GetThread()->RunOneTime(false);
            }
            Sleep(1);
        }
    });
    g_plmSuspendComplete = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    g_plmSignalResume = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (!g_plmSuspendComplete || !g_plmSignalResume)
        return 1;

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
        }
    }

    utils::Log::d("Main Thread", "Exiting Game");
    processLifeCycle.reset();
    utils::Log::d("Main Thread", "Exited processLifeCycle thread");
    registerNotify.reset();
    utils::Log::d("Main Thread", "Exited notify thread");
    g_sample.reset();
    utils::Log::d("Main Thread", "Exited Render thread");

    CloseHandle(g_plmSuspendComplete);
    CloseHandle(g_plmSignalResume);


    return (int) msg.wParam;
}

void CleanResource()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
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

GLvoid initializeShaderProgram(GLsizei ctxWidth, GLsizei ctxHeight)
{
    m_ctxWidth = ctxWidth;
    m_ctxHeight = ctxHeight;
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
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
    float vertices[] = {
        // postions            // texCoords
         0.5f,  0.5f,  0.5f,    //3.0f, 3.0f,                                         // top right outer
         0.5f, -0.5f,  0.5f,    //3.0f, 0.0f,                                         // bottom right outer
        -0.5f, -0.5f,  0.5f,    //0.0f, 0.0f,                                         // bottom left outer
        -0.5f,  0.5f,  0.5f,    //0.0f, 3.0f,                                         // top left outer
         0.5f,  0.5f, -0.5f,    //3.0f, 3.0f,                                         // top right inner
         0.5f, -0.5f, -0.5f,    //3.0f, 0.0f,                                         // bottom right inner
        -0.5f, -0.5f, -0.5f,    //0.0f, 0.0f,                                         // bottom left inner
        -0.5f,  0.5f, -0.5f,    //0.0f, 3.0f,                                         // top left inner
    };

    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 2,    // first triangle outer
        2, 3, 0,    // second triangle outer
        4, 5, 6,    // first triangle inner
        6, 7, 4,    // second triangle inner
        0, 3, 4,    // first triangle top
        4, 7, 3,    // second triangle top
        1, 2, 5,    // first triangle bottom
        5, 6, 2,    // second triangle bottom
        2, 3, 6,    // first triangle left
        6, 7, 3,    // second triangle left
        0, 1, 4,    // first triangle right
        4, 5, 1,    // second triangle right
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    //glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(0);
    //glEnableVertexAttribArray(1);

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
    glm::mat4 projection = glm::perspective(glm::radians(fov), (float)ctxWidth / (float)ctxHeight, 0.1f, 100.0f);
    glUseProgram(shaderProgram);
    unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    viewLoc = glGetUniformLocation(shaderProgram, "view");
    modelLoc = glGetUniformLocation(shaderProgram, "model");
    transformColorLoc = glGetUniformLocation(shaderProgram, "u_transform_color");
    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    //glBindVertexArray(0);


    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    StopWaiting();
}

GLvoid resize(GLsizei width, GLsizei height)
{
    m_ctxWidth = width;
    m_ctxHeight = height;
    glViewport(0, 0, width, height);
    (*g_sample)->GetThread()->Pause(true);
    (*g_sample)->GetThread()->ChangeMode(utils::MODE::UPDATE_CALLBACK);
    (*g_sample)->ResetCallbackRenderThread(width, height);
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

    StopWaiting();
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
    for (unsigned int i = 0; i < CONCURRENCY; i++)
    {
        calcThread.PushCallback(&Calc_pi, n, i, CONCURRENCY);
    }
    calcThread.Dispatch();
    int i = 0;
    while (!calcThread.HasAllMTProcessDone())
    {
        if (!calcThread.IsEmptyResult())
        {
            result += calcThread.PopResult();
            i++;
        }
        if (i == CONCURRENCY) break;
    }
    return result;
}

double Move(VKEY move)
{
    return [&](VKEY move) -> double {
        float deltaTime = 0;
        float cameraSpeed = 0;
        float timeSleep = 0;
        auto currentTP = std::chrono::system_clock::now();
        auto preUpdateTP = currentTP;
        std::chrono::duration<double> elapsed_seconds = currentTP - preUpdateTP;
        while (!m_isExiting && isKeyPressed[move])
        {
            deltaTime = (*g_sample)->GetTimer().GetElapsedSeconds();
            currentTP = std::chrono::system_clock::now();
            elapsed_seconds = currentTP - preUpdateTP;
            if (elapsed_seconds.count() < deltaTime)
            {
                continue;
            }
            preUpdateTP = currentTP;
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
    }(move);
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
    if (xpos ==0 || ypos == 0 || xpos == m_ctxWidth || ypos == m_ctxHeight)
    {
        SetMiddleCursorPos(hWnd);
        firstMouse = true;
        return;
    }

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

void ScrollCallback(HWND hWnd, double xoffset, double yoffset)
{
    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
    DEBUG_LOG("fov: {}, yoffset: {}", fov, yoffset);
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
    RECT rect;
    switch (message)
    {
    case WM_CREATE:
    {
        ghDC = GetDC(hWnd);
        (*g_sample)->GetThread()->PushCallback(&createGLContext);
        (*g_sample)->GetThread()->RunOneTime(true);
        Wait();

        GetClientRect(hWnd, &rect);
            
        (*g_sample)->GetThread()->PushCallback(&initializeShaderProgram,(GLsizei) rect.right,(GLsizei) rect.bottom);
        (*g_sample)->GetThread()->RunOneTime(true);
        Wait();

        m_isInitDone = true;
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
        if (MessageBoxA(0, "Do you want to exit game?", "Warning", MB_YESNO) == IDYES)
        {
            ExitGame(hWnd);
        }
        break;
        case IDM_EXPORT:
        {
            if (!m_isWaiting)
            {
                m_isWaiting = true;
                utils::WorkerThread t;
                t.CreateWorkerThread([&]() {
                    utils::Log::d("Debug", "Meter Performance");
                    double result = Calc_pi_MT(1E9);
                    DEBUG_LOG("test: {}", result);
                    MessageBoxA(0, FORMAT("Pi: {}", result), "Result", MB_SERVICE_NOTIFICATION);
                    m_isWaiting = false;
                    });
                t.Detach();
            }
        }
        break;
        case IDM_PAUSE:
        {
            /*m_isPause = !(*g_sample)->GetThread()->IsPaused();
            (*g_sample)->GetThread()->Pause(m_isPause);*/
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_MOUSEMOVE:
        POINTS posCursor = MAKEPOINTS(lParam);
        MouseCallback(hWnd, posCursor.x, posCursor.y);
    break;
    case WM_MOUSEWHEEL:
        POINTS posWheel = MAKEPOINTS(lParam);
        ScrollCallback(hWnd, posWheel.x, posWheel.y);
    break;
    case WM_KEYDOWN:
        if ((*g_sample)->IsAny(wParam, { 0x57, VK_UP }))
        {
            if (!isKeyPressed[VKEY::UP])
            {
                cameraFront = cameraDirect - cameraPos;
                calcThread.PushCallback(Move, VKEY::UP);
                isKeyPressed[VKEY::UP] = true;
                calcThread.Dispatch();
            }
        }
        if ((*g_sample)->IsAny(wParam, { 0x41, VK_LEFT }))
        {
            if (!isKeyPressed[VKEY::LEFT])
            {
                cameraFront = cameraDirect - cameraPos;
                calcThread.PushCallback(Move, VKEY::LEFT);
                isKeyPressed[VKEY::LEFT] = true;
                calcThread.Dispatch();
            }
        }
        if ((*g_sample)->IsAny(wParam, { 0x53, VK_DOWN }))
        {
            if (!isKeyPressed[VKEY::DOWN])
            {
                cameraFront = cameraDirect - cameraPos;
                calcThread.PushCallback(Move, VKEY::DOWN);
                isKeyPressed[VKEY::DOWN] = true;
                calcThread.Dispatch();
            }
        }
        if ((*g_sample)->IsAny(wParam, { 0x44, VK_RIGHT }))
        {
            if (!isKeyPressed[VKEY::RIGHT])
            {
                cameraFront = cameraDirect - cameraPos;
                calcThread.PushCallback(Move, VKEY::RIGHT);
                isKeyPressed[VKEY::RIGHT] = true;
                calcThread.Dispatch();
            }
        }
        if (wParam == VK_SHIFT && !isKeyPressed[VKEY::RIGHT])
        {

            isKeyPressed[VKEY::SHIFT] = true;
        }
    break;
    case WM_KEYUP:
        if ((*g_sample)->IsAny(wParam, { 0x57, VK_UP }))
        {
            isKeyPressed[VKEY::UP] = false;
            calcThread.CleanResults();
        }
        if ((*g_sample)->IsAny(wParam, { 0x41, VK_LEFT }))
        {
            isKeyPressed[VKEY::LEFT] = false;
            calcThread.CleanResults();
        }
        if ((*g_sample)->IsAny(wParam, { 0x53, VK_DOWN }))
        {
            isKeyPressed[VKEY::DOWN] = false;
            calcThread.CleanResults();
        }
        if ((*g_sample)->IsAny(wParam, { 0x44, VK_RIGHT }))
        {
            isKeyPressed[VKEY::RIGHT] = false;
            calcThread.CleanResults();
        }
        if (wParam == VK_SHIFT)
        {
            isKeyPressed[VKEY::SHIFT] = false;
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
            (*g_sample)->GetThread()->ChangeMode(utils::MODE::MESSAGE_QUEUE, 1);
            (*g_sample)->GetThread()->PushCallback(&resize,(GLsizei) rect.right, (GLsizei) rect.bottom);
            (*g_sample)->GetThread()->Dispatch();
        }
    }
    break;
    case WM_ACTIVATEAPP:
    {
        if (wParam)
        {
            LockCursor(true, hWnd);
        }
        else if (!isKeyPressed[VKEY::ALT])
        {
            LockCursor(false, hWnd);
        }
        registerNotify->PushCallback([&](WPARAM wParam) {
            if (!m_isExiting)
            {
                if (wParam == TRUE)
                {
                    SetEvent(g_plmSignalResume);
                }
                else
                {
                    ResetEvent(g_plmSuspendComplete);
                    ResetEvent(g_plmSignalResume);
                    {
                        std::lock_guard<std::mutex> lk_guard(m_mutex);
                        isSignalSuspend = true;
                    }

                    // To defer suspend, you must wait to exit this callback
                    (void)WaitForSingleObject(g_plmSuspendComplete, INFINITE);
                }
            }
        }, wParam);
        registerNotify->Dispatch();
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

void Wait()
{
    std::unique_lock<std::mutex> lk_guard(m_mutex);
    m_isWaiting = true;
    cv.wait(lk_guard, [&] {return !m_isWaiting; });
}

void StopWaiting()
{
    {
        std::lock_guard<std::mutex> lk_guard(m_mutex);
        m_isWaiting = false;
    }
    cv.notify_one();
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void ExitGame(HWND hWnd)
{
    m_isExiting = true;
    (*g_sample)->GetThread()->ChangeMode(utils::MODE::MESSAGE_QUEUE);
    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    (*g_sample)->GetThread()->PushCallback(&CleanResource);

    if (ghRC)
    {
        (*g_sample)->GetThread()->PushCallback(&wglDeleteContext, (HGLRC) ghRC);
    }
    if (ghDC)
    {
        (*g_sample)->GetThread()->PushCallback(&ReleaseDC, (HWND) hWnd, (HDC) ghDC);
    }
    (*g_sample)->GetThread()->Dispatch();
    DestroyWindow(hWnd);
}

GLvoid drawScene(DX::StepTimer const& timer, GLsizei const& ctxWidth, GLsizei const& ctxHeight)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind Texture
    //glBindTexture(GL_TEXTURE_2D, texture);

    glUseProgram(shaderProgram);

    // camera/view transformation
    glm::mat4 view = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
    if (cameraDirect == glm::vec3(0.0f, 0.0f, 0.0f))
    {
        float radius = 5.0f;
        float camX = static_cast<float>(sin(timer.GetTotalSeconds()) * radius);
        float camZ = static_cast<float>(cos(timer.GetTotalSeconds()) * radius);
        cameraPos = glm::vec3(camX, 0.0f, camZ);
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    view = glm::lookAt(cameraPos, cameraDirect, cameraUp);

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // create transformations
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 transformColor = glm::mat4(1.0f);
    // make sure to initialize matrix to identity matrix first
    /*(up < max_scale && !isDown) ? ++up : isDown = true;
    (up > (initial_up) && isDown) ? --up : isDown = false;
    float scale = (up) * ((float) 1 / max_scale);*/
    float scale = ((sin(timer.GetTotalSeconds()) + 1) * 0.5f * divided) + (1 - (double)divided);
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
    //transform = glm::rotate(transform, (float)timer.GetTotalSeconds(), glm::vec3(0.0, 1.0, 0.0));
    transformColor = glm::scale(transformColor, glm::vec3(scale, scale, scale));
    // draw our first triangle
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(transformColorLoc, 1, GL_FALSE, glm::value_ptr(transformColor));
    //glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
    // calculate the model matrix for each object and pass it to shader before drawing
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    //glBindVertexArray(0); // no need to unbind it every time

    //front mini model
    float modelAspect = 0.4;
    model = glm::translate(model, glm::vec3(2.0f, (modelAspect - 1.0f) / 2, 0.0f));
    model = glm::scale(model, glm::vec3(modelAspect, modelAspect, modelAspect));
    transformColor = glm::mat4(1.0f);
    transformColor = glm::scale(transformColor, glm::vec3(0.39f, 0.0f, 0.39f));

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(transformColorLoc, 1, GL_FALSE, glm::value_ptr(transformColor));
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    SwapBuffers(ghDC);
}