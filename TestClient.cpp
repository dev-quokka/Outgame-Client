#include "TestClient.h"

TestClient::~TestClient() {
    Disconnect();
}

void TestClient::Disconnect() {
    running = false;

    if (lobbySkt != INVALID_SOCKET) {
        shutdown(lobbySkt, SD_BOTH);
        closesocket(lobbySkt);
        lobbySkt = INVALID_SOCKET;
    }
    if (loginSkt != INVALID_SOCKET) {
        shutdown(loginSkt, SD_BOTH);
        closesocket(loginSkt);
        loginSkt = INVALID_SOCKET;
    }
    if (recvThread.joinable()) {
        recvThread.join();
    }
    WSACleanup();
}

// ====================== 로그인 서버 접속 ======================

bool TestClient::ConnectLoginServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    loginSkt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (loginSkt == INVALID_SOCKET) {
        std::cerr << "[Login] 소켓 생성 실패\n";
        return false;
    }
    SOCKADDR_IN addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9001);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    std::cout << "[Login] 로그인 서버 접속 중...\n";
    if (connect(loginSkt, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
        std::cerr << "[Login] 접속 실패\n";
        return false;
    }
    std::cout << "[Login] 로그인 서버 접속 성공\n";
    return true;
}

bool TestClient::TryLogin(const std::string& userId_, const std::string& password_) {
    userId = userId_;

    USER_LOGIN_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::USER_LOGIN_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.userId, userId_.c_str(), _TRUNCATE);
    strncpy_s(req.userPassword, password_.c_str(), _TRUNCATE);
    send(loginSkt, (char*)&req, sizeof(req), 0);

    constexpr int LOGIN_RECV_SIZE = 4096;
    char recvBuf[LOGIN_RECV_SIZE] = {};
    int totalReceived = 0;

    while (totalReceived < LOGIN_RECV_SIZE) {
        int len = recv(loginSkt, recvBuf + totalReceived,
            LOGIN_RECV_SIZE - totalReceived, 0);
        if (len <= 0) break;
        totalReceived += len;

        if (totalReceived >= (int)sizeof(USER_LOGIN_RESPONSE)) {
            auto header = reinterpret_cast<USER_LOGIN_RESPONSE*>(recvBuf);
            if (!header->isSuccess) break;

            int expectedTotal = sizeof(USER_LOGIN_RESPONSE)
                + sizeof(USER_INVENTORY_PACKET)
                + sizeof(USER_FRIEND_PACKET);
            if (totalReceived >= expectedTotal) break;
        }
    }

    int offset = 0;
    auto loginRes = reinterpret_cast<USER_LOGIN_RESPONSE*>(recvBuf + offset);
    offset += sizeof(USER_LOGIN_RESPONSE);

    if (!loginRes->isSuccess) {
        std::cerr << "[Login] 로그인 실패. failCode: "
            << (int)loginRes->failCode << '\n';
        return false; 
    }

    // 세션 데이터 저장
    strncpy_s(token, loginRes->token, _TRUNCATE);
    strncpy_s(lobbyIp, loginRes->ip, _TRUNCATE);
    lobbyPort = loginRes->port;
    userInfo = loginRes->userinfo;
    userCurrency = loginRes->currency;
    userCostume = loginRes->costume;

    std::cout << "[Login] 로그인 성공!\n";
    std::cout << "  유저: " << userId << " / Lv." << userInfo.level
        << " / BP: " << userCurrency.bp
        << " / GCoin: " << userCurrency.gcoin << '\n';
    std::cout << "  로비 서버: " << lobbyIp << ":" << lobbyPort << '\n';

    // 인벤토리
    auto invenRes = reinterpret_cast<USER_INVENTORY_PACKET*>(recvBuf + offset);
    offset += sizeof(USER_INVENTORY_PACKET);
    std::cout << "  인벤토리 아이템 수: " << invenRes->itemCount << '\n';

    // 친구 목록
    auto friendRes = reinterpret_cast<USER_FRIEND_PACKET*>(recvBuf + offset);
    uint16_t friendCount = friendRes->friendCount;
    std::cout << "  친구 수: " << friendCount << '\n';

    friends.clear();
    sentRequests.clear();
    receivedRequests.clear();

    for (int i = 0; i < friendCount; i++) {
        FriendInfo& fi = friendRes->friends[i];

        switch (fi.friendStatus) {
        case 0: sentRequests.push_back(fi); break;
        case 1: friends.push_back(fi); break;
        case 2: receivedRequests.push_back(fi); break;
        }
    }

    // 로그인 서버 소켓 닫기
    shutdown(loginSkt, SD_BOTH);
    closesocket(loginSkt);
    loginSkt = INVALID_SOCKET;
    return true;
}

