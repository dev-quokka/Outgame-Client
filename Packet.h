#pragma once
#include <cstdint>

constexpr uint16_t MAX_USER_ID_LEN = 21;
constexpr uint16_t MAX_USER_PASSWORD_LEN = 256;
constexpr uint16_t MAX_JWT_TOKEN_LEN = 257;
constexpr uint16_t MAX_INVENTORY_SIZE = 100;
constexpr uint16_t MAX_FRIEND_SIZE = 50;
constexpr uint16_t PACKET_SIZE = 1024;

#pragma pack(push, 1)

struct PACKET_HEADER {
    uint16_t PacketLength;
    uint16_t PacketId;
};

// ====================== 로그인 서버 ======================

struct UserInfo {
    char     userId[MAX_USER_ID_LEN + 1] = {};
    uint32_t exp = 0;
    uint16_t level = 1;
};

struct Currency {
    uint32_t bp = 0;
    uint32_t gcoin = 0;
};

struct Costume {
    uint32_t head = 0;
    uint32_t body = 0;
    uint32_t legs = 0;
    uint32_t feet = 0;
};

struct InventoryItem {
    uint32_t itemCode = 0;
    uint32_t quantity = 0;
    uint8_t  itemType = 0;
};

struct FriendInfo {
    char    friendId[MAX_USER_ID_LEN] = {};
    uint8_t friendStatus = 0;
    uint8_t onlineStatus = 0;
};

struct INVENTORY_RESPONSE : PACKET_HEADER {
    uint16_t itemCount = 0;
    // 이후 가변 데이터
};

struct FRIEND_LIST_RESPONSE : PACKET_HEADER {
    uint16_t friendCount = 0;
    // 이후 FriendInfo 배열
};

// 인벤토리 응답
struct USER_INVENTORY_PACKET : PACKET_HEADER {
    uint16_t    itemCount = 0;
    InventoryItem items[MAX_INVENTORY_SIZE];
};

// 로그인 요청
struct USER_LOGIN_REQUEST : PACKET_HEADER {
    char userPassword[MAX_USER_PASSWORD_LEN + 1] = {};
    char userId[MAX_USER_ID_LEN + 1] = {};
};

// 로그인 응답
struct USER_LOGIN_RESPONSE : PACKET_HEADER {
    char     token[257] = {};
    UserInfo userinfo;
    char     userId[MAX_USER_ID_LEN + 1] = {};
    Currency currency;
    Costume  costume;
    char     ip[16] = {};
    uint16_t port = 0;
    bool     isSuccess = false;
    uint8_t  failCode = 0;
};


// ====================== 로비 서버 접속 ======================

struct USER_LOBBY_CONNECT_REQUEST : PACKET_HEADER {
    char token[MAX_JWT_TOKEN_LEN] = {};
    char userId[MAX_USER_ID_LEN] = {};
};

struct USER_LOBBY_CONNECT_RESPONSE : PACKET_HEADER {
    bool isSuccess = false;
};

// ====================== 유저 검색 ======================

struct USER_SEARCH_REQUEST : PACKET_HEADER {
    char userId[MAX_USER_ID_LEN] = {};
};

struct USER_SEARCH_RESPONSE : PACKET_HEADER {
    char     userId[MAX_USER_ID_LEN] = {};
    uint16_t userLevel = 0;
    uint8_t  onlineStatus = 0;
    bool     isSuccess = false;
};

// ====================== 친구 요청 ======================

struct FRIEND_REQUEST_REQUEST : PACKET_HEADER {
    char targetId[MAX_USER_ID_LEN] = {};
};

struct FRIEND_REQUEST_RESPONSE : PACKET_HEADER {
    char    targetId[MAX_USER_ID_LEN] = {};
    bool    isSuccess = false;
    uint8_t failCode = 0;
};

struct FRIEND_REQUEST_NOTIFY : PACKET_HEADER {
    char     senderId[MAX_USER_ID_LEN] = {};
    uint32_t senderPk = 0;
    uint16_t senderLevel = 0;
    uint8_t  onlineStatus = 1;
};

// 친구 목록 응답
struct USER_FRIEND_PACKET : PACKET_HEADER {
    uint16_t   friendCount = 0;
    FriendInfo friends[MAX_FRIEND_SIZE];
};

// ====================== 친구 수락/거절 ======================

