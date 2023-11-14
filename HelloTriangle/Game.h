#pragma once

struct IApplicationContext;

class Game : public utils::IWorkerRunnable<void()>, protected utils::nonmove
{
private:
	struct SignalKey;
	utils::unique_ref<utils::WorkerThread<void()>> renderThread;
public:
	using MessageType = utils::BindedCallable<void()>;

public:
	Game(IApplicationContext& i_ctx);
	~Game();

	uint32_t GetFramesPerSecond() const;

	double GetElapsedSeconds() const;

	const utils::WorkerThread<void()>* GetThread() const;

	void PauseRenderThread(bool i_pause);

	void ChangeModeRenderThread(utils::MODE i_mode, size_t i_maxQueue = 0);

	void CleanQueueInRenderThread();

	void SyncRenderThread();

	utils::MessageHandle<void> PushMessage(MessageType i_message);

	bool IsAny(int value, std::vector<int> list);

	decltype(renderThread->sig_onFirstUpdate)& sig_onRenderTheadFirstUpdate;
	utils::Signal_mt<void(float), SignalKey> sig_onTick;

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
		utils::Access<SignalKey>(sig_onTick).DisconnectAll();
	}

	// Rendering loop timer.
	uint64_t                                    m_frame;
	uint64_t                                    m_lastFrame;
	float										m_preUpdateTime;
	// Render Thread
	utils::Signal_mt<void(float), SignalKey>	sig_resetTimer;
	std::vector<utils::Connection>				m_connections;
};