// ====================== 로비 서버 접속 ======================

bool TestClient::ConnectLobby() {
    lobbySkt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (lobbySkt == INVALID_SOCKET) {
        std::cerr << "[Lobby] 소켓 생성 실패\n";
        return false;
    }

    SOCKADDR_IN addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(lobbyPort);
    inet_pton(AF_INET, lobbyIp, &addr.sin_addr);

    std::cout << "[Lobby] 로비 서버 접속 중...\n";
    if (connect(lobbySkt, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
        std::cerr << "[Lobby] 접속 실패\n";
        return false;
    }

    // JWT 토큰으로 검증 요청
    USER_LOBBY_CONNECT_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::USER_LOBBY_CONNECT_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.userId, userId.c_str(), _TRUNCATE);
    strncpy_s(req.token, token, _TRUNCATE);

    send(lobbySkt, (char*)&req, sizeof(req), 0);

    char recvBuf[PACKET_SIZE] = {};
    recv(lobbySkt, recvBuf, PACKET_SIZE, 0);

    auto res = reinterpret_cast<USER_LOBBY_CONNECT_RESPONSE*>(recvBuf);
    if (!res->isSuccess) {
        std::cerr << "[Lobby] 토큰 검증 실패\n";
        closesocket(lobbySkt);
        lobbySkt = INVALID_SOCKET;
        return false;
    }

    std::cout << "[Lobby] 로비 접속 성공!\n";

    // recv 스레드 시작
    running = true;
    recvThread = std::thread(&TestClient::RecvThread, this);

    return true;
}

// ====================== recv 스레드 ======================

void TestClient::RecvThread() {
    char buffer[PACKET_SIZE];

    while (running) {
        memset(buffer, 0, PACKET_SIZE);
        int len = recv(lobbySkt, buffer, PACKET_SIZE, 0);

        if (len <= 0) {
            if (running) {
                std::cerr << "\n[RecvThread] 서버 연결 끊김\n";
            }
            running = false;
            responseCv.notify_all();
            break;
        }

        auto header = reinterpret_cast<PACKET_HEADER*>(buffer);
        uint16_t packetId = header->PacketId;

        std::vector<char> packet(buffer, buffer + len);

        if (IsNotifyPacket(packetId)) {
            // 알림 패킷 (화면에 바로 출력)
            HandleNotify(packetId, packet.data(), len);
        }
        else {
            // 응답 패킷 (큐에 넣기)
            std::lock_guard<std::mutex> lock(responseMtx);
            responseQueue.push(std::move(packet));
            responseCv.notify_one();
        }
    }
}

bool TestClient::IsNotifyPacket(uint16_t packetId) {
    switch ((PACKET_ID)packetId) {
    case PACKET_ID::FRIEND_REQUEST_NOTIFY:
    case PACKET_ID::FRIEND_ACCEPT_NOTIFY:
    case PACKET_ID::FRIEND_STATUS_NOTIFY:
    case PACKET_ID::COSTUME_CHANGE_NOTIFY:
    case PACKET_ID::PARTY_INVITE_NOTIFY:
    case PACKET_ID::PARTY_INVITE_REJECT_NOTIFY:
    case PACKET_ID::PARTY_INFO_PACKET:
    case PACKET_ID::PARTY_JOIN_NOTIFY:
    case PACKET_ID::PARTY_LEAVE_NOTIFY:
    case PACKET_ID::PARTY_KICK_NOTIFY:
    case PACKET_ID::PARTY_DELEGATE_NOTIFY:
    case PACKET_ID::PARTY_MEMBER_STATUS_NOTIFY:
        return true;
    default:
        return false;
    }
}