struct FRIEND_ACTION_REQUEST : PACKET_HEADER {
    char    targetId[MAX_USER_ID_LEN] = {};
    uint8_t action = 0;  // 0=수락, 1=거절/삭제/취소
};

struct FRIEND_ACTION_RESPONSE : PACKET_HEADER {
    char targetId[MAX_USER_ID_LEN] = {};
    bool isSuccess = false;
};

struct FRIEND_ACCEPT_NOTIFY : PACKET_HEADER {
    uint32_t friendPk = 0;
    uint8_t  accept = 0;  // 0=수락, 1=거절
};

struct FRIEND_STATUS_NOTIFY : PACKET_HEADER {
    uint32_t friendPk = 0;
    uint8_t  onlineStatus = 0;
};

// ====================== 코스튬 변경 ======================

struct COSTUME_CHANGE_REQUEST : PACKET_HEADER {
    uint32_t itemCode = 0;
    uint8_t  slot = 0;  // 1=head, 2=body, 3=legs, 4=feet
};

struct COSTUME_CHANGE_RESPONSE : PACKET_HEADER {
    uint32_t itemCode = 0;
    uint8_t  slot = 0;
    uint8_t  failCode = 0;
    bool     isSuccess = false;
};

struct COSTUME_CHANGE_NOTIFY : PACKET_HEADER {
    char     userId[MAX_USER_ID_LEN] = {};  
    uint32_t userPk = 0;
    uint32_t itemCode = 0;
    uint8_t  slot = 0;
};

// ====================== 파티 따라가기 ======================

struct PARTY_FOLLOW_REQUEST : PACKET_HEADER {
    char targetId[MAX_USER_ID_LEN] = {};
};

struct PARTY_FOLLOW_RESPONSE : PACKET_HEADER {
    uint32_t partyId = 0;
    bool     isSuccess = false;
    uint8_t  failCode = 0;
};

// ====================== 파티 초대 ======================

struct PARTY_INVITE_REQUEST : PACKET_HEADER {
    char targetId[MAX_USER_ID_LEN] = {};
};

struct PARTY_INVITE_RESPONSE : PACKET_HEADER {
    char    targetId[MAX_USER_ID_LEN] = {};
    bool    isSuccess = false;
    uint8_t failCode = 0;
};

struct PARTY_INVITE_NOTIFY : PACKET_HEADER {
    char     senderId[MAX_USER_ID_LEN] = {};
    uint16_t senderLevel = 0;
    uint32_t partyId = 0;
    uint8_t  memberCount = 0;
};

// ====================== 파티 초대 수락/거절 ======================

struct PARTY_INVITE_ACCEPT_REQUEST : PACKET_HEADER {
    char    senderId[MAX_USER_ID_LEN] = {};
    uint8_t accept = 0;  // 0=수락, 1=거절
};

struct PARTY_INVITE_ACCEPT_RESPONSE : PACKET_HEADER {
    uint32_t partyId = 0;
    bool     isSuccess = false;
    uint8_t  failCode = 0;
};

struct PARTY_INVITE_REJECT_NOTIFY : PACKET_HEADER {
    char senderId[MAX_USER_ID_LEN] = {};
};

// ====================== 파티 정보 ======================

// 새로 들어오는 파티 유저에게 기존 파티 정보 전달
struct PARTY_INFO_PACKET : PACKET_HEADER {
    uint32_t partyId = 0;
    uint32_t leaderPk = 0;
    uint8_t  memberCount = 0;

    struct PartyMember {
        char     userId[MAX_USER_ID_LEN] = {};
        uint32_t userPk = 0;
        uint16_t userLevel = 0;

        // 코스튬
        uint32_t head = 0;
        uint32_t body = 0;
        uint32_t legs = 0;
        uint32_t feet = 0;
    } members[4];
};

// ====================== 파티 입장/탈퇴/강퇴/위임 ======================

struct PARTY_JOIN_NOTIFY : PACKET_HEADER {
    char     userId[MAX_USER_ID_LEN] = {};
    uint32_t userPk = 0;
    uint32_t head = 0;
    uint32_t body = 0;
    uint32_t legs = 0;
    uint32_t feet = 0;
    uint16_t userLevel = 0; 
    uint8_t  memberCount = 0; 
};

struct PARTY_LEAVE_REQUEST : PACKET_HEADER {};

