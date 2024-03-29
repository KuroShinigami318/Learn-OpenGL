#include "stdafx.h"
#include "ApplicationContext.h"
#include "Log.h"
#include "Game.h"
#include "StepTimer.h"

Game::Game(IApplicationContext& i_ctx, utils::IMessageQueue& nextFrameQueue) : m_frame(0), m_preUpdateTime(0)
	, m_timer(new DX::StepTimer()), m_soundManager(i_ctx.soundManager)
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
		ASSERT(false);
	}));
	m_connections.push_back(i_ctx.soundManager.cb_onError.Connect([](const ISoundLoader::Error& error)
	{
		ERROR_LOG("SOUND_ERROR", "{}\n", error.reason);
	}));
	m_connections.push_back(i_ctx.soundManager.sig_onSoundPlaying.Connect([](ISoundPlayer* playing)
	{
		INFO_LOG_WITH_FORMAT(utils::Log::TextFormat(utils::Log::TextStyle::Reset, {0, 255, 0}), "SOUND_INFO", "{}\n", playing->GetSoundName());
	}));
	m_connections.push_back(i_ctx.sig_onSuspend.ConnectAsync(&nextFrameQueue, &Game::OnSuspending, this));
	m_connections.push_back(i_ctx.sig_onResume.ConnectAsync(&nextFrameQueue, &Game::OnResuming, this));
	LoadPlaylist();
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
	m_soundManager.Suspend();
	utils::Log::d("Game::OnSuspending", "Suspended", utils::Log::TextFormat(utils::Log::TextStyle::Reset, { 0, 255, 0 }));
}

void Game::OnResuming() const
{
	utils::Log::d("Game::OnResuming", "Resumed", utils::Log::TextFormat(utils::Log::TextStyle::Reset, { 0, 255, 0 }));
	m_soundManager.Resume();
}

void Game::LoadPlaylist()
{
	std::ifstream playlistStream;
	std::string tempbuff;
	playlistStream.open("assets/playlist", std::ios::in);
	if (playlistStream)
	{
		std::deque<std::string> playlist;
		while (!playlistStream.eof())
		{
			std::getline(playlistStream, tempbuff);
			tempbuff = "assets/" + tempbuff;
			playlist.push_back(tempbuff);
		}
		m_soundManager.PlayGroupSound(playlist);
		playlistStream.close();
	}
}