void TestClient::HandleNotify(uint16_t packetId, const char* data, int len) {
    std::cout << "\n";

    switch ((PACKET_ID)packetId) {
    case PACKET_ID::FRIEND_REQUEST_NOTIFY: {
        auto p = reinterpret_cast<const FRIEND_REQUEST_NOTIFY*>(data);
        std::cout << "[알림] 친구 요청: " << p->senderId
            << " (Lv." << p->senderLevel << ")\n";

        // receivedRequests에 바로 추가
        FriendInfo fi;
        strncpy_s(fi.friendId, p->senderId, _TRUNCATE);
        fi.friendStatus = 2;  // 받은 요청
        fi.onlineStatus = p->onlineStatus;

        std::lock_guard<std::mutex> lock(pendingMtx);
        receivedRequests.push_back(fi);
        break;
    }
    case PACKET_ID::FRIEND_ACCEPT_NOTIFY: {
        auto p = reinterpret_cast<const FRIEND_ACCEPT_NOTIFY*>(data);
        const char* msg = p->accept == 0 ? "수락" : "거절";
        std::cout << "[알림] " << p->senderId
            << "이(가) 친구 요청을 " << msg << "했습니다\n";

        std::lock_guard<std::mutex> lock(pendingMtx);
        if (p->accept == 0) {
            // 수락 -> sentRequests에서 제거 + friends에 추가
            for (auto it = sentRequests.begin(); it != sentRequests.end(); ++it) {
                if (std::string(it->friendId) == std::string(p->senderId)) {
                    FriendInfo fi = *it;
                    fi.friendStatus = 1;
                    friends.push_back(fi);
                    sentRequests.erase(it);
                    break;
                }
            }
        }
        else {
            // 거절 -> sentRequests에서 제거
            sentRequests.erase(
                std::remove_if(sentRequests.begin(), sentRequests.end(),
                    [&](const FriendInfo& f) {
                        return std::string(f.friendId) == std::string(p->senderId);
                    }),
                sentRequests.end());
        }
        break;
    }
    case PACKET_ID::FRIEND_STATUS_NOTIFY: {
        auto p = reinterpret_cast<const FRIEND_STATUS_NOTIFY*>(data);
        const char* status = p->onlineStatus == 0 ? "오프라인" :
            p->onlineStatus == 1 ? "로비" : "게임중";
        std::cout << "[알림] 친구 상태 변경 (pk: " << p->friendPk
            << ") → " << status << '\n';
        break;
    }
    case PACKET_ID::COSTUME_CHANGE_NOTIFY: {
        auto p = reinterpret_cast<const COSTUME_CHANGE_NOTIFY*>(data);
        std::cout << "[알림] 코스튬 변경: " << p->userId
            << " 슬롯" << (int)p->slot
            << " → " << p->itemCode << '\n';
        break;
    }
    case PACKET_ID::PARTY_INVITE_NOTIFY: {
        auto p = reinterpret_cast<const PARTY_INVITE_NOTIFY*>(data);
        std::cout << "[알림] 파티 초대: " << p->senderId
            << " (Lv." << p->senderLevel
            << ", 파티 " << (int)p->memberCount << "명)\n";

        // 저장
        std::lock_guard<std::mutex> lock(pendingMtx);
        pendingPartyInvites.push_back(*p);
        break;
    }
    case PACKET_ID::PARTY_INFO_PACKET: {
        auto p = reinterpret_cast<const PARTY_INFO_PACKET*>(data);
        std::cout << "[알림] 파티 정보 (ID: " << p->partyId
            << ", 파티장 pk: " << p->leaderPk
            << ", " << (int)p->memberCount << "명)\n";
        for (int i = 0; i < p->memberCount; i++) {
            auto& m = p->members[i];
            std::cout << "  " << m.userId
                << " Lv." << m.userLevel
                << " [" << m.head << "/" << m.body
                << "/" << m.legs << "/" << m.feet << "]\n";
        }
        break;
    }
    case PACKET_ID::PARTY_JOIN_NOTIFY: {
        auto p = reinterpret_cast<const PARTY_JOIN_NOTIFY*>(data);
        std::cout << "[알림] 파티원 입장: " << p->userId
            << " Lv." << p->userLevel << '\n';
        break;
    }
    case PACKET_ID::PARTY_LEAVE_NOTIFY: {
        auto p = reinterpret_cast<const PARTY_LEAVE_NOTIFY*>(data);
        std::cout << "[알림] 파티원 탈퇴 (pk: " << p->userPk << ")";
        if (p->newLeaderPk == 0)
            std::cout << " → 파티 해산";
        else
            std::cout << " → 새 파티장: " << p->newLeaderPk;
        std::cout << '\n';
        break;
    }
    case PACKET_ID::PARTY_KICK_NOTIFY: {
        auto p = reinterpret_cast<const PARTY_KICK_NOTIFY*>(data);
        std::cout << "[알림] 파티 강퇴 (pk: " << p->userPk << ")\n";
        break;
    }
    case PACKET_ID::PARTY_DELEGATE_NOTIFY: {
        auto p = reinterpret_cast<const PARTY_DELEGATE_NOTIFY*>(data);
        std::cout << "[알림] 새 파티장: pk " << p->newLeaderPk << '\n';
        break;
    }
    case PACKET_ID::PARTY_MEMBER_STATUS_NOTIFY: {
        auto p = reinterpret_cast<const PARTY_MEMBER_STATUS_NOTIFY*>(data);
        const char* status = p->onlineStatus == 0 ? "오프라인" : "온라인";
        std::cout << "[알림] 파티원 상태 (pk: " << p->userPk
            << ") → " << status << '\n';
        break;
    }
    default:
        std::cout << "[알림] 알 수 없는 패킷 ID: " << packetId << '\n';
        break;
    }

    std::cout << "> ";  
    std::cout.flush();
}

