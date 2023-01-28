#include "Sample.h"
#include "common/Log.h"
#include "common/WorkerThread.h"
#include <glad/glad.h>
#include <format>
#include <memory>

extern GLvoid drawScene(DX::StepTimer const& timer);

Sample::Sample() : m_frame(0)
{
	renderThread = std::make_shared<utils::WorkerThread<void()>>(false);
	renderThread->CreateWorkerThread([]() {});
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
		drawScene(m_timer);
	});
	m_frame++;
}

void Sample::Update(DX::StepTimer const&)
{
	utils::Log::i("Sample::Update", std::format("Get FPS: {}", m_timer.GetFramesPerSecond()).c_str());
}

void Sample::OnSuspending()
{
	utils::Log::d("Sample::OnSuspending", "Suspended");
	//renderThread->Pause(true);
	renderThread->Suspend();
}

void Sample::OnResuming()
{
	utils::Log::d("Sample::OnResuming", "Resumed");
	//renderThread->Pause(false);
	renderThread->Resume();
}

utils::WorkerThread<void()>* Sample::GetThread()
{
	return renderThread.get();
}

void Sample::ResetCallbackRenderThread()
{
	utils::WorkerThreadERR result = renderThread->PushCallback<void()>(std::bind(&Sample::Tick, this));
	if (result != utils::WorkerThreadERR::SUCCESSS)
	{
		utils::Log::e("Sample::ResetCallbackRenderThread", std::format("Error in pushing callback to render thread with error code [{}]", utils::WorkerThreadERRCode[(int)result]).c_str());
	}
}