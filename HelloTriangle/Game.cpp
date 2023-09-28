#include "framework.h"
#include "Game.h"
#include "ApplicationContext.h"

Game::Game(IApplicationContext& i_ctx) : m_frame(0), m_preUpdateTime(0), renderThread(false, std::shared_ptr<Game>(this, utils::WorkerThread<>::null_deleter()), "Render Thread")
{
	m_connections.push_back(sig_onTick.Connect(&Game::Update, this));
	m_connections.push_back(sig_resetTimer.Connect([this](float i_seconds)
	{
		if (i_seconds > 0)
		{
			m_preUpdateTime = i_seconds; 
		}
		else
		{
			utils::Log::e("Game::sig_resetTimer", utils::Format("Invalid time: {}", i_seconds));
		}
	}));
	m_connections.push_back(utils::Log::sig_errorThrow.Connect([](std::string what)
	{
#if defined(_DEBUG)
		OutputDebugStringA(utils::Format("{}\n", what).c_str());
#endif
		assert(true);
	}));
	m_connections.push_back(i_ctx.sig_onFPSChanged.Connect(&Game::SetFixedFPS, this));
	m_connections.push_back(i_ctx.sig_onSuspend.Connect(&Game::OnSuspending, this));
	m_connections.push_back(i_ctx.sig_onResume.Connect(&Game::OnResuming, this));
}

Game::~Game()
{
	renderThread.reset();
}

const DX::StepTimer& Game::GetTimer()
{
	return m_timer;
}

void Game::Tick()
{
	m_timer.Tick([&]()
	{
		utils::Access<SignalKey>(sig_onTick).Emit(m_timer.GetElapsedSeconds());
	});
}

void Game::Update(float)
{
	float elapsed = m_timer.GetTotalSeconds() - m_preUpdateTime;
	if (elapsed > 1)
	{
		utils::Log::i("Game::Update", utils::Format("Get FPS: {}", m_timer.GetFramesPerSecond()));
		utils::Access<SignalKey>(sig_resetTimer).Emit(m_timer.GetTotalSeconds());
		m_lastFrame = m_frame;
	}
	else if (elapsed < 0)
	{
		utils::Log::e("Game::Update", utils::Format("Reset time due to undefined behavior: {}", elapsed));
		utils::Access<SignalKey>(sig_resetTimer).Emit(m_timer.GetTotalSeconds());
	}
	m_frame++;
}

void Game::OnSuspending()
{
	utils::Log::d("Game::OnSuspending", "Suspended");
	renderThread->Pause(true);
}

void Game::OnResuming()
{
	utils::Log::d("Game::OnResuming", "Resumed");
	renderThread->Pause(false);
}

utils::WorkerThread<void()>* Game::GetThread()
{
	return renderThread.get();
}

bool Game::IsAny(int value, std::vector<int> list)
{
	return utils::Contains(value, list);
}

void Game::SetFixedFPS(short i_fps)
{
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedTicks(m_timer.TicksPerSecond / i_fps);
}