std::vector<char> TestClient::WaitResponse(int timeoutMs) {
    std::unique_lock<std::mutex> lock(responseMtx);
    bool received = responseCv.wait_for(lock,
        std::chrono::milliseconds(timeoutMs),
        [this] { return !responseQueue.empty() || !running; });

    if (!received || responseQueue.empty()) {
        return {};  // 타임아웃 or 서버 연결 끊김
    }

    auto packet = std::move(responseQueue.front());
    responseQueue.pop();
    return packet;
}

// ====================== 유저 검색 ======================

void TestClient::SearchUser() {
    std::string targetId;
    std::cout << "검색할 유저 ID: ";
    std::cin >> targetId;

    USER_SEARCH_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::USER_SEARCH_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.userId, targetId.c_str(), _TRUNCATE);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[SearchUser] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<USER_SEARCH_RESPONSE*>(packet.data());
    if (!res->isSuccess) {
        std::cout << "[SearchUser] 유저를 찾을 수 없습니다\n";
        return;  // 바로 메뉴로 돌아감
    }

    const char* status =
        res->onlineStatus == 0 ? "오프라인" :
        res->onlineStatus == 1 ? "로비" : "게임중";
    std::cout << "[SearchUser] " << res->userId
        << " Lv." << res->userLevel
        << " (" << status << ")\n";

    // 이미 친구인지 확인
    bool alreadyFriend = false;
    for (auto& f : friends) {
        if (std::string(f.friendId) == targetId) {
            alreadyFriend = true;
            break;
        }
    }

    // 메뉴 표시
    std::cout << "\n";
    if (!alreadyFriend) {
        std::cout << "  1. 친구 요청 보내기\n";
    }
    else {
        std::cout << "  1. (이미 친구입니다)\n";
    }
    if (res->onlineStatus != 0) {
        std::cout << "  2. 따라가기 (파티 입장)\n";
    }
    else {
        std::cout << "  2. (오프라인이라 따라가기 불가)\n";
    }
    std::cout << "  0. 돌아가기\n";
    std::cout << "> ";

    uint16_t select;
    std::cin >> select;

    if (select == 1 && !alreadyFriend) {
        // 친구 요청 보내기
        FRIEND_REQUEST_REQUEST friendReq;
        friendReq.PacketId = (uint16_t)PACKET_ID::FRIEND_REQUEST_REQUEST;
        friendReq.PacketLength = sizeof(friendReq);
        strncpy_s(friendReq.targetId, targetId.c_str(), _TRUNCATE);

        send(lobbySkt, (char*)&friendReq, sizeof(friendReq), 0);
        auto friendPacket = WaitResponse();
        if (friendPacket.empty()) {
            std::cerr << "[FriendRequest] 응답 타임아웃\n";
            return;
        }

        auto friendRes = reinterpret_cast<FRIEND_REQUEST_RESPONSE*>(friendPacket.data());
        if (friendRes->isSuccess) {
            std::cout << "[FriendRequest] " << targetId << "에게 친구 요청 전송\n";

            FriendInfo fi;
            strncpy_s(fi.friendId, targetId.c_str(), _TRUNCATE);
            fi.friendStatus = 0;
            fi.onlineStatus = res->onlineStatus;
            sentRequests.push_back(fi);
        }
        else {
            const char* reason;
            switch (friendRes->failCode) {
            case 1: reason = "이미 친구"; break;
            case 2: reason = "이미 요청중"; break;
            case 3: reason = "유저 없음"; break;
            default: reason = "서버 오류"; break;
            }
            std::cout << "[FriendRequest] 실패: " << reason << '\n';
        }
    }
    else if (select == 2 && res->onlineStatus != 0) {
        // 따라가기
        PARTY_FOLLOW_REQUEST followReq;
        followReq.PacketId = (uint16_t)PACKET_ID::PARTY_FOLLOW_REQUEST;
        followReq.PacketLength = sizeof(followReq);
        strncpy_s(followReq.targetId, targetId.c_str(), _TRUNCATE);

        send(lobbySkt, (char*)&followReq, sizeof(followReq), 0);
        auto followPacket = WaitResponse();
        if (followPacket.empty()) {
            std::cerr << "[PartyFollow] 응답 타임아웃\n";
            return;
        }

        auto followRes = reinterpret_cast<PARTY_FOLLOW_RESPONSE*>(followPacket.data());
        if (followRes->isSuccess) {
            std::cout << "[PartyFollow] 파티 입장 성공 (partyId: "
                << followRes->partyId << ")\n";
        }
        else {
            const char* reason;
            switch (followRes->failCode) {
            case 1: reason = "파티 꽉 참"; break;
            case 2: reason = "유저 없음"; break;
            case 3: reason = "이미 파티에 있음"; break;
            default: reason = "서버 오류"; break;
            }
            std::cout << "[PartyFollow] 실패: " << reason << '\n';
        }
    }
}

