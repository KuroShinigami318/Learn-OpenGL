#include "Sample.h"

extern GLvoid drawScene(DX::StepTimer const& timer, const GLsizei& ctxWidth, const GLsizei& ctxHeight);

Sample::Sample() : m_frame(0)
{
	renderThread = std::make_shared<utils::WorkerThread<void()>>(false, "Render Thread", utils::MODE::UPDATE_CALLBACK);
	ResetCallbackRenderThread();
	SetFixedFPS(144);
}

Sample::~Sample()
{
	renderThread.reset();
}

DX::StepTimer Sample::GetTimer()
{
	return m_timer;
}

void Sample::Tick()
{
	m_timer.Tick([&]()
	{
		Update(m_timer);
		drawScene(m_timer, m_ctxWidth, m_ctxHeight);
	});
	m_frame++;
}

void Sample::OnResize(const GLsizei& ctxWidth, const GLsizei& ctxHeight)
{
	m_ctxWidth = ctxWidth;
	m_ctxHeight = ctxHeight;
}

void Sample::Update(DX::StepTimer const&)
{
	if (m_timer.GetTotalSeconds() - m_preUpdateTime > 1)
	{
		utils::Log::i("Sample::Update", FORMAT("Get FPS: {}", m_timer.GetFramesPerSecond()));
		m_preUpdateTime = m_timer.GetTotalSeconds();
		m_lastFrame = m_frame;
	}
}

void Sample::OnSuspending()
{
	utils::Log::d("Sample::OnSuspending", "Suspended");
	renderThread->Pause(true);
	/*renderThread->Suspend();*/
}

void Sample::OnResuming()
{
	utils::Log::d("Sample::OnResuming", "Resumed");
	renderThread->Pause(false);
	/*renderThread->Resume();*/
}

utils::WorkerThread<void()>* Sample::GetThread()
{
	return renderThread.get();
}

void Sample::ResetCallbackRenderThread()
{
	utils::MessageHandle result = renderThread->PushCallback(&Sample::Tick, this);
	if (result.IsError())
	{
		utils::Log::e("Sample::ResetCallbackRenderThread", FORMAT("Error in pushing callback to render thread with error code: {}", magic_enum::enum_name(result.GetError())));
	}
}

bool Sample::IsAny(int value, std::vector<int> list)
{
	return utils::Contains(value, list);
}

void Sample::SetFixedFPS(short i_fps)
{
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedTicks(m_timer.TicksPerSecond / i_fps);
}