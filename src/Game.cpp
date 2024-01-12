#include "stdafx.h"
#include "ApplicationContext.h"
#include "Log.h"
#include "Game.h"
#include "StepTimer.h"

Game::Game(IApplicationContext& i_ctx, utils::IMessageSinkBase& nextFrameQueue) : m_frame(0), m_preUpdateTime(0)
	, m_timer(new DX::StepTimer())
{
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
	m_connections.push_back(i_ctx.sig_onSuspend.ConnectAsync(&nextFrameQueue, &Game::OnSuspending, this));
	m_connections.push_back(i_ctx.sig_onResume.ConnectAsync(&nextFrameQueue, &Game::OnResuming, this));
}

Game::~Game()
{
	utils::Access<SignalKey>(sig_onExit).Emit();
}

void Game::Tick(float delta)
{
	m_timer->Tick([this, delta]()
	{
		Update(delta);
	});
}

void Game::Update(float)
{
	float elapsed = (float)m_timer->GetTotalSeconds() - m_preUpdateTime;
	if (elapsed > 1)
	{
		utils::Log::i("Game::Update", utils::Format("Get FPS: {}", m_timer->GetFramesPerSecond()));
		utils::Access<SignalKey>(sig_resetTimer).Emit(m_timer->GetTotalSeconds());
		m_lastFrame = m_frame;
	}
	else if (elapsed < 0)
	{
		utils::Log::e("Game::Update", utils::Format("Reset time due to undefined behavior: {}", elapsed));
		utils::Access<SignalKey>(sig_resetTimer).Emit(m_timer->GetTotalSeconds());
	}
	m_frame++;
}

void Game::OnSuspending() const
{
	utils::Log::d("Game::OnSuspending", "Suspended");
}

void Game::OnResuming() const
{
	utils::Log::d("Game::OnResuming", "Resumed");
}