// ====================== 친구 목록 ======================

void TestClient::ShowFriendList() {
    std::cout << "\n===== 친구 목록 =====\n";

    int idx = 1;
    bool hasFriend = false;

    for (auto& f : friends) {
        if (f.friendStatus != 1) continue;  // 친구만 표시

        const char* onlineStr =
            f.onlineStatus == 0 ? "오프라인" :
            f.onlineStatus == 1 ? "로비" : "게임중";

        std::cout << "  " << idx << ". " << f.friendId
            << " (" << onlineStr << ")\n";
        idx++;
        hasFriend = true;
    }

    if (!hasFriend) {
        std::cout << "  친구가 없습니다.\n";
        return;
    }

    std::cout << "\n  1. 친구 삭제\n";
    std::cout << "  2. 친구 요청 보내기\n";
    std::cout << "  0. 뒤로가기\n";
    std::cout << "> ";

    uint16_t select;
    std::cin >> select;

    if (select == 1) {
        std::string targetId;
        std::cout << "삭제할 친구 ID: ";
        std::cin >> targetId;
        AcceptFriend(targetId, 1);  // action=1 삭제
    }
    else if (select == 2) {
        SendFriendRequest();
    }
}

// ====================== 친구 요청 관리 ======================

void TestClient::ManageFriendRequests() {
    std::cout << "\n===== 받은 요청 =====\n";
    int idx = 1;

    {
        std::lock_guard<std::mutex> lock(pendingMtx);
        for (auto& f : receivedRequests) {
            const char* onlineStr =
                f.onlineStatus == 0 ? "오프라인" :
                f.onlineStatus == 1 ? "로비" : "게임중";
            std::cout << "  " << idx << ". " << f.friendId
                << " (" << onlineStr << ")\n";
            idx++;
        }
    }

    std::cout << "\n===== 보낸 요청 =====\n";
    for (auto& f : sentRequests) {
        std::cout << "  " << idx << ". " << f.friendId << " (대기중)\n";
        idx++;
    }

    if (idx == 1) {
        std::cout << "  요청이 없습니다.\n";
        return;
    }

    std::cout << "\n  1. 요청 수락\n";
    std::cout << "  2. 요청 거절/취소\n";
    std::cout << "  0. 뒤로가기\n";
    std::cout << "> ";

    uint16_t select;
    std::cin >> select;

    if (select == 1) {
        std::string targetId;
        std::cout << "수락할 유저 ID: ";
        std::cin >> targetId;
        AcceptFriend(targetId, 0);

        { // receivedRequests에서 찾아서 friends로 이동
            std::lock_guard<std::mutex> lock(pendingMtx);
            for (auto it = receivedRequests.begin(); it != receivedRequests.end(); ++it) {
                if (std::string(it->friendId) == targetId) {
                    FriendInfo fi = *it;
                    fi.friendStatus = 1;
                    friends.push_back(fi);
                    receivedRequests.erase(it);
                    return;  // 찾았으면 끝
                }
            }
        }

        // sentRequests에서도 찾아서 friends로 이동
        for (auto it = sentRequests.begin(); it != sentRequests.end(); ++it) {
            if (std::string(it->friendId) == targetId) {
                FriendInfo fi = *it;
                fi.friendStatus = 1;
                friends.push_back(fi);
                sentRequests.erase(it);
                return;  // 찾았으면 끝
            }
        }
    }
    else if (select == 2) {
        std::string targetId;
        std::cout << "거절/취소할 유저 ID: ";
        std::cin >> targetId;
        AcceptFriend(targetId, 1);

        // receivedRequests에서 제거
        {
            std::lock_guard<std::mutex> lock(pendingMtx);
            receivedRequests.erase(
                std::remove_if(receivedRequests.begin(),
                    receivedRequests.end(),
                    [&](const FriendInfo& f) {
                        return std::string(f.friendId) == targetId;
                    }),
                receivedRequests.end());
        }

        // sentRequests에서도 제거 (보낸 요청 취소일 수 있으니)
        sentRequests.erase(
            std::remove_if(sentRequests.begin(), sentRequests.end(),
                [&](const FriendInfo& f) {
                    return std::string(f.friendId) == targetId;
                }),
            sentRequests.end());
    }
}


