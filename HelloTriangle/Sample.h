#pragma once
#include "framework.h"
#include <windows.h>
#include <memory>
#include <cstdint>
#include "common/StepTimer.h"
#include <glad/glad.h>
#include <vector>

class Sample : public utils::WorkerRunnable<void()>
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
	void OnResize(const GLsizei& ctxWidth, const GLsizei& ctxHeight);

	// Messages
	void OnSuspending();
	void OnResuming();

	utils::WorkerThread<void()>* GetThread();

	void ResetCallbackRenderThread();

	bool IsAny(int value, std::vector<int> list);

	void SetFixedFPS(short i_fps);

private:
	void Update(DX::StepTimer const& timer);

	// Rendering loop timer.
	uint64_t                                    m_frame;
	uint64_t                                    m_lastFrame;
	float										m_preUpdateTime;
	DX::StepTimer                               m_timer;
	GLsizei										m_ctxWidth, m_ctxHeight;
	// Render Thread
	std::shared_ptr<utils::WorkerThread<void()>> renderThread;
};

