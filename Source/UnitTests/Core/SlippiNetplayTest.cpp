#include "Core/Slippi/SlippiNetplay.h"
#include <enet/enet.h>
#include <gtest/gtest.h>

class SlippiNetplayClientTestAccess
{
  public:
	static ENetPeer *Init(SlippiNetplayClient &c)
	{
		c.m_remotePlayerCount = 1;
		c.playerActive[0] = true;
		c.playerActive[1] = true;
		c.matchInfo.remotePlayerSelections[0].playerIdx = 1;
		c.pingUs[0] = 0;
		c.lastFrameAcked[0] = 0;
		c.lastFrameTiming[0] = {};
		c.frameOffsetData[0] = {};
		c.slippiConnectStatus = SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;
		c.m_client = enet_host_create(nullptr, 1, 3, 0, 0);
		ENetAddress address = {};
		enet_address_set_host(&address, "127.0.0.1");
		address.port = 60000;
		auto peer = enet_host_connect(c.m_client, &address, 3, 0);
		peer->state = ENET_PEER_STATE_CONNECTED;
		c.m_server.push_back(peer);
		return peer;
	}
	static void Cleanup(SlippiNetplayClient &c) { c.m_server.clear(); }
	static void Receive(SlippiNetplayClient &c, sf::Packet &packet, ENetPeer *peer) { c.OnData(packet, peer); }
	static size_t HistorySize(SlippiNetplayClient &c) { return c.remotePadHistory[0].size(); }
	static void Bot(SlippiNetplayClient &c, bool local)
	{
		c.localPlayerIsBot = local;
		c.remotePlayerIsBot[0] = !local;
	}
	static std::unique_ptr<sf::Packet> LastSent(SlippiNetplayClient &c)
	{
		std::unique_ptr<sf::Packet> p;
		while (!c.m_async_queue.Empty())
			c.m_async_queue.Pop(p);
		return p;
	}
	static void RemoteSelection(SlippiNetplayClient &c, ENetPeer *peer)
	{
		SlippiPlayerSelections selection;
		selection.playerIdx = 1;
		selection.isCharacterSelected = true;
		sf::Packet p;
		c.writeToPacket(p, selection);
		c.OnData(p, peer);
	}
};

class SlippiNetplay370 : public testing::Test
{
  protected:
	SlippiNetplayClient client{true};
	ENetPeer *peer = nullptr;
	void SetUp() override
	{
		ASSERT_EQ(0, enet_initialize());
		peer = SlippiNetplayClientTestAccess::Init(client);
		ASSERT_NE(nullptr, peer);
	}
	void TearDown() override { SlippiNetplayClientTestAccess::Cleanup(client); }
	void Receive(s32 last, int count)
	{
		sf::Packet p;
		p << static_cast<MessageId>(NP_MSG_SLIPPI_PAD) << last << u8(1) << s32(0) << u32(0);
		for (int i = 0; i < count; ++i)
		{
			u8 pad[SLIPPI_PAD_WIRE_SIZE] = {};
			pad[0] = static_cast<u8>(last - i);
			pad[8] = static_cast<u8>(0x80 + last - i);
			p.append(pad, sizeof(pad));
		}
		SlippiNetplayClientTestAccess::Receive(client, p, peer);
	}
};

TEST_F(SlippiNetplay370, ReceivesNineByteInputsWithoutLosingRng)
{
	Receive(4, 4);
	auto output = client.GetSlippiRemotePad(0, 7);
	ASSERT_EQ(4, output->latestFrame);
	ASSERT_EQ(4 * SLIPPI_PAD_FULL_SIZE, output->data.size());
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_EQ(4 - i, output->data[i * 12]);
		EXPECT_EQ(0x84 - i, output->data[i * 12 + 8]);
		EXPECT_EQ(0, output->data[i * 12 + 9]);
	}
}

