#pragma once
#include "result.h"
#include "make_playlist_result.h"

namespace DX
{
class StepTimer;
}

class SoundManager;
class ISoundPlayer;

struct IApplicationContext;

class Game : protected utils::nonmove
{
protected:
	struct SignalKey;

public:
	enum class LoadErrorCode
	{
		ReadPlaylistFailed,
		InvalidFolder
	};
	using LoadError = utils::Error<LoadErrorCode, make_playlist_error>;
	using LoadResult = Result<void, LoadError>;

public:
	Game(IApplicationContext& i_ctx, utils::IMessageQueue& nextFrameQueue);
	~Game();

	utils::Signal_mt<void(), SignalKey> sig_onExit;
	void Tick(float delta);
	LoadResult LoadPlaylist(const std::string& folder) const;
	void SuspendSound() const;
	void ResumeSound() const;
	void RequestExit();

private:
	// Messages
	void OnSuspending() const;
	void OnResuming() const;
	void Update(float);
	void OnLoadedPlaylist(SoundManager::LoadResult);

	SoundManager&								m_soundManager;
	ISoundPlayer*								m_currentPlaying;
	// Rendering loop timer.
	utils::unique_ref<DX::StepTimer>			m_timer;
	uint64_t                                    m_frame;
	uint64_t                                    m_lastFrame;
	float										m_preUpdateTime;

	utils::Signal<void(float), SignalKey>		sig_resetTimer;
	std::vector<utils::Connection>				m_connections;
};

