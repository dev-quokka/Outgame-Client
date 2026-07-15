#pragma once

#include "Packet.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <string>
#include <vector>
#include <functional>

#pragma comment(lib, "ws2_32.lib")

class TestClient {
public:
    TestClient() = default;
    ~TestClient();

    // ====================== 접속 ======================
    bool ConnectLoginServer();
    bool TryLogin(const std::string& userId_, const std::string& password_);
    bool ConnectLobby();
    void Disconnect();

    // ====================== 친구 기능 ======================
    void SearchUser();
    void SendFriendRequest();
    void AcceptFriend(const std::string& targetId, uint8_t action);
    void ShowFriendList();
    void ManageFriendRequests();

    // ====================== 코스튬 기능 ======================
    void ChangeCostume();

    // ====================== 파티 기능 ======================
    void ShowPartyInfo();
    void PartyFollow();
    void PartyInvite();
    void PartyInviteAccept(const std::string& senderId, uint8_t accept);
    void PartyLeave();
    void PartyKick();
    void PartyDelegate();
    void MatchStart();
    void ManagePartyInvites();

private:
    // ====================== 네트워크 ======================
    // recv 스레드 (모든 패킷을 받아서 응답/알림으로 분류)
    void RecvThread();
    // PacketId가 알림 패킷인지 확인
    bool IsNotifyPacket(uint16_t packetId);
    // 알림 패킷 화면 출력
    void HandleNotify(uint16_t packetId, const char* data, int len);
    // 메인 스레드에서 응답 대기
    std::vector<char> WaitResponse(int timeoutMs = 5000);


    std::vector<FriendInfo> friends;          // 진짜 친구 목록
    std::vector<FriendInfo> sentRequests;     // 내가 보낸 요청 목록
    std::vector<FriendInfo> receivedRequests; // 받은 요청 목록


    // 파티 요청 알림
    std::vector<PARTY_INVITE_NOTIFY>   pendingPartyInvites;

    // ====================== 소켓 ======================
    SOCKET loginSkt = INVALID_SOCKET;
    SOCKET lobbySkt = INVALID_SOCKET;

    // ====================== recv 스레드 ======================
    std::thread recvThread;
    std::atomic<bool> running{ false };

    // 응답 큐 (메인 스레드가 꺼내가는 큐)
    std::queue<std::vector<char>> responseQueue;
    std::mutex responseMtx;
    std::mutex pendingMtx; // 요청 목록 사용할때 쓰는 뮤텍스
    std::condition_variable responseCv;

    // ====================== 세션 데이터 ======================
    std::string userId;
    char token[MAX_JWT_TOKEN_LEN] = {};
    char lobbyIp[16] = {};
    uint16_t lobbyPort = 0;

    // 로그인 시 받은 데이터
    UserInfo    userInfo;
    Currency userCurrency;
    Costume  userCostume;

    // 파티 정보
    struct PartyMemberInfo {
        std::string userId;
        uint16_t userLevel = 0;
        uint32_t head = 0;
        uint32_t body = 0;
        uint32_t legs = 0;
        uint32_t feet = 0;
        uint8_t  onlineStatus = 1;
    };

    // 현재 파티 상태
    uint32_t currentPartyId = 0;
    std::string partyLeaderId;
    std::vector<PartyMemberInfo> partyMembers;
};