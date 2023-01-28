#pragma once
#include <windows.h>
#include <memory>
#include <cstdint>
#include "common/StepTimer.h"
#include "common/WorkerThread.h"

class Sample
{
public:
	Sample();
	virtual ~Sample();
	Sample(Sample&&) = delete;
	Sample& operator= (Sample&&) = delete;

	Sample(Sample const&) = delete;
	Sample& operator= (Sample const&) = delete;

	DX::StepTimer GetTimer();

	void Tick();

	// Messages
	void OnSuspending();
	void OnResuming();

	utils::WorkerThread<void()>* GetThread();

	void ResetCallbackRenderThread();

private:
	void Update(DX::StepTimer const& timer);

	// Rendering loop timer.
	uint64_t                                    m_frame;
	DX::StepTimer                               m_timer;
	// Render Thread
	std::shared_ptr<utils::WorkerThread<void()>> renderThread;
};