// ====================== 친구 요청 ======================

void TestClient::SendFriendRequest() {
    std::string targetId;
    std::cout << "친구 요청할 유저 ID: ";
    std::cin >> targetId;

    FRIEND_REQUEST_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::FRIEND_REQUEST_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.targetId, targetId.c_str(), _TRUNCATE);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[FriendRequest] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<FRIEND_REQUEST_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[FriendRequest] " << targetId << "에게 친구 요청 전송\n";
    }
    else {
        const char* reason;
        switch (res->failCode) {
        case 1: reason = "이미 친구"; break;
        case 2: reason = "이미 요청중"; break;
        case 3: reason = "유저 없음"; break;
        default: reason = "서버 오류"; break;
        }
        std::cout << "[FriendRequest] 실패: " << reason << '\n';
    }
}

// ====================== 친구 수락/거절 ======================

void TestClient::AcceptFriend(const std::string& targetId, uint8_t action) {
    FRIEND_ACTION_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::FRIEND_ACTION_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.targetId, targetId.c_str(), _TRUNCATE);
    req.action = action;

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[AcceptFriend] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<FRIEND_ACTION_RESPONSE*>(packet.data());
    if (!res->isSuccess) {
        std::cout << "[AcceptFriend] 실패\n";
    }
}

// ====================== 코스튬 변경 ======================

