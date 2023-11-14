#include "common-utils.h"
#include "Game.h"
#include "ApplicationContext.h"

Game::Game(IApplicationContext& i_ctx) : m_frame(0), m_preUpdateTime(0), renderThread(false, this, "Render Thread", std::make_unique<utils::HeartBeats<utils::BPS>>(300, utils::BPS())), m_lastFrame(0), sig_onRenderTheadFirstUpdate(renderThread->sig_onFirstUpdate)
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
	renderThread->StopAsync();
}

uint32_t Game::GetFramesPerSecond() const
{
	return renderThread->GetFramesPerSecond();
}

double Game::GetElapsedSeconds() const
{
	return renderThread->GetElapsedSeconds();
}

void Game::Tick()
{
	utils::Access<SignalKey>(sig_onTick).Emit(renderThread->GetElapsedSeconds());
}

void Game::Update(float)
{
	float elapsed = renderThread->GetTotalSeconds() - m_preUpdateTime;
	if (elapsed > 1)
	{
		utils::Log::i("Game::Update", utils::Format("Get FPS: {}", renderThread->GetFramesPerSecond()));
		utils::Access<SignalKey>(sig_resetTimer).Emit(renderThread->GetTotalSeconds());
		m_lastFrame = m_frame;
	}
	else if (elapsed < 0)
	{
		utils::Log::e("Game::Update", utils::Format("Reset time due to undefined behavior: {}", elapsed));
		utils::Access<SignalKey>(sig_resetTimer).Emit(renderThread->GetTotalSeconds());
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

const utils::WorkerThread<void()>* Game::GetThread() const
{
	return renderThread.get();
}

void Game::PauseRenderThread(bool i_pause)
{
	renderThread->Pause(i_pause);
}

void Game::ChangeModeRenderThread(utils::MODE i_mode, size_t i_maxQueue)
{
	if (!utils::Contains(i_mode, { utils::MODE::MESSAGE_QUEUE, utils::MODE::UPDATE_CALLBACK }))
	{
		return;
	}
	renderThread->ChangeMode(i_mode, i_maxQueue);
}

void Game::CleanQueueInRenderThread()
{
	renderThread->Clear();
}

void Game::SyncRenderThread()
{
	renderThread->Dispatch();
	renderThread->Wait();
}

utils::MessageHandle<void> Game::PushMessage(MessageType i_message)
{
	return renderThread->PushCallback(i_message);
}

bool Game::IsAny(int value, std::vector<int> list)
{
	return utils::Contains(value, list);
}

void Game::SetFixedFPS(short i_fps)
{
	if (i_fps == 0)
		return;
	renderThread->SetHeart(i_fps);
}