#pragma once
#include "Signal.h"

struct IApplicationContext : protected utils::noncopy, protected utils::nonmove
{
protected:
	struct SignalKey;
public:
	utils::Signal<void(short), SignalKey> sig_onFPSChanged;
	utils::Signal<void(int), SignalKey> sig_onKeyDown;
	utils::Signal<void(int), SignalKey> sig_onKeyUp;
	utils::Signal<void(), SignalKey> sig_onSuspend;
	utils::Signal<void(), SignalKey> sig_onResume;

private:
	virtual void Suspend() = 0;
	virtual void Resume() = 0;
	virtual void InvokeKeyDown(int) = 0;
	virtual void InvokeKeyUp(int) = 0;
	virtual void ChangeFPSLimit(short) = 0;
};

struct ApplicationContext : public IApplicationContext
{
public:
	void Suspend() override
	{
		utils::Access<SignalKey>(sig_onSuspend).Emit();
	}
	void Resume() override
	{
		utils::Access<SignalKey>(sig_onResume).Emit();
	}
	void InvokeKeyDown(int key) override
	{
		utils::Access<SignalKey>(sig_onKeyDown).Emit(key);
	}
	void InvokeKeyUp(int key) override
	{
		utils::Access<SignalKey>(sig_onKeyUp).Emit(key);
	}
	void ChangeFPSLimit(short fps) override
	{
		utils::Access<SignalKey>(sig_onFPSChanged).Emit(fps);
	}
};