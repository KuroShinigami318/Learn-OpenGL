#pragma once
namespace DX
{
class StepTimer;
}

struct IApplicationContext;

class Game : protected utils::nonmove
{
public:
	struct SignalKey;

public:
	Game(IApplicationContext& i_ctx);
	~Game();

	utils::Signal_mt<void(), SignalKey> sig_onExit;
	void Tick(float delta);

private:
	// Messages
	void OnSuspending() const;
	void OnResuming() const;
	void Update(float);

	// Rendering loop timer.
	utils::unique_ref<DX::StepTimer>			m_timer;
	uint64_t                                    m_frame;
	uint64_t                                    m_lastFrame;
	float										m_preUpdateTime;
	// Render Thread
	utils::Signal<void(float), SignalKey>		sig_resetTimer;
	std::vector<utils::Connection>				m_connections;
};