TEST_F(SlippiNetplay370, RejectsGapsAndOldPacketsWithoutAcknowledgingMissingInputs)
{
	Receive(4, 4);
	auto queued = enet_list_size(&peer->outgoingUnreliableCommands);
	Receive(8, 2); // Missing frames 5 and 6.
	EXPECT_EQ(4, client.GetSlippiRemotePad(0, 7)->latestFrame);
	EXPECT_EQ(queued, enet_list_size(&peer->outgoingUnreliableCommands));
	Receive(3, 3);
	EXPECT_EQ(queued, enet_list_size(&peer->outgoingUnreliableCommands));
	Receive(8, 4);
	EXPECT_EQ(8, client.GetSlippiRemotePad(0, 10)->latestFrame);
	EXPECT_EQ(queued + 1, enet_list_size(&peer->outgoingUnreliableCommands));
}

TEST_F(SlippiNetplay370, RematchResetsHistoryBeforeInputsAndLateSelectionsKeepThem)
{
	SlippiPlayerSelections selection;
	selection.isCharacterSelected = true;
	client.SetMatchSelections(selection);
	Receive(4, 4);
	client.StartSlippiGame();
	client.SetMatchSelections(selection);
	EXPECT_EQ(0, client.GetSlippiRemotePad(0, 7)->latestFrame);
	EXPECT_EQ(0, SlippiNetplayClientTestAccess::HistorySize(client));
	Receive(2, 2);
	SlippiNetplayClientTestAccess::RemoteSelection(client, peer);
	client.SetMatchSelections(selection);
	EXPECT_EQ(2, client.GetSlippiRemotePad(0, 7)->latestFrame);
	EXPECT_EQ(2, SlippiNetplayClientTestAccess::HistorySize(client));
}

TEST_F(SlippiNetplay370, DuplicateFramesKeepTheirInputsAndRefreshMatchingChecksums)
{
	u8 pad[12] = {};
	pad[0] = 1;
	pad[8] = 0x91;
	client.SendSlippiPad(std::make_unique<SlippiPad>(1, 5, 42, pad));
	client.SendSlippiPad(std::make_unique<SlippiPad>(1, 6, 43, pad));
	pad[8] = 0x92;
	client.SendSlippiPad(std::make_unique<SlippiPad>(1, 7, 44, pad));
	auto sent = SlippiNetplayClientTestAccess::LastSent(client);
	ASSERT_NE(nullptr, sent);
	ASSERT_EQ(14 + SLIPPI_PAD_WIRE_SIZE, sent->getDataSize());
	MessageId mid;
	s32 frame, checksumFrame;
	u8 port;
	u32 checksum;
	*sent >> mid >> frame >> port >> checksumFrame >> checksum;
	EXPECT_EQ(1, frame);
	EXPECT_EQ(6, checksumFrame);
	EXPECT_EQ(43u, checksum);
	EXPECT_EQ(0x91, static_cast<const u8 *>(sent->getData())[14 + 8]);
}

TEST_F(SlippiNetplay370, BotKeepsDeployedChecksumPolicyAndStillSendsRng)
{
	SlippiNetplayClientTestAccess::Bot(client, true);
	u8 pad[12] = {};
	pad[8] = 0x91;
	client.SendSlippiPad(std::make_unique<SlippiPad>(1, 5, 42, pad));
	auto sent = SlippiNetplayClientTestAccess::LastSent(client);
	ASSERT_NE(nullptr, sent);
	MessageId mid;
	s32 frame, checksumFrame;
	u8 port;
	u32 checksum;
	*sent >> mid >> frame >> port >> checksumFrame >> checksum;
	EXPECT_EQ(0, checksumFrame);
	EXPECT_EQ(0u, checksum);
	EXPECT_EQ(0x91, static_cast<const u8 *>(sent->getData())[14 + 8]);
}

TEST_F(SlippiNetplay370, RunaheadWindowIncludesRequestedFrameAndRng)
{
	SlippiNetplayClientTestAccess::Bot(client, false);
	Receive(30, 30);
	auto output = client.GetSlippiRemotePad(0, 7, 18);
	ASSERT_EQ(18, output->latestFrame);
	ASSERT_EQ(7 * 12, output->data.size());
	EXPECT_EQ(18, output->data[0]);
	EXPECT_EQ(0x92, output->data[8]);
	EXPECT_EQ(12, output->data[6 * 12]);
}