void TestClient::ChangeCostume() {
    uint16_t slot;
    uint32_t itemCode;
    std::cout << "슬롯 (1=head, 2=body, 3=legs, 4=feet): ";
    std::cin >> slot;
    std::cout << "아이템 코드: ";
    std::cin >> itemCode;

    COSTUME_CHANGE_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::COSTUME_CHANGE_REQUEST;
    req.PacketLength = sizeof(req);
    req.slot = static_cast<uint8_t>(slot);
    req.itemCode = itemCode;

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[Costume] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<COSTUME_CHANGE_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[Costume] 슬롯 " << (int)res->slot
            << " → " << res->itemCode << " 변경 완료\n";
    }
    else {
        const char* reason;
        switch (res->failCode) {
        case 1: reason = "인벤에 없는 아이템"; break;
        case 2: reason = "잘못된 슬롯"; break;
        default: reason = "서버 오류"; break;
        }
        std::cout << "[Costume] 실패: " << reason << '\n';
    }
}

// ====================== 파티 관리 ======================

void TestClient::ManagePartyInvites() {
    std::lock_guard<std::mutex> lock(pendingMtx);

    std::cout << "\n===== 파티 초대 목록 =====\n";

    if (pendingPartyInvites.empty()) {
        std::cout << "  초대가 없습니다.\n";
        return;
    }

    for (int i = 0; i < (int)pendingPartyInvites.size(); i++) {
        auto& p = pendingPartyInvites[i];
        std::cout << "  " << i + 1 << ". " << p.senderId
            << " (Lv." << p.senderLevel
            << ", 파티 " << (int)p.memberCount << "명)\n";
    }

    std::cout << "\n  1. 초대 수락\n";
    std::cout << "  2. 초대 거절\n";
    std::cout << "  0. 뒤로가기\n";
    std::cout << "> ";

    uint16_t select;
    std::cin >> select;

    if (select == 1 || select == 2) {
        std::string senderId;
        std::cout << "유저 ID: ";
        std::cin >> senderId;

        uint8_t accept = (select == 1) ? 0 : 1;
        PartyInviteAccept(senderId, accept);

        // 처리된 초대 제거
        pendingPartyInvites.erase(
            std::remove_if(pendingPartyInvites.begin(),
                pendingPartyInvites.end(),
                [&](const PARTY_INVITE_NOTIFY& p) {
                    return std::string(p.senderId) == senderId;
                }),
            pendingPartyInvites.end());
    }
}


// ====================== 파티 따라가기 ======================

void TestClient::PartyFollow() {
    std::string targetId;
    std::cout << "따라갈 유저 ID: ";
    std::cin >> targetId;

    PARTY_FOLLOW_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::PARTY_FOLLOW_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.targetId, targetId.c_str(), _TRUNCATE);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[PartyFollow] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<PARTY_FOLLOW_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[PartyFollow] 파티 입장 성공 (partyId: " << res->partyId << ")\n";
    }
    else {
        const char* reason;
        switch (res->failCode) {
        case 1: reason = "파티장 아님"; break;
        case 2: reason = "파티 꽉 참"; break;
        case 3: reason = "이미 파티에 있음"; break;
        default: reason = "유저 없음 또는 서버 오류"; break;
        }
        std::cout << "[PartyFollow] 실패: " << reason << '\n';
    }
}

