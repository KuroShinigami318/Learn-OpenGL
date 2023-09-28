#pragma once
#include "common/StepTimer.h"

struct IApplicationContext;

class Game : public utils::WorkerRunnable<void()>
{
private:
	struct SignalKey;

public:
	Game(IApplicationContext& i_ctx);
	virtual ~Game();
	Game(Game&&) = delete;
	Game& operator= (Game&&) = delete;

	Game(Game const&) = delete;
	Game& operator= (Game const&) = delete;

	const DX::StepTimer& GetTimer();

	utils::WorkerThread<void()>* GetThread();

	bool IsAny(int value, std::vector<int> list);

	utils::Signal<void(float), SignalKey> sig_onTick;

private:
	void Tick();

	// Messages
	void OnSuspending();
	void OnResuming();
	void SetFixedFPS(short i_fps);
	void Update(float);
	void OnRun()
	{
		Tick();
	}

	void OnCancel()
	{
		// do cleanup on cancel
	}

	// Rendering loop timer.
	uint64_t                                    m_frame;
	uint64_t                                    m_lastFrame;
	DX::StepTimer                               m_timer;
	float										m_preUpdateTime;
	// Render Thread
	utils::unique_ref<utils::WorkerThread<void()>> renderThread;
	utils::Signal<void(float), SignalKey>		sig_resetTimer;
	std::vector<utils::Connection>				m_connections;
};

