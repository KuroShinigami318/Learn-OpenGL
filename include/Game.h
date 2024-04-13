#pragma once
#include "result.h"

namespace DX
{
class StepTimer;
}

class SoundManager;

struct IApplicationContext;

class Game : protected utils::nonmove
{
protected:
	struct SignalKey;

public:
	enum class LoadErrorCode
	{
		InvalidFolder
	};
	using LoadError = utils::Error<LoadErrorCode>;
	using LoadResult = Result<void, LoadError>;

public:
	Game(IApplicationContext& i_ctx, utils::IMessageQueue& nextFrameQueue);
	~Game();

	utils::Signal_mt<void(), SignalKey> sig_onExit;
	void Tick(float delta);
	LoadResult LoadPlaylist(const std::string& folder);

private:
	// Messages
	void OnSuspending() const;
	void OnResuming() const;
	void Update(float);
	void OnLoadedPlaylist(SoundManager::LoadResult) const;

	SoundManager&								m_soundManager;
	// Rendering loop timer.
	utils::unique_ref<DX::StepTimer>			m_timer;
	uint64_t                                    m_frame;
	uint64_t                                    m_lastFrame;
	float										m_preUpdateTime;
	// Render Thread
	utils::Signal<void(float), SignalKey>		sig_resetTimer;
	std::vector<utils::Connection>				m_connections;
};