struct PARTY_LEAVE_RESPONSE : PACKET_HEADER {
    bool    isSuccess = false;
    uint8_t failCode = 0;
};

struct PARTY_LEAVE_NOTIFY : PACKET_HEADER {
    uint32_t userPk = 0;
    uint32_t newLeaderPk = 0;
};

struct PARTY_KICK_REQUEST : PACKET_HEADER {
    char targetId[MAX_USER_ID_LEN] = {};
};

struct PARTY_KICK_RESPONSE : PACKET_HEADER {
    char    targetId[MAX_USER_ID_LEN] = {};
    bool    isSuccess = false;
    uint8_t failCode = 0;
};

struct PARTY_KICK_NOTIFY : PACKET_HEADER {
    uint32_t userPk = 0;
};

struct PARTY_DELEGATE_REQUEST : PACKET_HEADER {
    char targetId[MAX_USER_ID_LEN] = {};
};

struct PARTY_DELEGATE_RESPONSE : PACKET_HEADER {
    char    targetId[MAX_USER_ID_LEN] = {};
    bool    isSuccess = false;
    uint8_t failCode = 0;
};

struct PARTY_DELEGATE_NOTIFY : PACKET_HEADER {
    uint32_t newLeaderPk = 0;
};

struct PARTY_MEMBER_STATUS_NOTIFY : PACKET_HEADER {
    uint32_t userPk = 0;
    uint32_t partyId = 0;
    uint8_t  onlineStatus = 0;
};

// ====================== 매칭 시작 ======================

struct MATCH_START_REQUEST : PACKET_HEADER {};

struct MATCH_START_RESPONSE : PACKET_HEADER {
    bool    isSuccess = false;
    uint8_t failCode = 0;
};

#pragma pack(pop)

// ====================== 패킷 ID ======================

enum class PACKET_ID : uint16_t {

    // ======================= LOGIN SERVER (1~ ) =======================
    USER_LOGIN_REQUEST = 1,
    USER_LOGIN_RESPONSE = 2,
    INVENTORY_RESPONSE = 3,
    FRIEND_LIST_RESPONSE = 4,


    // ======================= LOBBY SERVER (21~ ) =======================

    USER_LOBBY_CONNECT_REQUEST = 21,
    USER_LOBBY_CONNECT_RESPONSE = 22,


    USER_SEARCH_REQUEST = 25,
    USER_SEARCH_RESPONSE = 26,


    // ************* FRIEND *************

    FRIEND_REQUEST_REQUEST = 30,
    FRIEND_REQUEST_RESPONSE = 31,
    FRIEND_REQUEST_NOTIFY = 32,

    FRIEND_STATUS_NOTIFY = 33,
    FRIEND_ACCEPT_NOTIFY = 34,

    FRIEND_ACTION_REQUEST = 35,
    FRIEND_ACTION_RESPONSE = 36,


    // ************* COSTUME *************

    COSTUME_CHANGE_REQUEST = 51,
    COSTUME_CHANGE_RESPONSE = 52,

    COSTUME_CHANGE_NOTIFY = 55,


    // ************* PARTY *************

    PARTY_FOLLOW_REQUEST = 101,
    PARTY_FOLLOW_RESPONSE = 102,

    PARTY_INVITE_REQUEST = 105,
    PARTY_INVITE_RESPONSE = 106,

    PARTY_INVITE_NOTIFY = 110,

    PARTY_INVITE_ACCEPT_REQUEST = 111,
    PARTY_INVITE_ACCEPT_RESPONSE = 112,
    PARTY_INVITE_REJECT_NOTIFY = 113,

    PARTY_JOIN_NOTIFY = 114,
    PARTY_INFO_PACKET = 115,

    PARTY_LEAVE_REQUEST = 121,
    PARTY_LEAVE_RESPONSE = 122,

    PARTY_LEAVE_NOTIFY = 124,

    PARTY_KICK_REQUEST = 131,
    PARTY_KICK_RESPONSE = 132,
    PARTY_KICK_NOTIFY = 134,

    PARTY_DELEGATE_REQUEST = 141,
    PARTY_DELEGATE_RESPONSE = 142,
    PARTY_DELEGATE_NOTIFY = 143,

    PARTY_MEMBER_STATUS_NOTIFY = 145,


    MATCH_START_REQUEST = 201,
    MATCH_START_RESPONSE = 202,
};