// ====================== 파티 초대 ======================

void TestClient::PartyInvite() {
    std::string targetId;
    std::cout << "초대할 유저 ID: ";
    std::cin >> targetId;

    PARTY_INVITE_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::PARTY_INVITE_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.targetId, targetId.c_str(), _TRUNCATE);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[PartyInvite] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<PARTY_INVITE_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[PartyInvite] " << targetId << "에게 초대 전송\n";
    }
    else {
        std::cout << "[PartyInvite] 실패. failCode: " << (int)res->failCode << '\n';
    }
}

// ====================== 파티 초대 수락/거절 ======================

void TestClient::PartyInviteAccept(const std::string& senderId, uint8_t accept) {
    PARTY_INVITE_ACCEPT_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::PARTY_INVITE_ACCEPT_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.senderId, senderId.c_str(), _TRUNCATE);
    req.accept = accept;

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[PartyInviteAccept] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<PARTY_INVITE_ACCEPT_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        if (accept == 0) {
            std::cout << "[PartyInviteAccept] 파티 입장 (partyId: "
                << res->partyId << ")\n";
        }
        else {
            std::cout << "[PartyInviteAccept] 초대 거절\n";
        }
    }
    else {
        std::cout << "[PartyInviteAccept] 실패. failCode: "
            << (int)res->failCode << '\n';
    }
}

// ====================== 파티 탈퇴 ======================

void TestClient::PartyLeave() {
    PARTY_LEAVE_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::PARTY_LEAVE_REQUEST;
    req.PacketLength = sizeof(req);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[PartyLeave] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<PARTY_LEAVE_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[PartyLeave] 파티 탈퇴 완료\n";
    }
    else {
        std::cout << "[PartyLeave] 실패. failCode: " << (int)res->failCode << '\n';
    }
}

// ====================== 파티 강퇴 ======================

void TestClient::PartyKick() {
    std::string targetId;
    std::cout << "강퇴할 유저 ID: ";
    std::cin >> targetId;

    PARTY_KICK_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::PARTY_KICK_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.targetId, targetId.c_str(), _TRUNCATE);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[PartyKick] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<PARTY_KICK_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[PartyKick] " << targetId << " 강퇴 완료\n";
    }
    else {
        std::cout << "[PartyKick] 실패. failCode: " << (int)res->failCode << '\n';
    }
}

// ====================== 파티장 위임 ======================

void TestClient::PartyDelegate() {
    std::string targetId;
    std::cout << "위임할 유저 ID: ";
    std::cin >> targetId;

    PARTY_DELEGATE_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::PARTY_DELEGATE_REQUEST;
    req.PacketLength = sizeof(req);
    strncpy_s(req.targetId, targetId.c_str(), _TRUNCATE);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[PartyDelegate] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<PARTY_DELEGATE_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[PartyDelegate] " << targetId << "에게 파티장 위임 완료\n";
    }
    else {
        std::cout << "[PartyDelegate] 실패. failCode: " << (int)res->failCode << '\n';
    }
}

// ====================== 매칭 시작 ======================

void TestClient::MatchStart() {
    MATCH_START_REQUEST req;
    req.PacketId = (uint16_t)PACKET_ID::MATCH_START_REQUEST;
    req.PacketLength = sizeof(req);

    send(lobbySkt, (char*)&req, sizeof(req), 0);
    auto packet = WaitResponse();
    if (packet.empty()) {
        std::cerr << "[MatchStart] 응답 타임아웃\n";
        return;
    }

    auto res = reinterpret_cast<MATCH_START_RESPONSE*>(packet.data());
    if (res->isSuccess) {
        std::cout << "[MatchStart] 매칭 시작!\n";
    }
    else {
        const char* reason;
        switch (res->failCode) {
        case 1: reason = "파티장만 가능"; break;
        default: reason = "서버 오류"; break;
        }
        std::cout << "[MatchStart] 실패: " << reason << '\n';
    }
}
