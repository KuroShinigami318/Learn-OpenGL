#include "Sample.h"
#include <format>
#include <cmath>

extern GLvoid drawScene(DX::StepTimer const& timer, const GLsizei& ctxWidth, const GLsizei& ctxHeight);

Sample::Sample() : m_frame(0)
{
	renderThread = std::make_shared<utils::WorkerThread<void()>>(false, "Render Thread");
}

Sample::~Sample()
{
	renderThread.reset();
}

DX::StepTimer Sample::GetTimer()
{
	return m_timer;
}

void Sample::Tick(const GLsizei& ctxWidth, const GLsizei& ctxHeight)
{
	m_timer.Tick([&]()
	{
		Update(m_timer);
		drawScene(m_timer, ctxWidth, ctxHeight);
	});
	m_frame++;
}

void Sample::Update(DX::StepTimer const&)
{
	//utils::Log::i("Sample::Update", FORMAT("Get FPS: {}", m_timer.GetFramesPerSecond()));
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

void Sample::ResetCallbackRenderThread(const GLsizei& ctxWidth, const GLsizei& ctxHeight)
{
	utils::WorkerThreadERR result = renderThread->PushCallback(&Sample::Tick, this, ctxWidth, ctxHeight);
	if (result != utils::WorkerThreadERR::SUCCESSS)
	{
		utils::Log::e("Sample::ResetCallbackRenderThread", FORMAT("Error in pushing callback to render thread with error code [{}]", utils::WorkerThreadERRCode[(int)result]));
	}
}

bool Sample::IsAny(int value, std::vector<int> list)
{
	return utils::Contains(value, list);
}