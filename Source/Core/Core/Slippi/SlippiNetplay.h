// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FifoQueue.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "Core/Slippi/SlippiPad.h"
#include "InputCommon/GCPadStatus.h"
#include <SFML/Network/Packet.hpp>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Qos2.h>
#endif

#define SLIPPI_ONLINE_LOCKSTEP_INTERVAL 30 // Number of frames to wait before attempting to time-sync
#define SLIPPI_PING_DISPLAY_INTERVAL 60

struct SlippiRemotePadOutput
{
	int32_t latestFrame;
	u8 playerIdx;
	std::vector<u8> data;
};

class SlippiPlayerSelections
{
  public:
	u8 playerIdx = 0;
	u8 characterId = 0;
	u8 characterColor = 0;
	bool isCharacterSelected = false;

	u16 stageId = 0;
	bool isStageSelected = false;

	u32 rngOffset = 0;

	void Merge(SlippiPlayerSelections &s)
	{
		this->rngOffset = s.rngOffset;

		if (s.isStageSelected)
		{
			this->stageId = s.stageId;
			this->isStageSelected = true;
		}

		if (s.isCharacterSelected)
		{
			this->characterId = s.characterId;
			this->characterColor = s.characterColor;
			this->isCharacterSelected = true;
		}
	}

	void Reset()
	{
		characterId = 0;
		characterColor = 0;
		isCharacterSelected = false;

		stageId = 0;
		isStageSelected = false;

		rngOffset = 0;
	}
};

class SlippiMatchInfo
{
  public:
	SlippiPlayerSelections localPlayerSelections;
	SlippiPlayerSelections remotePlayerSelections[3];

	void Reset()
	{
		localPlayerSelections.Reset();
		for (int i = 0; i < 3; i++)
		{
			remotePlayerSelections[i].Reset();
		}
	}
};

class SlippiNetplayClient
{
  public:
	void ThreadFunc();
	void SendAsync(std::unique_ptr<sf::Packet> packet);

	SlippiNetplayClient(bool isDecider); // Make a dummy client
	SlippiNetplayClient(const std::string &address1, const u16 remotePort1, const std::string &address2,
	                    const u16 remotePort2, const u16 localPort, bool isDecider, u8 playerIdx);
	~SlippiNetplayClient();

	// Slippi Online
	enum class SlippiConnectStatus
	{
		NET_CONNECT_STATUS_UNSET,
		NET_CONNECT_STATUS_INITIATED,
		NET_CONNECT_STATUS_CONNECTED,
		NET_CONNECT_STATUS_FAILED,
		NET_CONNECT_STATUS_DISCONNECTED,
	};

	bool IsDecider();
	bool IsConnectionSelected();
	SlippiConnectStatus GetSlippiConnectStatus();
	void StartSlippiGame();
	void SendConnectionSelected();
	void SendSlippiPad(std::unique_ptr<SlippiPad> pad);
	void SetMatchSelections(SlippiPlayerSelections &s);
	std::unique_ptr<SlippiRemotePadOutput> GetSlippiRemotePad(int32_t curFrame);
	SlippiMatchInfo *GetMatchInfo();
	u64 GetSlippiPing();
	int32_t GetSlippiLatestRemoteFrame();
	s32 CalcTimeOffsetUs();

  protected:
	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players;
		std::recursive_mutex async_queue_write;
	} m_crit;

	Common::FifoQueue<std::unique_ptr<sf::Packet>, false> m_async_queue;

	ENetHost *m_client = nullptr;
	std::vector<ENetPeer *> m_server;
	std::thread m_thread;

	std::string m_selected_game;
	Common::Flag m_is_running{false};
	Common::Flag m_do_loop{true};

	unsigned int m_minimum_buffer_size = 6;

	u32 m_current_game = 0;

	// Slippi Stuff
	struct FrameTiming
	{
		int32_t frame;
		u64 timeUs;
	};

	struct
	{
		// TODO: Should the buffer size be dynamic based on time sync interval or not?
		int idx;
		std::vector<s32> buf;
	} frameOffsetData;

	bool isConnectionSelected = false;
	bool isDecider = false;
	u8 playerIdx = 0;
	int32_t lastFrameAcked;
	bool hasGameStarted = false;
	FrameTiming lastFrameTiming;
	u64 pingUs;
	std::deque<std::unique_ptr<SlippiPad>> localPadQueue;  // most recent inputs at start of deque
	std::deque<std::unique_ptr<SlippiPad>> remotePadQueue; // most recent inputs at start of deque
	Common::FifoQueue<FrameTiming, false> ackTimers;
	SlippiConnectStatus slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_UNSET;
	SlippiMatchInfo matchInfo;

	bool m_is_recording = false;

	void writeToPacket(sf::Packet &packet, SlippiPlayerSelections &s);
	std::unique_ptr<SlippiPlayerSelections> readSelectionsFromPacket(sf::Packet &packet);

  private:
	unsigned int OnData(sf::Packet &packet);
	void Send(sf::Packet &packet);
	void Disconnect();

	bool m_is_connected = false;

#ifdef _WIN32
	HANDLE m_qos_handle;
	QOS_FLOWID m_qos_flow_id;
#endif

	u32 m_timebase_frame = 0;
};
